#include "engine.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <linux/openat2.h>
#include <linux/magic.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
static_assert(sizeof(float)==4,"vector ABI requires IEEE float32");
namespace {
    [[noreturn]] void fail(QString message){
        throw std::runtime_error(message.toStdString());
    }
    QByteArray digest(QString path,qint64 expected=-1){
        QFile f(path);
        if(!f.open(QIODevice::ReadOnly))fail("cannot read "+path);
        if(expected>=0&&f.size()!=expected)fail("model byte size mismatch");
        QCryptographicHash h(QCryptographicHash::Sha256);
        if(!h.addData(&f))fail("hash read failed");
        return h.result().toHex();
    }
    bool beneath(QString path,QString root){
        return root=="/"?path.startsWith('/'):path==root||path.startsWith(root+"/");
    }
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
    struct CloseDirectory {void operator()(DIR *dir)const{::closedir(dir);}};
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

    QByteArray digestHeld(const LocalFd& held){
        auto fd=readableFd(held,false);QFile file;if(!file.open(fd.value,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle))fail("cannot hash held source");fd.release();
        QCryptographicHash hash(QCryptographicHash::Sha256);qint64 bytes=0;
        while(!file.atEnd()){auto chunk=file.read(64*1024);if(chunk.isEmpty()&&file.error()!=QFileDevice::NoError)fail("held source hash read failed");bytes+=chunk.size();if(bytes>32*1024*1024)fail("source hash limit exceeded");hash.addData(chunk);}
        return hash.result().toHex();
    }
    QString key(QString owner,QString id){
        return owner+"\n"+id;
    }
    QByteArray vectorBlob(QJsonObject response){
        if(!response.value("ok").toBool())fail(response.value("error").toString("embedding failure"));
        auto a=response.value("vector").toArray();
        if(a.size()!=768)fail("wrong vector dimension");
        QByteArray blob(768*int(sizeof(float)),Qt::Uninitialized);
        double norm=0;
        for(int i=0;i<768;i++){
            if(!a[i].isDouble())fail("invalid vector field");
            float v=float(a[i].toDouble());
            if(!std::isfinite(v))fail("nonfinite vector");
            norm+=double(v)*v;
            std::memcpy(blob.data()+i*sizeof(float),&v,sizeof(float));
        }
        if(std::abs(norm-1)>0.002)fail("unnormalized vector");
        return blob;
    }
    QString extract(QString program,QStringList args,int seconds=30){
        QProcess p;
        p.setProcessChannelMode(QProcess::SeparateChannels);
        p.start(program,args);
        if(!p.waitForStarted(5000))fail("extractor unavailable: "+program);
        QByteArray out;
        QElapsedTimer t;
        t.start();
        while(p.state()!=QProcess::NotRunning){
            p.waitForReadyRead(100);
            out+=p.readAllStandardOutput();
            p.readAllStandardError();
            if(out.size()>2*1024*1024||t.elapsed()>seconds*1000){
                p.kill();
                p.waitForFinished(5000);
                fail("extraction limit exceeded");
            }
        }
        out+=p.readAllStandardOutput();
        if(p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0)fail("extractor failed: "+program);
        if(out.size()>2*1024*1024)fail("extraction output too large");
        return QString::fromUtf8(out);
    }
}
Engine::Engine(EngineOptions options):opt(options){
    snapshot={
        {
            "ready",false
        }
        ,{
            "model_ready",false
        }
        ,{
            "busy",false
        }
        ,{
            "paused",false
        }
        ,{
            "roots",QJsonArray()
        }
        ,{
            "indexed",0
        }
        ,{
            "segments",0
        }
        ,{
            "errors",0
        }
        ,{
            "current",""
        }
        ,{
            "progress",0
        }
        ,{
            "total",0
        }
        ,{
            "error",""
        }
    }
    ;
    snapshot["index_revision"]=0; snapshot["identity"]=QJsonObject(); snapshot["failures"]=QJsonArray();
    snapshot["phase"]="verifying"; snapshot["recovery_attempt"]=0;
    thread=std::thread([this]{
        loop();
    }
    );
}
Engine::~Engine(){
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop=true;
        generation++;
    }
    wake.notify_all();
    if(thread.joinable())thread.join();
}
QStringList Engine::watchPaths(){
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return watched;
}
QJsonObject Engine::status(){
    std::lock_guard<std::mutex> lock(snapshotMutex);
    auto result=snapshot;
    result["paused"]=paused.load();
    if(paused)result["busy"]=false;
    return result;
}
void Engine::disconnect(QString owner){
    std::lock_guard<std::mutex> lock(mutex);
    if(activeOwner==owner)disconnected.insert(owner);for(const auto &task:queue)if(task.owner==owner)canceled.remove(key(owner,task.request.value("id").toString()));queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const Task &t){return t.owner==owner;}),queue.end());
}
void Engine::submit(QJsonObject r, QString owner, Reply reply) {
    std::lock_guard<std::mutex> lock(mutex);
    if (r.value("op").toString()=="cancel") {
        QString target=r.value("target").toString();
        bool owned=activeOwner==owner && activeId==target && !activeCommitted;
        for (const auto &task:queue) if(task.owner==owner && task.request.value("id").toString()==target) owned=true;
        if(owned) canceled.insert(key(owner,target));
        reply({{"accepted",owned}});
        return;
    }
    if(queue.size()>=256) { reply({{"ok",false},{"error","service queue full"}}); return; }
    // Ingress is intent only. Validation and cancellation precede committed state changes.
    queue.push_back({r,owner,reply,generation.load()});
    wake.notify_one();
}
bool Engine::validLocked(const Task &task) {
    return !stop && !canceled.contains(key(task.owner,task.request.value("id").toString()))
        && !disconnected.contains(task.owner)
        && (task.request.value("op").toString()!="search" || task.generation==generation);
}
bool Engine::valid(const Task &task) { std::lock_guard<std::mutex> lock(mutex); return validLocked(task); }
void Engine::commitControl(Task &task, const std::function<void()> &changes) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!validLocked(task)) fail("canceled");
    if(!db.transaction()) fail("control transaction failed");
    try { changes(); if(!db.commit()) fail("control commit failed"); }
    catch(...) { db.rollback(); throw; }
    activeCommitted=true;
    if(task.owner!="watcher") generation++;
    task.generation=generation.load();
    QString op=task.request.value("op").toString();
    if(op=="pause") paused=true;
    if(op=="resume") paused=false;
}
void Engine::servicePending() {
    for(int count=0;count<256 && !stop;count++) {
        Task task;
        { std::lock_guard<std::mutex> lock(mutex); if(queue.empty()) return;
          task=queue.front(); queue.pop_front(); activeOwner=task.owner;
          activeId=task.request.value("id").toString(); activeCommitted=false; }
        handle(task);
    }
}
QSqlQuery Engine::sql(QString statement,QVariantList values){
    QSqlQuery q(db);
    if(!q.prepare(statement))fail(q.lastError().text());
    for(auto &v:values)q.addBindValue(v);
    if(!q.exec())fail(q.lastError().text());
    return q;
}
QJsonArray Engine::roots(){
    QJsonArray result;
    auto q=sql("SELECT path,exclusions FROM roots ORDER BY path");
    while(q.next())result.append(QJsonObject{
        {
            "path",q.value(0).toString()
        }
        ,{
            "exclusions",QJsonDocument::fromJson(q.value(1).toByteArray()).array()
        }
    }
    );
    return result;
}
void Engine::refresh(){
    auto rr=roots();
    QJsonArray failures;
    auto f=sql("SELECT path,error,warning FROM files WHERE error<>'' OR warning<>'' LIMIT 20");
    while(f.next())failures.append(QJsonObject{
        {
            "path",f.value(0).toString()
        }
        ,{
            "error",f.value(1).toString()
        }
        ,{
            "warning",f.value(2).toString()
        }
    }
    );
    auto count=[this](QString s){
        auto q=sql(s);
        q.next();
        return q.value(0).toInt();
    }
    ;
    auto revisionQuery=sql("SELECT value FROM meta WHERE key='index_revision'");revisionQuery.next();
    QJsonObject update{
        {
            "index_revision",QJsonValue(revisionQuery.value(0).toLongLong())
        },{
            "roots",rr
        }
        ,{
            "indexed",count("SELECT count(*) FROM files WHERE error='' ")
        }
        ,{
            "errors",count("SELECT count(*) FROM files WHERE error<>''")
        }
        ,{
            "segments",count("SELECT count(*) FROM segments")
        }
        ,{
            "busy",scan.generation!=0&&!paused
        }
        ,{
            "paused",paused.load()
        }
        ,{
            "progress",scan.position
        }
        ,{
            "total",scan.files.size()
        }
        ,{
            "identity",identity
        }
        ,{
            "failures",failures
        }
    }
    ;
    std::lock_guard<std::mutex> lock(snapshotMutex);
    for(auto i=update.begin();i!=update.end();++i)snapshot[i.key()]=i.value();
}
void Engine::closeProducer(){
    if(!producer)return;
    if(producer->state()!=QProcess::NotRunning){
        producer->closeWriteChannel();
        if(!producer->waitForFinished(1500)){
            producer->terminate();
            if(!producer->waitForFinished(1500)){
                producer->kill();
                producer->waitForFinished(5000);
            }
        }
    }
    producer.reset();
    producerBuffer.clear();
}
QJsonObject Engine::embed(QJsonObject request){
    try {
    if(!producer){
            if(digest(opt.textModel,309855456)!="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135"||digest(opt.imageModel,554821024)!="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f")fail("model SHA256 mismatch");
            if(digest(opt.probe)!=opt.probeSha.toLatin1())fail("unverified producer binary: hash changed before reload");
        producer=std::make_unique<QProcess>();
        producer->setProcessChannelMode(QProcess::SeparateChannels);
        producer->start(opt.probe,{
            opt.textModel,opt.imageModel,QString::number(opt.threads)
        }
        );
        if(!producer->waitForStarted(5000)){
            closeProducer();
            fail("producer failed to start");
        }
    }
    QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    request["id"]=id;
    producer->write(QJsonDocument(request).toJson(QJsonDocument::Compact)+"\n");
    QElapsedTimer timer;
    timer.start();
    while(!producerBuffer.contains('\n')){
        producer->waitForReadyRead(100);
        producerBuffer+=producer->readAllStandardOutput();
        producer->readAllStandardError();
        if(stop||timer.elapsed()>90000||producerBuffer.size()>128*1024||producer->state()==QProcess::NotRunning){
            closeProducer();
            fail(stop?"stopping":"producer timeout or exit");
        }
    }
    int n=producerBuffer.indexOf('\n');
    auto line=producerBuffer.left(n);
    producerBuffer.remove(0,n+1);
    QJsonParseError error;
    auto doc=QJsonDocument::fromJson(line,&error);
    auto r=doc.object();
    if(error.error!=QJsonParseError::NoError||r.value("id").toString()!=id||!r.value("ok").isBool()){
        closeProducer();
        fail("producer protocol mismatch");
    }
    return r;
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); throw; }
}
QJsonObject Engine::extractorIdentity() {
    QJsonObject result;
    for(QString program:{"pdftotext","pdfinfo","pdftoppm","tesseract"}) {
        QString executable=QStandardPaths::findExecutable(program);
        if(executable.isEmpty()) { result[program]="unavailable"; continue; }
        QProcess process; process.setProcessChannelMode(QProcess::MergedChannels);
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("LC_ALL","C");
        process.setProcessEnvironment(environment);
        process.start(executable,{program=="tesseract"?"--version":"-v"});
        if(!process.waitForStarted(3000)||!process.waitForFinished(5000)) {
            process.kill(); process.waitForFinished(3000); fail("extractor version probe failed: "+program);
        }
        if(process.exitCode()!=0) fail("extractor version probe rejected: "+program);
        QString line=QString::fromUtf8(process.readAll()).split('\n').value(0).trimmed();
        if(line.isEmpty()||line.size()>1024) fail("invalid extractor version: "+program);
        result[program]=line;
    }
    return result;
}
void Engine::verifyExtractors() {
    if(extractorIdentity()!=identity.value("extractors").toObject()) {
        setFailure("extractor identity changed; explicit wipe/rebuild required");
        fail("extractor identity changed; explicit wipe/rebuild required");
    }
}
void Engine::closeDatabase() {
    if(db.isValid()) { QString name=db.connectionName(); db.close(); db=QSqlDatabase(); QSqlDatabase::removeDatabase(name); }
}
void Engine::openDatabase() {
    if(!db.isValid()) { db=QSqlDatabase::addDatabase("QSQLITE","index-worker-"+QUuid::createUuid().toString()); db.setDatabaseName(opt.data+"/index.sqlite"); }
    if(!db.isOpen()&&!db.open()) fail(db.lastError().text());
    sql("PRAGMA foreign_keys=ON"); sql("PRAGMA journal_mode=WAL"); sql("PRAGMA synchronous=FULL");
    QByteArray bytes=QJsonDocument(identity).toJson(QJsonDocument::Compact);
    auto tables=sql("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'");
    bool existing=tables.next();
    if(existing) {
        auto query=sql("SELECT value FROM meta WHERE key='identity'");
        if(!query.next()||query.value(0).toByteArray()!=bytes) fail("stored identity mismatch; explicit wipe/rebuild required");
    }
    sql("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT)");
    qint64 seed=0; QFile checkpoint(opt.data+"/index-revision"); if(checkpoint.open(QIODevice::ReadOnly)) seed=qMax(qint64(0),checkpoint.readAll().trimmed().toLongLong());
    sql("INSERT OR IGNORE INTO meta VALUES('index_revision',?)",{QString::number(seed)});
    sql("INSERT OR IGNORE INTO meta VALUES('identity',?)",{bytes});
    sql("CREATE TABLE IF NOT EXISTS roots(path TEXT PRIMARY KEY,exclusions TEXT NOT NULL)");
    sql("CREATE TABLE IF NOT EXISTS files(path TEXT PRIMARY KEY,root TEXT NOT NULL,hash TEXT NOT NULL,error TEXT NOT NULL,warning TEXT NOT NULL DEFAULT '')");
    sql("CREATE TABLE IF NOT EXISTS pages(id INTEGER PRIMARY KEY,path TEXT NOT NULL REFERENCES files(path) ON DELETE CASCADE,page INTEGER NOT NULL,title TEXT NOT NULL,text TEXT NOT NULL,kind TEXT NOT NULL)");
    sql("CREATE TABLE IF NOT EXISTS segments(id INTEGER PRIMARY KEY,path TEXT NOT NULL REFERENCES files(path) ON DELETE CASCADE,page_id INTEGER NOT NULL REFERENCES pages(id) ON DELETE CASCADE,title TEXT NOT NULL,text TEXT NOT NULL,page INTEGER NOT NULL,kind TEXT NOT NULL,vector BLOB NOT NULL)");
    sql("CREATE VIRTUAL TABLE IF NOT EXISTS terms USING fts5(text,title,content='pages',content_rowid='id')");
    sql("CREATE TRIGGER IF NOT EXISTS pages_ai AFTER INSERT ON pages BEGIN INSERT INTO terms(rowid,text,title) VALUES(new.id,new.text,new.title); UPDATE meta SET value=CAST(value AS INTEGER)+1 WHERE key='index_revision'; END");
    sql("CREATE TRIGGER IF NOT EXISTS pages_ad AFTER DELETE ON pages BEGIN INSERT INTO terms(terms,rowid,text,title) VALUES('delete',old.id,old.text,old.title); UPDATE meta SET value=CAST(value AS INTEGER)+1 WHERE key='index_revision'; END");
    auto p=sql("SELECT value FROM meta WHERE key='paused'"); paused=p.next()&&p.value(0).toString()=="1";
}
void Engine::setFailure(QString error) {
    closeProducer(); operational=false; scan={};
    { std::lock_guard<std::mutex> lock(mutex); generation++; }
    std::lock_guard<std::mutex> lock(snapshotMutex); watched.clear();
    snapshot["ready"]=false; snapshot["model_ready"]=false; snapshot["busy"]=false;
    snapshot["current"]=""; snapshot["error"]=error; snapshot["phase"]="failed";
}
void Engine::initialize() {
    operational=false; closeProducer();
    { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["ready"]=false; snapshot["model_ready"]=false;
      snapshot["error"]=""; snapshot["phase"]="verifying"; snapshot["recovery_attempt"]=snapshot.value("recovery_attempt").toInt()+1; }
    try {
        auto info=embed({{"op","info"}}); if(!info.value("ok").toBool()) fail("producer info failed");
        identity=info.value("info").toObject(); identity.remove("load_seconds");
        if(identity.value("dimension").toInt()!=768||identity.value("max_tokens").toInt()!=2048||identity.value("backend")!="CPU"||identity.value("pipeline")!="digup-vectors1-linux-cpu-probe1"||identity.value("model")!="google/embeddinggemma-2"||identity.value("model_revision")!="bfcd298762cc34d0357ece5ebdd31791a3a374d8"||identity.value("runtime")!="llama.cpp-b11461+no-logits") fail("producer identity mismatch");
        identity["text_sha256"]="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135";
        identity["image_sha256"]="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f";
        identity["producer_sha256"]=QString::fromLatin1(opt.probeSha.toLatin1());
        identity["engine_pipeline"]="kde-index-full-pages-v3-segments1800-image140-ocr32";
        identity["representation"]=3; identity["extractors"]=extractorIdentity();
        identity["dtype"]="float32"; identity["byte_order"]=QSysInfo::ByteOrder==QSysInfo::LittleEndian?"little":"big";
        openDatabase(); purgeInadmissibleRows(); operational=true;
        { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["ready"]=true; snapshot["model_ready"]=true; snapshot["error"]=""; snapshot["phase"]="ready"; }
        refresh();
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); throw; }
}
QJsonObject Engine::wipe(Task &task) {
    // Explicit removal is linearized before verification. A failed rebuild cannot restore or adopt old content.
    { std::lock_guard<std::mutex> lock(mutex); if(!validLocked(task)) fail("canceled"); activeCommitted=true; generation++; task.generation=generation.load(); }
    closeProducer(); operational=false; scan={};
    { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["ready"]=false; snapshot["model_ready"]=false; snapshot["busy"]=false; snapshot["phase"]="wiping"; }
    bool removed=false;
    qint64 revision=status().value("index_revision").toVariant().toLongLong();
    if(db.isOpen()) { QSqlQuery q(db); if(q.exec("SELECT value FROM meta WHERE key='index_revision'")&&q.next()) revision=qMax(revision,q.value(0).toLongLong()); }
    else if(QFileInfo::exists(opt.data+"/index.sqlite")) {
        QString name="wipe-revision-"+QUuid::createUuid().toString();
        { auto reader=QSqlDatabase::addDatabase("QSQLITE",name); reader.setConnectOptions("QSQLITE_OPEN_READONLY"); reader.setDatabaseName(opt.data+"/index.sqlite");
          if(reader.open()) { QSqlQuery q(reader); if(q.exec("SELECT value FROM meta WHERE key='index_revision'")&&q.next()) revision=qMax(revision,q.value(0).toLongLong()); } reader.close(); }
        QSqlDatabase::removeDatabase(name);
    }
    QFile oldCheckpoint(opt.data+"/index-revision"); if(oldCheckpoint.open(QIODevice::ReadOnly)) revision=qMax(revision,oldCheckpoint.readAll().trimmed().toLongLong());
    closeDatabase();
    try {
        if(revision==std::numeric_limits<qint64>::max()) fail("index revision overflow");
        QSaveFile checkpoint(opt.data+"/index-revision"); if(!checkpoint.open(QIODevice::WriteOnly)||checkpoint.write(QByteArray::number(revision+1)+"\n")<0||!checkpoint.commit()) fail("wipe revision checkpoint failed");
        for(QString suffix:{"","-wal","-shm"}) { QString file=opt.data+"/index.sqlite"+suffix; if(QFileInfo::exists(file)&&!QFile::remove(file)) fail("owned database removal failed"); }
        removed=true; paused=false;
        { std::lock_guard<std::mutex> lock(snapshotMutex); watched.clear(); snapshot["roots"]=QJsonArray(); snapshot["indexed"]=0; snapshot["segments"]=0; snapshot["errors"]=0; snapshot["failures"]=QJsonArray(); snapshot["index_revision"]=QJsonValue(revision+1); snapshot["current"]=""; }
        initialize();
        return {{"accepted",true},{"removed",true},{"ready",true},{"index_revision",status().value("index_revision")}};
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); return {{"accepted",true},{"removed",removed},{"ready",false},{"recovery_error",QString::fromUtf8(error.what())}}; }
}
void Engine::loop() {
    try {
        for(const auto &name:QDir(opt.data).entryList(QDir::Dirs|QDir::NoDotAndDotDot)) if(QRegularExpression("^extract-[A-Za-z0-9]{6}$").match(name).hasMatch()&&!QFileInfo(opt.data+"/"+name).isSymLink()) if(!QDir(opt.data+"/"+name).removeRecursively()) fail("cannot remove interrupted extraction snapshot");
        initialize(); if(!paused) startScan();
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); }
    while(!stop) {
        try {
            servicePending();
            if(operational && scan.generation && !paused) { scanStep(); continue; }
            std::unique_lock<std::mutex> lock(mutex);
            if(!wake.wait_for(lock,std::chrono::seconds(10),[this]{return stop||!queue.empty();})) { lock.unlock(); closeProducer(); }
        } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); }
    }
    closeProducer(); closeDatabase();
}
QString Engine::rootFor(QString path){
    auto rr=roots();
    for(auto v:rr){
        auto r=v.toObject();
        if(beneath(path,r.value("path").toString()))return r.value("path").toString();
    }
    return {
    }
    ;
}
void Engine::purgeInadmissibleRows(){
    auto mounts=localMounts();auto rr=roots();QStringList rejectedRoots,remove;QJsonArray warnings;
    for(auto value:rr){QString root=value.toObject().value("path").toString();
        auto reject=[&](QString error){rejectedRoots<<root;if(warnings.size()<20)warnings.append(QJsonObject{{"path",root},{"error",error}});};
        try {auto fd=localPath(root,mounts);struct stat st{};if(::fstat(fd.value,&st)!=0)fail("selected root metadata unavailable");if(!S_ISDIR(st.st_mode))denyLocal("selected root is not a directory");}
        catch(const LocalPolicyError& e){reject(QString::fromUtf8(e.what()));}
        catch(const LocalLookupError& e){if(e.code==ENOENT||e.code==ENOTDIR||e.code==ELOOP)reject(QString::fromUtf8(e.what()));else throw;}
    }
    auto rows=sql("SELECT path,root FROM files");while(rows.next()){
        QString path=rows.value(0).toString();bool reject=rejectedRoots.contains(rows.value(1).toString());
        if(!reject)try{admitLexical(mounts,path);}catch(const LocalPolicyError&){reject=true;}
        if(reject)remove<<path;
    }
    if(!remove.isEmpty()){if(!db.transaction())fail("root policy transaction failed");try{for(auto path:remove)sql("DELETE FROM files WHERE path=?",{path});if(!db.commit())fail("root policy commit failed");}catch(...){db.rollback();throw;}}
    std::lock_guard<std::mutex> lock(snapshotMutex);snapshot["scan_warnings"]=warnings;
}
bool Engine::allowed(QString path){
    if(!eligible(path))return false;
    try {auto fd=localPath(path);struct stat st{};return ::fstat(fd.value,&st)==0&&S_ISREG(st.st_mode);}catch(const std::exception&){return false;}
}
bool Engine::definitelyExcluded(QString path){
    // Exclusion commits need affirmative removal evidence. Query admission's
    // conservative false on temporary metadata failure is not such evidence.
    if(!eligible(path))return true;
    try {auto fd=localPath(path);struct stat st{};return ::fstat(fd.value,&st)==0&&!S_ISREG(st.st_mode);}
    catch(const LocalPolicyError&){return true;}
    catch(const LocalLookupError& e){return e.code==ENOENT||e.code==ENOTDIR||e.code==ELOOP;}
    catch(const std::exception&){return false;}
}
bool Engine::eligible(QString path){
    QString sourceName=path.mid(path.lastIndexOf('/')+1);int dot=sourceName.lastIndexOf('.');
    QString sourceSuffix=dot<0?QString():sourceName.mid(dot+1);
    if(beneath(path,QDir::cleanPath(opt.data)))return false;
    QString root=rootFor(path);
    if(root.isEmpty()||path==root)return false;
    QString rel=path.mid(root=="/"?1:root.size()+1);
    if(QSet<QString>{"credentials","secrets","passwords","id_rsa"}.contains((dot<0?sourceName:sourceName.left(dot)).toLower()))return false;
    static QSet<QString> deny={
        "node_modules","build","dist","target","vendor","cache","__pycache__","credentials","secrets","private_keys","venv","passwords","cmakefiles","_build"
    }
    ;
    for(auto part:rel.split('/'))if(part.startsWith('.')||part.toLower().startsWith("build-")||deny.contains(part.toLower()))return false;
    auto q=sql("SELECT exclusions FROM roots WHERE path=?",{
        root
    }
    );
    q.next();
    for(auto v:QJsonDocument::fromJson(q.value(0).toByteArray()).array())if(beneath(rel,v.toString()))return false;
    return QSet<QString>{
        "txt","md","pdf","jpg","jpeg","png"
    }
    .contains(sourceSuffix.toLower());
}
void Engine::startScan(Task *task){
    Scan next; next.generation=generation.load();
    QStringList nextWatches; QJsonArray warnings;
    quint64 entries=0,directories=0,pathBytes=0,skippedProviders=0,skippedNames=0;
    auto warning=[&](QString path,QString error){skippedProviders++;if(warnings.size()<20)warnings.append(QJsonObject{{"path",path},{"error",error}});};
    auto metric=[&](QString error={}){std::lock_guard<std::mutex> lock(snapshotMutex);snapshot["scan_entries"]=double(entries);snapshot["scan_directories"]=double(directories);snapshot["scan_path_bytes"]=double(pathBytes);snapshot["scan_error"]=error;snapshot["scan_warnings"]=warnings;snapshot["scan_skipped_providers"]=double(skippedProviders);snapshot["scan_skipped_names"]=double(skippedNames);};
    auto canceled=[&]{if(stop||(task&&!valid(*task)))fail("canceled");};
    auto retain=[&](QString path,int copies=1){quint64 bytes=quint64(path.size())*sizeof(QChar)*copies;if(bytes>opt.scanPathBytesLimit-pathBytes)fail("scan path storage limit exceeded");pathBytes+=bytes;};
    auto watch=[&](QString path){if(quint64(nextWatches.size())>=opt.scanWatchLimit)fail("scan watch coverage limit exceeded");retain(path);nextWatches<<path;};
    auto directory=[&](QString path){if(directories>=opt.scanDirectoryLimit)fail("scan directory limit exceeded");directories++;retain(path);};
    try {
        verifyExtractors();
        auto mounts=localMounts(); auto rr=roots(); if(rr.size()>64)fail("selected root limit exceeded");
        for(auto v:rr){
            canceled(); QString root=v.toObject().value("path").toString();
            try {admitLexical(mounts,root);} catch(const LocalPolicyError& e){warning(root,QString::fromUtf8(e.what()));continue;}
            QStringList dirs;directory(root);dirs<<root;
            while(!dirs.isEmpty()){
                canceled();QString dir=dirs.takeLast();auto held=localPath(dir,mounts);struct stat st{};
                if(::fstat(held.value,&st)!=0||!S_ISDIR(st.st_mode))fail("selected directory changed during scan");
                auto reader=readableFd(held,true);DIR *raw=::fdopendir(reader.value);if(!raw)fail("cannot enumerate held local directory");reader.release();
                std::unique_ptr<DIR,CloseDirectory> stream(raw);watch(dir);
                while(true){
                    canceled();errno=0;auto entry=::readdir(stream.get());if(!entry){if(errno)fail("local directory enumeration failed");break;}
                    QByteArray rawName(entry->d_name);if(rawName=="."||rawName=="..")continue;
                    if(entries>=opt.scanEntryLimit)fail("scan entry limit exceeded");entries++;
                    QString name=QString::fromUtf8(rawName);
                    if(name.toUtf8()!=rawName){skippedNames++;retain(dir);if(warnings.size()<20)warnings.append(QJsonObject{{"path",dir},{"entry_bytes_hex",QString::fromLatin1(rawName.toHex())},{"error","non-UTF8 filename skipped before lookup"}});if(entries%128==0)metric();continue;}
                    QString path=dir=="/"?"/"+name:dir+"/"+name;retain(path);
                    if(entries%128==0)metric();
                    if(name.startsWith('.'))continue;
                    QString rel=path.mid(root=="/"?1:root.size()+1);bool exclude=beneath(path,QDir::cleanPath(opt.data));
                    static const QSet<QString> deny={"node_modules","build","dist","target","vendor","cache","__pycache__","credentials","secrets","private_keys","venv","passwords","cmakefiles","_build"};
                    if(deny.contains(name.toLower())||name.toLower().startsWith("build-"))exclude=true;
                    for(auto e:v.toObject().value("exclusions").toArray())if(beneath(rel,e.toString()))exclude=true;
                    if(exclude)continue;
                    try {admitLexical(mounts,path);} catch(const LocalPolicyError& e){warning(path,QString::fromUtf8(e.what()));continue;}
                    if(entry->d_type==DT_LNK)continue;
                    LocalFd child;
                    try {child=localPath(path,mounts);} catch(const LocalLookupError& e){if(e.code==ELOOP||e.code==ENOENT||e.code==ENOTDIR)continue;throw;} // Links/disappearing entries are not followed.
                    struct stat childStat{};if(::fstat(child.value,&childStat)!=0)fail("cannot inspect held scan entry");
                    if(S_ISDIR(childStat.st_mode)){directory(path);dirs<<path;}
                    else if(S_ISREG(childStat.st_mode)&&eligible(path)){
                        if(next.files.size()>=10000)fail("scan file limit exceeded");retain(path,2);next.files.append(path);next.seen.insert(path);watch(path);
                    }
                }
            }
        }
        canceled();
        // Only a complete enumeration is evidence that unobserved old rows disappeared.
        auto q=sql("SELECT path FROM files");QStringList remove;
        while(q.next())if(!next.seen.contains(q.value(0).toString()))remove<<q.value(0).toString();
        auto removeRows=[&]{for(auto path:remove)sql("DELETE FROM files WHERE path=?",{path});};
        if(task)commitControl(*task,removeRows);else removeRows();
        std::sort(next.files.begin(),next.files.end());next.generation=generation.load();scan=std::move(next);
        {std::lock_guard<std::mutex> lock(snapshotMutex);watched=nextWatches;snapshot["current"]="";}
        metric();refresh();
    } catch(const std::exception& e){
        QString error=QString::fromUtf8(e.what());
        if(error!="canceled") {scan={};std::lock_guard<std::mutex> lock(snapshotMutex);watched.clear();snapshot["current"]="";}
        metric(error);refresh();throw;
    }
}
void Engine::scanStep(){
    if(scan.generation!=generation){
        scan={
        }
        ;
        refresh();
        return;
    }
    if(scan.position>=scan.files.size()){
        scan.generation=0;
        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            snapshot["current"]="";
        }
        refresh();
        return;
    }
    QString path=scan.files[scan.position++];
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        snapshot["current"]=path;
    }
    indexFile(path,scan.generation);
    if(operational) refresh();
}
void Engine::indexFile(QString path,quint64 gen){
    QString sourceHash("");
    try{
        if(!allowed(path))return;
        QString sourceName=path.mid(path.lastIndexOf('/')+1),sourceSuffix=sourceName.mid(sourceName.lastIndexOf('.')+1);
        auto held=localPath(path); struct stat sourceStat{};
        if(::fstat(held.value,&sourceStat)!=0||!S_ISREG(sourceStat.st_mode))fail("source is not a regular local file");
        if(sourceStat.st_size>32*1024*1024)fail("file exceeds 32 MiB");
        auto reader=readableFd(held,false);QFile source;
        if(!source.open(reader.value,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle))fail("cannot read held local source");reader.release();
        QTemporaryDir temp(opt.data+"/extract-XXXXXX");if(!temp.isValid())fail("temporary directory failed");
        QString copy=temp.path()+"/input."+sourceSuffix;QFile destination(copy);
        if(!destination.open(QIODevice::WriteOnly|QIODevice::NewOnly))fail("snapshot creation failed");
        QCryptographicHash hash(QCryptographicHash::Sha256);qint64 copied=0;
        while(!source.atEnd()){
            if(gen!=generation||paused||stop||!operational)return;
            QByteArray bytes=source.read(64*1024);if(bytes.isEmpty()&&source.error()!=QFileDevice::NoError)fail("source snapshot read failed");
            copied+=bytes.size();if(copied>32*1024*1024)fail("file exceeds 32 MiB while snapshotting");
            if(destination.write(bytes)!=bytes.size())fail("snapshot write failed");hash.addData(bytes);
        }
        destination.close();source.close();sourceHash=QString::fromLatin1(hash.result().toHex());
        auto old=sql("SELECT hash,error FROM files WHERE path=?",{path});
        if(old.next()&&old.value(0).toString()==sourceHash&&old.value(1).toString().isEmpty())return;
        if(!producer)embed({{"op","info"}});
        servicePending();if(gen!=generation||paused||stop||!operational)return;
        {std::lock_guard<std::mutex> lock(mutex);if(gen!=generation||paused||stop||!operational)return;sql("DELETE FROM files WHERE path=?",{path});}
        QString kind=sourceSuffix.toLower()=="pdf"?"pdf":QSet<QString>{
            "jpg","jpeg","png"
        }
        .contains(sourceSuffix.toLower())?"image":"text";
        QString text,warning("");
        QJsonArray pages;
        if(kind=="pdf"){
            text=extract("pdftotext",{
                "-layout","-enc","UTF-8",copy,"-"
            }
            );
            QString meta=extract("pdfinfo",{
                copy
            }
            );
            auto match=QRegularExpression(R"(Pages:\s+(\d+))").match(meta);
            if(!match.hasMatch())fail("cannot determine PDF page count");
            int count=match.captured(1).toInt();
            auto parts=text.split(QChar('\f'));
            for(int page=1;page<=count;page++){
                servicePending(); if(gen!=generation||paused||stop||!operational)return;
                QString part=page<=parts.size()?parts[page-1].trimmed():QString();
                if(part.isEmpty()){
                    if(page>32){
                        warning="OCR page limit: empty pages after32 were skipped";
                        continue;
                    }
                    try{
                        QString prefix=temp.path()+"/page";
                        extract("pdftoppm",{
                            "-f",QString::number(page),"-l",QString::number(page),"-singlefile","-scale-to","1600","-png",copy,prefix
                        }
                        ,30);
                        part=extract("tesseract",{
                            prefix+".png","stdout","--psm","3"
                        }
                        ,30).trimmed();
                        if(part.isEmpty())warning="PDF page "+QString::number(page)+": OCR produced no readable text";
                    }
                    catch(const std::exception &e){
                        warning="PDF page "+QString::number(page)+": "+QString::fromUtf8(e.what());
                    }
                }
                if(!part.isEmpty())pages.append(QJsonObject{
                    {
                        "page",page
                    }
                    ,{
                        "text",part
                    }
                }
                );
            }
            if(pages.isEmpty())fail(warning.isEmpty()?"PDF produced no readable text":warning);
        }
        else if(kind=="text"){
            QFile f(copy);
            if(!f.open(QIODevice::ReadOnly))fail("text read failed");
            auto bytes=f.readAll();
            if(bytes.contains('\0')||bytes.size()>2*1024*1024)fail("binary text or extraction limit");
            text=QString::fromUtf8(bytes);
            if(text.trimmed().isEmpty())fail("empty text");
            pages.append(QJsonObject{
                {
                    "page",0
                }
                ,{
                    "text",text
                }
            }
            );
        }
        else{
            try{
                text=extract("tesseract",{
                    copy,"stdout","--psm","3"
                }
                ,30).trimmed();
            }
            catch(const std::exception &e){
                text="";
                warning="Image OCR: "+QString::fromUtf8(e.what());
            }
            pages.append(QJsonObject{
                {
                    "page",0
                }
                ,{
                    "text",text
                }
            }
            );
        }
        struct Segment{
            QString text;
            int page;
            QByteArray vector;
        }
        ;
        QList<Segment> segments;
        for(auto v:pages){
            servicePending(); if(gen!=generation||paused||stop||!operational)return;
            auto page=v.toObject();
            QString source=page.value("text").toString();
            if(kind=="image"){
                segments.append({
                    source.left(1800),page.value("page").toInt(),vectorBlob(embed({
                        {
                            "op","image"
                        }
                        ,{
                            "path",copy
                        }
                        ,{
                            "budget",140
                        }
                    }
                    ))
                }
                );
            }
            for(int start=0;start<source.size();start+=1800){
                if(segments.size()>=256)fail("segment limit exceeded");
                servicePending(); if(gen!=generation||paused||stop||!operational)return;
                QString passage=source.mid(start,1800);
                segments.append({
                    passage,page.value("page").toInt(),vectorBlob(embed({
                        {
                            "op","document"
                        }
                        ,{
                            "title",sourceName
                        }
                        ,{
                            "text",passage
                        }
                    }
                    ))
                }
                );
            }
        }
        if(gen!=generation||paused||stop||!allowed(path))return;
        auto currentHeld=localPath(path);struct stat currentStat{};
        if(::fstat(currentHeld.value,&currentStat)!=0||currentStat.st_dev!=sourceStat.st_dev||currentStat.st_ino!=sourceStat.st_ino||!S_ISREG(currentStat.st_mode)||digestHeld(currentHeld)!=sourceHash.toLatin1())return;
        std::lock_guard<std::mutex> commitLock(mutex);
        if(gen!=generation||paused||stop||!operational)return;
        if(!db.transaction())fail("transaction failed");
        try{
            sql("INSERT INTO files(path,root,hash,error) VALUES(?,?,?,?)",{
                path,rootFor(path),sourceHash,QString("")
            }
            );
            sql("UPDATE files SET warning=? WHERE path=?",{
                warning,path
            }
            );
            QHash<int,qint64> pageIds;
            for(auto value:pages) { auto page=value.toObject(); auto q=sql("INSERT INTO pages(path,page,title,text,kind) VALUES(?,?,?,?,?)",{path,page.value("page").toInt(),sourceName,page.value("text").toString(),kind}); pageIds[page.value("page").toInt()]=q.lastInsertId().toLongLong(); }
            for(auto &segment:segments) sql("INSERT INTO segments(path,page_id,title,text,page,kind,vector) VALUES(?,?,?,?,?,?,?)",{path,pageIds[segment.page],sourceName,segment.text,segment.page,kind,segment.vector});
            if(gen!=generation||paused||stop){
                db.rollback();
                return;
            }
            if(!db.commit())fail("commit failed");
        }
        catch(...){
            db.rollback();
            throw;
        }
    }
    catch(const std::exception &e){
        if(operational&&gen==generation&&!paused&&!stop&&allowed(path)){
            std::lock_guard<std::mutex> lock(mutex);
            if(gen!=generation||paused||stop||!operational)return;
            sql("DELETE FROM files WHERE path=?",{
                path
            }
            );
            sql("INSERT INTO files(path,root,hash,error) VALUES(?,?,?,?)",{
                path,rootFor(path),sourceHash,QString::fromUtf8(e.what())
            }
            );
        }
    }
}
void Engine::handle(Task task) {
    QJsonObject result;
    try {
        QString op=task.request.value("op").toString();
        if(!valid(task)) fail("canceled");
        if(op=="wipe") result=wipe(task);
        else {
            if(!operational) {
                if(op!="scan"&&op!="resume") fail(status().value("error").toString());
                initialize();
                if(!valid(task)) fail("canceled");
            }
            if(op=="search") {
                QString text=task.request.value("text").toString().trimmed();
                if(text.isEmpty()||text.size()>2048) fail("query must contain 1..2048 characters");
                QString kind=task.request.value("kind").toString();
                if(!kind.isEmpty()&&!QSet<QString>{"text","pdf","image"}.contains(kind)) fail("unknown kind");
                int limit=task.request.value("limit").toInt(20); if(limit<1||limit>50) fail("limit must be 1..50");
                auto vector=vectorBlob(embed({{"op","query"},{"text",text}}));
                if(!valid(task)) fail("canceled");
                QSet<qint64> fts;
                QStringList tokens; for(QString token:text.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts)) { token.replace('"',"\"\""); tokens<<"\""+token+"\""; }
                auto hits=sql("SELECT rowid FROM terms WHERE terms MATCH ?",{tokens.join(" AND ")}); while(hits.next()) fts.insert(hits.value(0).toLongLong());
                QHash<QString,QJsonObject> best;
                auto better=[](const QJsonObject &a,const QJsonObject &b) {
                    if(a.value("exact").toBool()!=b.value("exact").toBool()) return a.value("exact").toBool();
                    if(a.value("term_match").toBool()!=b.value("term_match").toBool()) return a.value("term_match").toBool();
                    if(a.value("semantic_score").toDouble()!=b.value("semantic_score").toDouble()) return a.value("semantic_score").toDouble()>b.value("semantic_score").toDouble();
                    return a.value("path").toString()<b.value("path").toString();
                };
                auto rows=sql("SELECT s.id,s.path,s.title,s.text,s.page,s.kind,s.vector,f.warning,p.id,p.text FROM segments s JOIN files f ON f.path=s.path JOIN pages p ON p.id=s.page_id");
                while(rows.next()) {
                    QString path=rows.value(1).toString(); if(!kind.isEmpty()&&rows.value(5).toString()!=kind) continue;
                    if(!allowed(path)) continue;
                    QByteArray blob=rows.value(6).toByteArray(); if(blob.size()!=vector.size()) fail("corrupt vector size");
                    double dot=0; for(int i=0;i<768;i++) { float a,b; std::memcpy(&a,vector.constData()+i*sizeof(float),sizeof(float)); std::memcpy(&b,blob.constData()+i*sizeof(float),sizeof(float)); if(!std::isfinite(b)) fail("corrupt nonfinite vector"); dot+=double(a)*b; }
                    QString pageText=rows.value(9).toString(),title=rows.value(2).toString();
                    int position=pageText.indexOf(text,0,Qt::CaseInsensitive);
                    bool literal=position>=0||title.contains(text,Qt::CaseInsensitive),terms=fts.contains(rows.value(8).toLongLong());
                    QString passage=rows.value(3).toString();
                    if(position>=0) { int start=qMax(0,position-200); passage=pageText.mid(start,qMin(pageText.size()-start,text.size()+400)); }
                    double fraction=std::clamp((dot+1)/2,0.0,1.0);
                    double score=literal?1.0:terms?0.90+0.09*fraction:0.89*fraction;
                    QJsonObject row{{"path",path},{"title",title},{"text",passage},{"page",rows.value(4).toInt()},{"kind",rows.value(5).toString()},{"score",score},{"semantic_score",dot},{"exact",literal},{"term_match",terms},{"match_type",literal?"literal":terms?"terms":"semantic"},{"match_scope",position>=0?"text":literal?"title":terms?"page":"segment"},{"warning",rows.value(7).toString()}};
                    if(!best.contains(path)||better(row,best[path])) best[path]=row;
                }
                QList<QJsonObject> ranked=best.values(); std::sort(ranked.begin(),ranked.end(),better);
                QJsonArray results; for(int i=0;i<qMin(limit,int(ranked.size()));i++) results.append(ranked[i]);
                auto revision=sql("SELECT value FROM meta WHERE key='index_revision'"); revision.next();
                { std::lock_guard<std::mutex> lock(mutex); if(!validLocked(task)) fail("canceled"); activeCommitted=true; }
                result={{"results",results},{"identity",identity},{"generation",double(task.generation)},{"index_revision",QJsonValue(revision.value(0).toLongLong())}};
            } else if(op=="add_root") {
                QString requested=task.request.value("path").toString();
                if(requested.contains(QChar(char16_t(0))))fail("root path cannot contain NUL");
                if(!QDir::isAbsolutePath(requested))fail("root must be an absolute directory");
                QString path=QDir::cleanPath(requested);
                if(path!=requested)fail("root path must be absolute and clean; dot components are not accepted");
                auto held=localPath(path); struct stat rootStat{};
                if(::fstat(held.value,&rootStat)!=0||!S_ISDIR(rootStat.st_mode))fail("root must be an existing absolute non-symlink directory");
                if(roots().size()>=64)fail("selected root limit exceeded");
                if(beneath(path,QDir::cleanPath(opt.data))) fail("engine data directory cannot be selected as content");
                for(auto value:roots()) { QString root=value.toObject().value("path").toString(); if(beneath(path,root)||beneath(root,path)) fail("root already selected or overlaps selection"); }
                commitControl(task,[&]{sql("INSERT INTO roots VALUES(?,?)",{path,QString("[]")});}); result["accepted"]=true;
                try { startScan(); } catch(const std::exception &error) { result["scan_error"]=QString::fromUtf8(error.what()); }
            } else if(op=="remove_root"||op=="set_exclusions") {
                QString path=QDir::cleanPath(task.request.value("path").toString()); auto root=sql("SELECT path FROM roots WHERE path=?",{path}); if(!root.next()) fail("selected root not found");
                QJsonArray exclusions;
                if(op=="set_exclusions") {
                    if(!task.request.value("exclusions").isArray()) fail("exclusions must be an array");
                    for(auto value:task.request.value("exclusions").toArray()) { QString prefix=value.toString(); if(prefix.isEmpty()||QDir::isAbsolutePath(prefix)||prefix.split('/').contains("..")||prefix.contains('\\')) fail("invalid relative exclusion prefix"); prefix=QDir::cleanPath(prefix); if(prefix==".") fail("invalid exclusion prefix"); exclusions.append(prefix); }
                }
                commitControl(task,[&]{
                    if(op=="remove_root") { sql("DELETE FROM files WHERE root=?",{path}); sql("DELETE FROM roots WHERE path=?",{path}); }
                    else { sql("UPDATE roots SET exclusions=? WHERE path=?",{QJsonDocument(exclusions).toJson(QJsonDocument::Compact),path}); auto files=sql("SELECT path FROM files WHERE root=?",{path}); QStringList remove; while(files.next()) if(definitelyExcluded(files.value(0).toString())) remove<<files.value(0).toString(); for(QString file:remove) sql("DELETE FROM files WHERE path=?",{file}); }
                }); result["accepted"]=true;
                try { startScan(); } catch(const std::exception &error) { result["scan_error"]=QString::fromUtf8(error.what()); }
            } else if(op=="pause") {
                commitControl(task,[&]{sql("INSERT OR REPLACE INTO meta VALUES('paused','1')");}); scan={};
                { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["current"]=""; }
                result["accepted"]=true;
            } else if(op=="resume") {
                commitControl(task,[&]{sql("INSERT OR REPLACE INTO meta VALUES('paused','0')");}); result["accepted"]=true;
                try { startScan(); } catch(const std::exception &error) { result["scan_error"]=QString::fromUtf8(error.what()); }
            } else if(op=="scan") { startScan(&task); result["accepted"]=true; }
            else fail("unknown operation");
        }
        if(operational) refresh();
    } catch(const std::exception &error) { result={{"ok",false},{"error",QString::fromUtf8(error.what())}}; }
    task.reply(result);
    { std::lock_guard<std::mutex> lock(mutex); canceled.remove(key(task.owner,task.request.value("id").toString())); disconnected.remove(task.owner); activeOwner.clear(); activeId.clear(); activeCommitted=false; }
}
