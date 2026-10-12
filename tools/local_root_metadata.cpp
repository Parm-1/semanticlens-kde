#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
int main(int argc,char **argv) {
    for(int i=1;i<argc;i++){
        struct statx st{};
        int rc=statx(AT_FDCWD,argv[i],AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT|AT_STATX_DONT_SYNC,STATX_BASIC_STATS|STATX_MNT_ID,&st);
        std::cout<<"{\"fixture\":\""<<argv[i]<<"\",\"rc\":"<<rc<<",\"mask\":"<<st.stx_mask<<",\"attributes\":"<<st.stx_attributes<<",\"attributes_mask\":"<<st.stx_attributes_mask<<",\"mode\":"<<st.stx_mode<<",\"size\":"<<st.stx_size<<",\"blocks\":"<<st.stx_blocks<<",\"mount_id\":"<<st.stx_mnt_id<<"}\n";
    }
}
