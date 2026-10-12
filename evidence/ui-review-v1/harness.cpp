#include "backend.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QLocalServer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QThread>
#include <QFile>
#include <QDir>
#include <cstdio>
#include <functional>
#include <signal.h>

QJsonArray events;
void record(QString type, QJsonObject details = {}) {details["event"] = type; events.append(details);}
bool waitFor(std::function<bool()> fn, int ms) {QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();if(fn())return true;QThread::msleep(5);}QCoreApplication::processEvents();return fn();}
void pause(int ms){waitFor([]{return false;},ms);}
int main(int argc,char**argv){
 QApplication app(argc,argv); if(argc!=3)return 2;
 QString mode=argv[1],root=QFileInfo(argv[2]).absoluteFilePath();
 QString frozen=root+"/evidence/ui-v1/candidate", own=root+"/local/ui-review-v1";
 AppPaths paths{own+"/"+mode+".sock",frozen+"/bin/kde-index-service",frozen+"/bin/embedding-probe",root+"/models",own+"/data-"+mode,own+"/download.py"};
 bool outcome=true;
 if(mode=="real"){
   {Backend b(paths);bool ready=waitFor([&]{return b.connected()&&b.modelReady();},40000);record("first-start",{{"ready",ready},{"message",b.message()}});outcome&=ready;
    b.shutdownOwnedService();bool off=waitFor([&]{return !b.connected();},5000);record("owned-service-shutdown",{{"disconnected",off}});
    pause(8500);record("after-retry-bound",{{"connected",b.connected()},{"message",b.message()},{"socket_exists",QFileInfo::exists(paths.socket)}});outcome&=off&&!b.connected();}
   {Backend b(paths);bool ready=waitFor([&]{return b.connected()&&b.modelReady();},40000);record("fresh-backend-recovery",{{"ready",ready},{"message",b.message()}});outcome&=ready;b.shutdownOwnedService();waitFor([&]{return !b.connected();},5000);}
 }else if(mode=="markup"){
   QTcpServer http;http.listen(QHostAddress::LocalHost,0);int fetches=0;
   QObject::connect(&http,&QTcpServer::newConnection,&app,[&]{auto socket=http.nextPendingConnection();QObject::connect(socket,&QTcpSocket::readyRead,&app,[&,socket]{auto req=socket->readAll();++fetches;record("http-resource-request",{{"raw",QString::fromLatin1(req)}});socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();});});
   QString corpus=own+"/markup-corpus";QDir().mkpath(corpus);QFile file(corpus+"/markup.txt");file.open(QIODevice::WriteOnly);file.write(QString("<b>MARKUP-CANARY</b><img src='http://127.0.0.1:%1/actual-service-canary.png'>").arg(http.serverPort()).toUtf8());file.close();
   {Backend b(paths);QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("backend",&b);engine.load(QUrl::fromLocalFile(frozen+"/src/ui/Main.qml"));
    bool ready=waitFor([&]{return b.connected()&&b.modelReady();},40000);record("real-service-ready",{{"ready",ready}});outcome&=ready;b.addRoot(QUrl::fromLocalFile(corpus));bool indexed=waitFor([&]{return b.indexed()==1&&!b.busy();},30000);record("actual-source-indexed",{{"indexed",indexed},{"roots",QJsonArray::fromVariantList(b.roots())},{"http_fetches",fetches}});outcome&=indexed;
    b.setQuery("MARKUP-CANARY");bool got=waitFor([&]{return !b.results().isEmpty();},15000);pause(1500);record("actual-search-rendered",{{"results",QJsonArray::fromVariantList(b.results())},{"http_fetches",fetches}});outcome&=got;
    auto w=qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));if(w)record("window-capture",{{"saved",w->grabWindow().save(root+"/evidence/ui-review-v1/actual-markup.png")}});
    b.shutdownOwnedService();waitFor([&]{return !b.connected();},5000);
   }http.close();
 }else if(mode=="dialog"){
   paths.models=own+"/no-models";
   {Backend b(paths);QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("backend",&b);engine.load(QUrl::fromLocalFile(frozen+"/src/ui/Main.qml"));auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));auto preview=window?window->findChild<QObject*>("resultPreview"):nullptr;
    if(preview){QVariantMap result{{"kind","pdf"},{"page",2},{"path",own+"/corpus/manual.pdf"},{"title","manual.pdf"},{"text","PDF-10299"}};preview->setProperty("selectedResult",result);QMetaObject::invokeMethod(preview,"open");bool rendered=waitFor([&]{return !b.previewUrl().isEmpty();},5000);QString file=b.previewUrl().toLocalFile();record("dialog-rendered",{{"rendered",rendered},{"path",file}});outcome&=rendered;QMetaObject::invokeMethod(preview,"close");record("immediately-after-close-invocation",{{"file_exists",QFileInfo::exists(file)},{"visible",preview->property("visible").toBool()},{"preview_url",b.previewUrl().toString()}});bool cleaned=waitFor([&]{return !preview->property("visible").toBool()&&b.previewUrl().isEmpty()&&!QFileInfo::exists(file);},2000);record("after-closed-phase",{{"cleaned",cleaned},{"file_exists",QFileInfo::exists(file)},{"visible",preview->property("visible").toBool()}});outcome&=cleaned;}else outcome=false;
   }
 }else if(mode=="peer"){
   QLocalServer server;server.listen(paths.socket);QLocalSocket*peer=nullptr;QByteArray buf;QList<QJsonObject> searches;int revision=2;
   auto reply=[&](QString id,QJsonObject fields){fields["id"]=id;fields["protocol"]=1;if(!fields.contains("ok"))fields["ok"]=true;record("reply",fields);peer->write(QJsonDocument(fields).toJson(QJsonDocument::Compact)+'\n');peer->flush();};
   QObject::connect(&server,&QLocalServer::newConnection,&app,[&]{peer=server.nextPendingConnection();QObject::connect(peer,&QLocalSocket::readyRead,&app,[&]{buf+=peer->readAll();while(buf.contains('\n')){auto i=buf.indexOf('\n');auto request=QJsonDocument::fromJson(buf.left(i)).object();buf.remove(0,i+1);record("request",request);QString id=request["id"].toString(),op=request["op"].toString();if(op=="status")reply(id,{{"status",QJsonObject{{"model_ready",true},{"busy",false},{"paused",false},{"index_revision",revision},{"roots",QJsonArray{}},{"failures",QJsonArray{}},{"indexed",1},{"errors",0},{"progress",0},{"total",0}}}});else if(op=="search")searches.append(request);else if(op=="cancel")reply(id,{{"accepted",true}});}});});
   QTcpServer http;http.listen(QHostAddress::LocalHost,0);int fetches=0;
   QObject::connect(&http,&QTcpServer::newConnection,&app,[&]{auto socket=http.nextPendingConnection();QObject::connect(socket,&QTcpSocket::readyRead,&app,[&,socket]{auto req=socket->readAll();++fetches;record("http-resource-request",{{"raw",QString::fromLatin1(req)}});socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();});});
   {
    Backend b(paths);QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("backend",&b);engine.load(QUrl::fromLocalFile(frozen+"/src/ui/Main.qml"));
    bool ready=waitFor([&]{return b.modelReady();},2000);record("source-shaped-status",{{"ready",ready}});outcome&=ready;
    b.setQuery("alpha");waitFor([&]{return searches.size()>=1;},1500);QString old=searches.value(0)["id"].toString();b.setQuery("beta");waitFor([&]{return searches.size()>=2;},1500);QString active=searches.value(1)["id"].toString();
    QJsonObject row{{"path",own+"/corpus/payment.txt"},{"title","payment.txt"},{"text","synthetic source passage"},{"page",0},{"kind","text"},{"score",1.0},{"semantic_score",.5},{"exact",true},{"warning",""}};
    reply(old,{{"results",QJsonArray{row}},{"index_revision",2},{"generation",1}});pause(100);record("old-query-success",{{"results_count",b.results().size()}});outcome&=b.results().isEmpty();
    reply(active,{{"results",QJsonArray{row}},{"index_revision",1},{"generation",1}});pause(100);record("older-revision-success",{{"results_count",b.results().size()}});outcome&=b.results().isEmpty();
    waitFor([&]{return searches.size()>=3;},1500);auto latest=searches.value(2)["id"].toString();reply(latest,{{"results",QJsonArray{row}},{"index_revision",2},{"generation",1}});waitFor([&]{return !b.results().isEmpty();},1000);record("current-query-success",{{"results_count",b.results().size()}});outcome&=b.results().size()==1;
    b.setQuery("markup");waitFor([&]{return searches.size()>=4;},1500);row["text"]=QString("<b>source excerpt</b><img src='http://127.0.0.1:%1/excerpt-canary.png'>").arg(http.serverPort());
    reply(searches.value(3)["id"].toString(),{{"results",QJsonArray{row}},{"index_revision",2},{"generation",1}});pause(2000);record("markup-observation",{{"results_count",b.results().size()},{"http_fetches",fetches}});
    auto w=qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));if(w)record("window-capture",{{"saved",w->grabWindow().save(root+"/evidence/ui-review-v1/markup.png")}});
    b.setQuery("");pause(100);record("empty-query",{{"results_count",b.results().size()}});outcome&=b.results().isEmpty();
    b.setQuery("reconnect");waitFor([&]{return searches.size()>=5;},1500);QString stale=searches.value(4)["id"].toString();peer->disconnectFromServer();waitFor([&]{return !b.connected();},1000);record("peer-disconnect",{{"ready",b.modelReady()},{"results_count",b.results().size()}});
    bool reconnected=waitFor([&]{return b.connected()&&b.modelReady()&&searches.size()>=6;},3000);record("external-service-reconnect",{{"resubmitted_query",reconnected},{"latest_text",searches.last()["text"]}});outcome&=reconnected;
    reply(stale,{{"ok",false},{"error","canceled"}});pause(100);record("old-canceled-error",{{"message",b.message()}});
   }
   if(peer){peer->abort();delete peer;peer=nullptr;}server.close();QLocalServer::removeServer(paths.socket);http.close();
 }else if(mode=="teardown"||mode=="pending"){
   bool pending=mode=="pending";
   if(pending){QDir().mkpath(own+"/renderer");QFile fake(own+"/renderer/pdftoppm");fake.open(QIODevice::WriteOnly);fake.write("#!/usr/bin/python3\nimport os,time\nopen(os.path.join(os.path.dirname(__file__),'preview.pid'),'w').write(str(os.getpid()))\ntime.sleep(60)\n");fake.close();fake.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);QFile::remove(own+"/renderer/preview.pid");qputenv("PATH",(own+"/renderer:").toUtf8()+qgetenv("PATH"));}
   QFile script(paths.downloader);script.open(QIODevice::WriteOnly);script.write("import os,time\nopen(os.path.join(os.path.dirname(__file__),'download.pid'),'w').write(str(os.getpid()))\ntime.sleep(60)\n");script.close();
   QString pidfile=own+"/download.pid";QFile::remove(pidfile);QString preview;
   {Backend b(paths);QObject::connect(&b,&Backend::previewChanged,&app,[&]{record("preview-observer",{{"url",b.previewUrl().toString()},{"message",b.previewMessage()}});});
    b.downloadModel();bool started=waitFor([&]{return QFileInfo::exists(pidfile);},2000);record("fake-download-start",{{"started",started},{"downloading",b.downloading()}});outcome&=started;
    b.previewResult({{"kind","pdf"},{"page",2},{"path",own+"/corpus/manual.pdf"}});bool rendered=waitFor([&]{return pending?QFileInfo::exists(own+"/renderer/preview.pid"):!b.previewUrl().isEmpty();},3000);preview=b.previewUrl().toLocalFile();record("real-preview-rendered",{{"rendered",rendered},{"path",preview}});outcome&=rendered;
   }
   QFile p(pidfile);p.open(QIODevice::ReadOnly);auto pid=p.readAll().toLongLong();record("after-destructor",{{"download_pid",double(pid)},{"download_alive",pid>0&&::kill(pid,0)==0},{"preview_exists",QFileInfo::exists(preview)}});outcome&=pid>0&&::kill(pid,0)!=0&&!QFileInfo::exists(preview);
   if(pending){QFile p(own+"/renderer/preview.pid");p.open(QIODevice::ReadOnly);auto pid=p.readAll().toLongLong();record("pending-preview-after-destructor",{{"pid",double(pid)},{"alive",pid>0&&::kill(pid,0)==0}});outcome&=pid>0&&::kill(pid,0)!=0;}
   // Constructor could have started an owned service during event processing; dispose only our socket's service.
   QLocalSocket cleanup;cleanup.connectToServer(paths.socket);if(cleanup.waitForConnected(1000)){cleanup.write("{\"id\":\"review-cleanup\",\"op\":\"shutdown\"}\n");cleanup.waitForBytesWritten(1000);cleanup.waitForDisconnected(3000);}pause(1000);
 }else return 2;
 record("completion",{{"harness_expectations",outcome},{"socket_exists",QFileInfo::exists(paths.socket)}});auto output=QJsonDocument(QJsonObject{{"mode",mode},{"events",events}}).toJson();fwrite(output.constData(),1,output.size(),stdout);return outcome?0:1;
}
