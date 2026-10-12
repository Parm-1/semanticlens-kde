#include "backend.h"
#include <QCoreApplication>
#include <QLocalServer>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QThread>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <functional>
QJsonArray events;bool good=true;
void record(QString e,QJsonObject v={}){v["event"]=e;events.append(v);}
bool wait(std::function<bool()> f,int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();if(f())return true;QThread::msleep(5);}return f();}
QByteArray start(qint64 pid){QFile f(QString("/proc/%1/stat").arg(pid));if(!f.open(QIODevice::ReadOnly))return {};auto row=f.readAll();auto fields=row.mid(row.lastIndexOf(')')+2).simplified().split(' ');if(fields.size()<=19||fields[0]=="Z"||fields[0]=="X")return {};return fields[19];}
int main(int argc,char**argv){QCoreApplication app(argc,argv);if(argc!=2)return 2;QString root=QFileInfo(argv[1]).absoluteFilePath(),own=root+"/local/ui-review-v2-ownership";QDir().mkpath(own);
 QString pidfile=own+"/launcher.pid",helper=own+"/launcher.py";QFile::remove(pidfile);QFile script(helper);script.open(QIODevice::WriteOnly);script.write("#!/usr/bin/python3\nimport os,time\nopen('"+pidfile.toUtf8()+"','w').write(str(os.getpid()))\ntime.sleep(60)\n");script.close();script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
 QString socket=own+"/peer.sock";QLocalServer server;QLocalSocket*peer=nullptr;QByteArray buffer;int shutdowns=0;
 QObject::connect(&server,&QLocalServer::newConnection,&app,[&]{peer=server.nextPendingConnection();struct ucred credentials{};socklen_t size=sizeof(credentials);::getsockopt(int(peer->socketDescriptor()),SOL_SOCKET,SO_PEERCRED,&credentials,&size);record("connected-peer-credentials",{{"pid",double(credentials.pid)},{"uid",double(credentials.uid)},{"listener_pid",double(getpid())}});QObject::connect(peer,&QLocalSocket::readyRead,&app,[&]{buffer+=peer->readAll();while(buffer.contains('\n')){auto i=buffer.indexOf('\n');auto request=QJsonDocument::fromJson(buffer.left(i)).object();buffer.remove(0,i+1);record("raw-request",request);auto op=request["op"].toString();if(op=="shutdown")++shutdowns;QJsonObject reply{{"id",request["id"]},{"protocol",1},{"ok",true}};if(op=="status")reply["status"]=QJsonObject{{"ready",true},{"model_ready",true},{"busy",false},{"index_revision",1}};record("raw-reply",reply);peer->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');}});});
 qint64 pid=0;QByteArray ticks;
 {Backend b({socket,helper,root+"/evidence/ui-v2/candidate/bin/embedding-probe",root+"/models",own+"/data",""});bool launched=wait([&]{return QFileInfo::exists(pidfile);},5000);QFile p(pidfile);p.open(QIODevice::ReadOnly);pid=p.readAll().toLongLong();ticks=start(pid);record("owned-launcher-alive",{{"pid",double(pid)},{"start_ticks",QString::fromLatin1(ticks)},{"live",pid>0&&!ticks.isEmpty()},{"launched",launched}});good&=launched&&pid>0&&!ticks.isEmpty();bool listening=server.listen(socket);bool connected=wait([&]{return b.connected()&&b.modelReady();},3000);record("different-live-listener-connected",{{"listening",listening},{"connected",connected},{"launcher_live",start(pid)==ticks},{"launcher_pid",double(pid)},{"listener_pid",double(getpid())}});good&=listening&&connected&&start(pid)==ticks&&pid!=getpid();b.shutdownOwnedService();wait([]{return false;},400);record("wrong-peer-hook",{{"shutdown_requests",shutdowns},{"client_connected",b.connected()},{"launcher_live",start(pid)==ticks}});good&=shutdowns==0&&b.connected()&&start(pid)==ticks;}
 record("ordinary-client-destruction",{{"launcher_live",start(pid)==ticks}});good&=start(pid)==ticks;
 if(pid>0&&!ticks.isEmpty()&&start(pid)==ticks)::kill(pid,SIGTERM);bool exited=wait([&]{return start(pid).isEmpty();},3000);record("owned-launcher-closed",{{"pid",double(pid)},{"closed",exited}});good&=exited;
 if(peer){peer->abort();delete peer;}server.close();QLocalServer::removeServer(socket);
 QString countfile=own+"/attempts.txt",failure=own+"/fail-launch.py";QFile::remove(countfile);QFile failed(failure);failed.open(QIODevice::WriteOnly);failed.write("#!/usr/bin/python3\nimport os\nopen('"+countfile.toUtf8()+"','a').write(str(os.getpid())+'\\n')\n");failed.close();failed.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
 auto attempts=[&]{QFile f(countfile);f.open(QIODevice::ReadOnly);return QString::fromLatin1(f.readAll()).split('\n',Qt::SkipEmptyParts);};
 {Backend b({own+"/absent.sock",failure,root+"/evidence/ui-v2/candidate/bin/embedding-probe",root+"/models",own+"/failed-data",""});bool bounded=wait([&]{return b.message().contains("Use Retry search");},12000);auto first=attempts();record("automatic-episode-bound",{{"bounded",bounded},{"message",b.message()},{"launch_pids",QJsonArray::fromStringList(first)}});good&=bounded&&first.size()==2;wait([]{return false;},1200);record("bound-stays-stopped",{{"launch_count",attempts().size()}});good&=attempts().size()==2;b.retryService();bool again=wait([&]{return b.message().contains("Use Retry search")&&attempts().size()==4;},12000);record("explicit-retry-new-bounded-episode",{{"bounded",again},{"message",b.message()},{"launch_pids",QJsonArray::fromStringList(attempts())}});good&=again;}
 for(auto s:attempts())record("failed-launch-closed",{{"pid",s},{"closed",start(s.toLongLong()).isEmpty()}});
 record("completion",{{"expectations",good},{"peer_socket_exists",QFileInfo::exists(socket)},{"absent_socket_exists",QFileInfo::exists(own+"/absent.sock")}});auto out=QJsonDocument(QJsonObject{{"events",events}}).toJson();fwrite(out.constData(),1,out.size(),stdout);return good?0:1;
}
