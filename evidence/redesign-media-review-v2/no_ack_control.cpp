#include "media_group.h"
#include <sys/wait.h>
#include <chrono>
#include <cstdio>
#include <cerrno>
int main() {
 int sockets[2]; if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,sockets)) return 2;
 auto start=std::chrono::steady_clock::now(); const auto child=fork();
 if(child==0) {close(sockets[0]);if(setpgid(0,0))_exit(125);_exit(sendOwnMediaGroup(sockets[1])?77:126);}
 close(sockets[1]);auto group=receiveMediaGroup(sockets[0]);int status=0;auto waited=waitpid(child,&status,0);
 auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
 errno=0;int signalResult=group?group->signal(0):99;int signalErrno=errno;close(sockets[0]);
 bool rejected=waited==child&&WIFEXITED(status)&&WEXITSTATUS(status)==126;
 bool pass=bool(group)&&rejected&&elapsed>=1900&&elapsed<3000&&signalResult==-1&&signalErrno==ESRCH;
 printf("{\"child_pid\":%d,\"descriptor_adopted\":%s,\"ack_sent\":false,\"child_exit\":%d,\"elapsed_ms\":%lld,\"held_group_signal\":%d,\"errno\":%d,\"pass\":%s}\n",child,group?"true":"false",WIFEXITED(status)?WEXITSTATUS(status):-1,(long long)elapsed,signalResult,signalErrno,pass?"true":"false");return pass?0:1;
}
