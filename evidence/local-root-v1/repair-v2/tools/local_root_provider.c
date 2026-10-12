#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
static const char payload[]="OWNED-PROVIDER-73921 genuine ordinary provider content\n";
static FILE *logfile;
static void record(const char *op,const char *path){fprintf(logfile,"%s %s\n",op,path);fflush(logfile);}
static int attrs(const char *path,struct stat *st,struct fuse_file_info *fi){(void)fi;record("getattr",path);memset(st,0,sizeof(*st));st->st_uid=getuid();st->st_gid=getgid();st->st_nlink=1;st->st_blocks=8;
 if(!strcmp(path,"/")){st->st_mode=S_IFDIR|0755;st->st_nlink=2;return 0;}
 if(!strcmp(path,"/provider.txt")){st->st_mode=S_IFREG|0444;st->st_size=sizeof(payload)-1;return 0;}return -ENOENT;}
static int listing(const char *path,void *buffer,fuse_fill_dir_t filler,off_t offset,struct fuse_file_info *fi,enum fuse_readdir_flags flags){(void)offset;(void)fi;(void)flags;record("readdir",path);if(strcmp(path,"/"))return -ENOENT;filler(buffer,".",NULL,0,0);filler(buffer,"..",NULL,0,0);filler(buffer,"provider.txt",NULL,0,0);return 0;}
static int opening(const char *path,struct fuse_file_info *fi){fi->direct_io=1;record("open",path);return strcmp(path,"/provider.txt")?-ENOENT:0;}
static int reading(const char *path,char *buffer,size_t size,off_t offset,struct fuse_file_info *fi){(void)fi;record("read",path);if(strcmp(path,"/provider.txt"))return -ENOENT;size_t n=sizeof(payload)-1;if((size_t)offset>=n)return 0;if(size>n-offset)size=n-offset;memcpy(buffer,payload+offset,size);return (int)size;}
int main(int argc,char **argv){const char *log=getenv("LOCAL_ROOT_PROVIDER_LOG");if(!log)return 2;logfile=fopen(log,"a");if(!logfile)return 3;struct fuse_operations ops={.getattr=attrs,.readdir=listing,.open=opening,.read=reading};int rc=fuse_main(argc,argv,&ops,NULL);fclose(logfile);return rc;}
