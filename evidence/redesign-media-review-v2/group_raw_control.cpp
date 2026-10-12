#include "media_group.h"
#include <sys/wait.h>
#include <sys/prctl.h>
#include <cerrno>
#include <cstdio>
int main() {
 prctl(PR_SET_CHILD_SUBREAPER,1);int s[2];if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,s))return 2;
 const auto leader=fork();if(leader==0){close(s[0]);if(setsid()<0||!sendOwnMediaGroup(s[1]))_exit(126);auto child=fork();if(child==0){close(s[1]);for(;;)pause();}send(s[1],&child,sizeof(child),MSG_NOSIGNAL);_exit(0);}
 close(s[1]);auto group=receiveMediaGroup(s[0]);bool ack=acknowledgeMediaGroup(s[0],bool(group));pid_t child=0;auto childBytes=recv(s[0],&child,sizeof(child),0);close(s[0]);int leaderStatus=0;auto leaderWait=waitpid(leader,&leaderStatus,0);
 errno=0;auto afterLeader=group?group->signal(0):-99;auto afterLeaderErrno=errno;
 errno=0;auto terminate=group?group->signal(SIGKILL):-99;auto terminateErrno=errno;int childStatus=0;auto childWait=waitpid(child,&childStatus,0);
 auto sentinel=fork();if(sentinel==0){setsid();for(;;)pause();}
 errno=0;auto empty=group?group->signal(SIGKILL):-99;auto emptyErrno=errno;auto alive=kill(sentinel,0);kill(sentinel,SIGKILL);int sentinelStatus=0;auto sentinelWait=waitpid(sentinel,&sentinelStatus,0);
 printf("{\"group_fd\":%d,\"ack_sent\":%s,\"descendant_bytes\":%lld,\"leader_pid\":%d,\"leader_wait\":%d,\"leader_wait_status\":%d,\"post_leader_reap_signal0\":%d,\"post_leader_reap_errno\":%d,\"group_kill\":%d,\"group_kill_errno\":%d,\"descendant_pid\":%d,\"descendant_wait\":%d,\"descendant_wait_status\":%d,\"empty_group_kill\":%d,\"empty_group_errno\":%d,\"sentinel_pid\":%d,\"sentinel_alive_result\":%d,\"sentinel_wait\":%d,\"sentinel_wait_status\":%d}\n",group?group->fd:-1,ack?"true":"false",(long long)childBytes,leader,leaderWait,leaderStatus,afterLeader,afterLeaderErrno,terminate,terminateErrno,child,childWait,childStatus,empty,emptyErrno,sentinel,alive,sentinelWait,sentinelStatus);
 return bool(group)&&ack&&childBytes==sizeof(child)&&leaderWait==leader&&WIFEXITED(leaderStatus)&&WEXITSTATUS(leaderStatus)==0&&afterLeader==0&&terminate==0&&childWait==child&&WIFSIGNALED(childStatus)&&WTERMSIG(childStatus)==SIGKILL&&empty==-1&&emptyErrno==ESRCH&&alive==0&&sentinelWait==sentinel?0:1;
}
