#include "media_group.h"
// Media snapshots use the engine-v5 Linux storage admission contract.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QImageReader>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QSet>
#include <QSize>
#include <linux/openat2.h>
#include <linux/magic.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdexcept>
#include <cstring>
namespace {
[[noreturn]] void fail(QString message) { throw std::runtime_error(message.toStdString()); }
    struct LocalPolicyError:std::runtime_error {explicit LocalPolicyError(QString message):std::runtime_error(message.toStdString()){}};
    [[noreturn]] void denyLocal(QString message){throw LocalPolicyError(message);}
    struct LocalLookupError:std::runtime_error {int code;explicit LocalLookupError(int error):std::runtime_error("local path resolution rejected: "+std::string(::strerror(error))),code(error){}};
    struct LocalFd {
        int value=-1;
        explicit LocalFd(int v=-1):value(v){}
        ~LocalFd(){if(value>=0)::close(value);}
        LocalFd(const LocalFd&)=delete;
        LocalFd& operator=(const LocalFd&)=delete;
        LocalFd(LocalFd&& v) noexcept:value(v.value){v.value=-1;}
        LocalFd& operator=(LocalFd&& v) noexcept {if(value>=0)::close(value);value=v.value;v.value=-1;return *this;}
        int release(){int v=value;value=-1;return v;}
    };
    struct LocalMount { quint64 id; QString path,type; };
    using LocalMounts=QList<LocalMount>;
    QByteArray mountUnescape(QByteArray text){
        text.replace("\\040"," "); text.replace("\\011","\t"); text.replace("\\012","\n"); text.replace("\\134","\\"); return text;
    }
    quint64 localMagic(QString type){
        if(type=="ext2"||type=="ext3"||type=="ext4")return EXT4_SUPER_MAGIC;
        if(type=="btrfs")return BTRFS_SUPER_MAGIC;
        if(type=="xfs")return XFS_SUPER_MAGIC;
        if(type=="f2fs")return F2FS_SUPER_MAGIC;
        if(type=="ntfs3")return 0x7366746e; // Linux ntfs3 super.c, not the FUSE driver.
        if(type=="exfat")return EXFAT_SUPER_MAGIC;
        if(type=="vfat"||type=="msdos")return MSDOS_SUPER_MAGIC;
        if(type=="tmpfs")return TMPFS_MAGIC;
        if(type=="iso9660")return ISOFS_SUPER_MAGIC;
        if(type=="udf")return UDF_SUPER_MAGIC;
        return 0;
    }
    bool atOrBeneath(QString path,QString root){return root=="/"?path.startsWith('/'):path==root||path.startsWith(root+"/");}
    LocalMounts localMounts(){
        QFile file("/proc/self/mountinfo"); if(!file.open(QIODevice::ReadOnly))fail("local storage mount information unavailable");
        QByteArray bytes=file.read(1024*1024+1); if(bytes.size()>1024*1024)fail("mount information limit exceeded");
        LocalMounts result;
        for(auto line:bytes.split('\n')){
            if(line.isEmpty())continue; auto parts=line.split(' '); int split=parts.indexOf("-");
            bool ok=false; quint64 id=parts.value(0).toULongLong(&ok);
            if(!ok||parts.size()<10||split<6||split+3>=parts.size())fail("invalid local mount information");
            QByteArray rawPoint=mountUnescape(parts[4]);QString point=QString::fromUtf8(rawPoint);
            if(point.toUtf8()!=rawPoint)continue; // JSON/Qt paths cannot name these bytes without aliasing.
            if(!point.startsWith('/'))fail("invalid local mount point");
            result.append({id,point,QString::fromUtf8(parts[split+1])});
        }
        if(result.isEmpty())fail("empty local mount information"); return result;
    }
    const LocalMount& mountFor(const LocalMounts& mounts,QString path){
        const LocalMount *found=nullptr;
        for(const auto& m:mounts)if(atOrBeneath(path,m.path)&&(!found||m.path.size()>=found->path.size()))found=&m;
        if(!found)fail("selected path has no mount information"); return *found;
    }
    void admitLexical(const LocalMounts& mounts,QString path){
        if(path.contains(QChar(char16_t(0)))||QString::fromUtf8(path.toUtf8())!=path||!QDir::isAbsolutePath(path)||path!=QDir::cleanPath(path)||path.toUtf8().size()>4096)denyLocal("local path must be an absolute clean path within4096bytes");
        for(const auto& m:mounts)if(atOrBeneath(path,m.path)&&!localMagic(m.type))denyLocal("unsupported storage provider: "+m.type+" at "+m.path);
        if(!localMagic(mountFor(mounts,path).type))denyLocal("unsupported storage provider");
    }
    void validateMountFd(int fd,const LocalMount& mount){
        struct statx sx{}; struct statfs sf{};
        if(::statx(fd,"",AT_EMPTY_PATH,STATX_MNT_ID,&sx)!=0||!(sx.stx_mask&STATX_MNT_ID)||sx.stx_mnt_id!=mount.id||::fstatfs(fd,&sf)!=0||quint64(sf.f_type)!=localMagic(mount.type))fail("selected storage mount changed or cannot be verified");
    }
    LocalFd localPath(QString path,const LocalMounts& mounts){
        // Admission precedes selected-path lookup. NO_XDEV protects each approved
        // segment; only a lexically approved mountpoint can start another segment.
        admitLexical(mounts,path);
        LocalFd current(::open("/",O_PATH|O_DIRECTORY|O_CLOEXEC)); if(current.value<0)fail("cannot open storage root");
        validateMountFd(current.value,mountFor(mounts,"/"));
        QString prefix;
        for(auto part:path.split('/',Qt::SkipEmptyParts)){
            prefix+="/"+part; const auto& expected=mountFor(mounts,prefix);
            bool crossing=expected.path==prefix;
            struct open_how how{}; how.flags=O_PATH|O_CLOEXEC;
            how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|(crossing?0:RESOLVE_NO_XDEV);
            QByteArray name=part.toUtf8(); int fd=int(::syscall(SYS_openat2,current.value,name.constData(),&how,sizeof(how)));
            if(fd<0)throw LocalLookupError(errno);
            current=LocalFd(fd); validateMountFd(current.value,expected);
        }
        return current;
    }
    LocalFd localPath(QString path){auto mounts=localMounts();return localPath(path,mounts);}
    LocalFd readableFd(const LocalFd& held,bool directory){
        // Reopen the already held inode, never its externally mutable source path.
        QByteArray path=QByteArray("/proc/self/fd/")+QByteArray::number(held.value);
        int fd=::open(path.constData(),O_RDONLY|O_CLOEXEC|O_NONBLOCK|(directory?O_DIRECTORY:0));
        if(fd<0)fail("cannot read held local source descriptor");
        return LocalFd(fd);
    }


void render(const QJsonObject &request, const QString &directory, const MediaGroup &group) {
    const auto path=request.value("path").toString(), kind=request.value("kind").toString();
    const auto root=request.value("root").toString();
    const auto mounts=localMounts();
    admitLexical(mounts,root); admitLexical(mounts,path);
    if(root==path||!atOrBeneath(path,root))fail("file outside selected root");
    const auto relative=path.mid(root=="/"?1:root.size()+1);
    static const QSet<QString> deny={"node_modules","build","dist","target","vendor","cache","__pycache__","credentials","secrets","private_keys","venv","passwords","cmakefiles","_build"};
    for(const auto &part:relative.split('/'))if(part.startsWith('.')||part.toLower().startsWith("build-")||deny.contains(part.toLower()))fail("excluded source");
    for(const auto &value:request.value("exclusions").toArray())if(atOrBeneath(relative,value.toString()))fail("excluded source");
    const auto name=path.mid(path.lastIndexOf('/')+1);const auto stem=name.left(name.lastIndexOf('.')).toLower();
    if(QSet<QString>{"credentials","secrets","passwords","id_rsa"}.contains(stem))fail("excluded source");
    const auto suffix=name.mid(name.lastIndexOf('.')+1).toLower();
    if((kind=="image"&&!QSet<QString>{"png","jpg","jpeg"}.contains(suffix))||(kind=="pdf"&&suffix!="pdf")||(kind!="image"&&kind!="pdf"))fail("unsupported media");
    const int pixels=request.value("pixels").toInt();const int page=request.value("page").toInt();
    if((pixels!=320&&pixels!=1600)||(kind=="pdf"&&(page<1||page>100000)))fail("invalid rendering bounds");
    auto held=localPath(path,mounts);struct stat before{};
    if(::fstat(held.value,&before)!=0||!S_ISREG(before.st_mode)||before.st_size<0||before.st_size>32*1024*1024)fail("not a regular local source within32MiB");
    auto reader=readableFd(held,false);QFile source;
    if(!source.open(reader.value,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle))fail("source read failed");reader.release();
    const auto snapshot=directory+"/input."+suffix;QFile copy(snapshot);
    if(!copy.open(QIODevice::WriteOnly|QIODevice::NewOnly))fail("snapshot creation failed");
    qint64 copied=0;
    while(!source.atEnd()) {const auto bytes=source.read(64*1024);if(bytes.isEmpty()&&source.error()!=QFileDevice::NoError)fail("source read failed");copied+=bytes.size();if(copied>32*1024*1024||copy.write(bytes)!=bytes.size())fail("snapshot size/write failed");}
    struct stat after{};
    if(::fstat(source.handle(),&after)!=0||before.st_size!=after.st_size||before.st_mtim.tv_sec!=after.st_mtim.tv_sec||before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec||before.st_ctim.tv_sec!=after.st_ctim.tv_sec||before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec||copied!=before.st_size)fail("source changed while snapshotting");
    source.close();copy.close();
    if(!copy.setPermissions(QFileDevice::ReadOwner))fail("snapshot permissions failed");
    const auto output=directory+"/render.png";
    if(kind=="pdf") {
        const auto renderer=QStandardPaths::findExecutable("pdftoppm");if(renderer.isEmpty())fail("pdftoppm unavailable");
        QProcess process;
        // A killed helper must not leave its PDF renderer behind.
        const pid_t helper=::getpid();process.setChildProcessModifier([helper] {::prctl(PR_SET_PDEATHSIG,SIGKILL);if(::getppid()!=helper)::_exit(126);});
        process.setStandardOutputFile(QProcess::nullDevice());process.setStandardErrorFile(QProcess::nullDevice());
        process.start(renderer,{"-f",QString::number(page),"-l",QString::number(page),"-singlefile","-scale-to",QString::number(pixels),"-png",snapshot,directory+"/render"});
        if(!process.waitForStarted(1000))fail("PDF render unavailable");
        if(!process.waitForFinished(25000)) {
            // Kill the complete helper group, including grandchildren spawned by
            // a stalled renderer. The backend reports this as an unavailable preview.
            group.signal(SIGKILL);::_exit(124);
        }
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)fail("PDF render failed");
    } else {
        QImageReader image(snapshot);image.setDecideFormatFromContent(true);image.setAutoTransform(true);
        const auto format=image.format();if(format!="png"&&format!="jpeg")fail("unsupported image payload");
        const auto size=image.size();if(!size.isValid()||qint64(size.width())*size.height()>64*1024*1024)fail("image dimensions exceeded");
        // JPEG supports scaled decoding. Decode PNG within the allocation limit;
        // the smooth scaled-read path stalled in the local Qt6.12 controls.
        if(format=="jpeg"&&(size.width()>pixels||size.height()>pixels))image.setScaledSize(size.scaled(pixels,pixels,Qt::KeepAspectRatio));
        auto raster=image.read();if(raster.isNull())fail("image decode failed");
        if(raster.width()>pixels||raster.height()>pixels)raster=raster.scaled(pixels,pixels,Qt::KeepAspectRatio,Qt::FastTransformation);
        if(!raster.save(output,"PNG"))fail("image output failed");
    }
    QFile::remove(snapshot);
    QImageReader check(output);const auto size=check.size();
    if(!size.isValid()||size.width()>pixels||size.height()>pixels||QFileInfo(output).size()>16*1024*1024)fail("render output exceeded bounds");
}
}
int main(int argc,char **argv) {
    if(::getpgrp()!=::getpid()&&::setpgid(0,0)!=0)return 1;
    MediaGroup group(int(::syscall(SYS_pidfd_open, ::getpid(), 0)));
    if (group.fd < 0 || group.signal(0) != 0) return 126;
    // Decoder and renderer resource limits are inherited by the PDF child.
    struct rlimit memory {512*1024*1024,512*1024*1024}, cpu {30,30}, file {32*1024*1024,32*1024*1024};
    if(::setrlimit(RLIMIT_AS,&memory)||::setrlimit(RLIMIT_CPU,&cpu)||::setrlimit(RLIMIT_FSIZE,&file))return 1;
    QCoreApplication app(argc,argv);QImageReader::setAllocationLimit(64);
    try {if(app.arguments().size()!=2)fail("invalid arguments");QFile input;if(!input.open(STDIN_FILENO,QIODevice::ReadOnly,QFileDevice::DontCloseHandle))fail("request unavailable");
        const auto bytes=input.read(65537);if(bytes.size()>65536)fail("request too large");QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
        if(error.error!=QJsonParseError::NoError||!document.isObject())fail("invalid request");render(document.object(),app.arguments()[1],group);return 0;
    }catch(const std::exception &e) {QFile output;if(output.open(STDERR_FILENO,QIODevice::WriteOnly,QFileDevice::DontCloseHandle))output.write(e.what());return 1;}
}
