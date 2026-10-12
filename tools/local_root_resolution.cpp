#include <linux/openat2.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
void control(const char *name,int dir,const char *path,unsigned long resolve) {
    open_how how{};how.flags=O_PATH|O_CLOEXEC;how.resolve=resolve;
    errno=0;int fd=syscall(SYS_openat2,dir,path,&how,sizeof(how));int error=errno;
    struct statx st{};int rc=-1;
    if(fd>=0){rc=statx(fd,"",AT_EMPTY_PATH,STATX_BASIC_STATS|STATX_MNT_ID,&st);close(fd);}
    std::cout<<"{\"name\":\""<<name<<"\",\"fd_success\":"<<(fd>=0?"true":"false")<<",\"errno\":"<<error<<",\"statx_rc\":"<<rc<<",\"mount_id\":"<<st.stx_mnt_id<<"}\n";
}
int main(int argc,char **argv){
    if(argc!=2)return 2;int owned=open(argv[1],O_PATH|O_DIRECTORY|O_CLOEXEC);if(owned<0)return 3;
    auto resolve=RESOLVE_BENEATH|RESOLVE_NO_XDEV|RESOLVE_NO_SYMLINKS;
    control("ordinary",owned,"ordinary.txt",resolve);control("owned-symlink",owned,"symlink.txt",resolve);control("parent-escape",owned,"../",resolve);close(owned);
    int slash=open("/",O_PATH|O_DIRECTORY|O_CLOEXEC);if(slash<0)return 4;
    control("existing-proc-mount-crossing",slash,"proc",resolve);close(slash);
}
