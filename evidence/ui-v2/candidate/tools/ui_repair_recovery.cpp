// Real frozen engine-v2 recovery through a byte-transparent local relay.
#include "backend.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocalServer>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QCryptographicHash>
#include <QQuickWindow>
#include <functional>
#include <cstdio>
QJsonArray events; bool success=true; std::function<QJsonObject()> observed;
void record(QString event,QJsonObject x={}) { x["event"]=event;events.append(x); }
bool waitFor(std::function<bool()> fn,int ms=40000){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();if(fn())return true;QThread::msleep(5);}return fn();}
void check(QString event,bool value,QJsonObject x={}){if(observed){auto now=observed();for(auto i=now.begin();i!=now.end();++i)x[i.key()]=i.value();}x["check"]=value;record(event,x);success&=value;}
void delay(int ms){waitFor([]{return false;},ms);}
QStringList children(qint64 pid){QStringList result;QDir tasks(QString("/proc/%1/task").arg(pid));for(auto task:tasks.entryList(QDir::Dirs|QDir::NoDotAndDotDot)){QFile f(tasks.filePath(task+"/children"));if(f.open(QIODevice::ReadOnly))result+=QString::fromLatin1(f.readAll()).split(' ',Qt::SkipEmptyParts);}return result;}
int main(int argc,char**argv){
 QApplication app(argc,argv);if(argc!=2)return 2;QString root=QFileInfo(argv[1]).absoluteFilePath(),own=root+"/local/ui-v2-recovery-final2";QDir().mkpath(own+"/models");QDir().mkpath(own+"/corpus");
 QString text=own+"/models/embeddinggemma-2-Q8_0.gguf",image=own+"/models/mmproj-embeddinggemma-2-Q8_0.gguf",wrong=own+"/wrong.gguf";QFile w(wrong);w.open(QIODevice::WriteOnly);w.write("incorrect synthetic weights");w.close();
 auto redirect=[&](QString target){QFile::remove(text);return QFile::link(target,text);};redirect(root+"/models/embeddinggemma-2-Q8_0.gguf");QFile::link(root+"/models/mmproj-embeddinggemma-2-Q8_0.gguf",image);
 QFile file(own+"/corpus/source.txt");file.open(QIODevice::WriteOnly);file.write("RECOVERY-7491 actual normal text content");file.close();
 QString service=root+"/evidence/engine-v2/candidate/bin/kde-index-service",probe=root+"/evidence/ui-v1/candidate/bin/embedding-probe",sock=own+"/engine.sock",front=own+"/client.sock",data=own+"/data";
 QFile exe(probe);exe.open(QIODevice::ReadOnly);QString sha=QString::fromLatin1(QCryptographicHash::hash(exe.readAll(),QCryptographicHash::Sha256).toHex());
 QProcess engineProcess;engineProcess.setProcessChannelMode(QProcess::MergedChannels);engineProcess.setStandardOutputFile(own+"/service.log",QIODevice::Append);
 auto start=[&]{engineProcess.start(service,{"--data-dir",data,"--socket",sock,"--probe",probe,"--probe-sha256",sha,"--text-model",text,"--image-model",image});return engineProcess.waitForStarted(3000)&&waitFor([&]{return QFileInfo::exists(sock);},10000);};
 auto request=[&](QString op){QLocalSocket c;c.connectToServer(sock);if(!c.waitForConnected(2000))return QJsonObject{};c.write(QJsonDocument(QJsonObject{{"id","raw-control"},{"op",op}}).toJson(QJsonDocument::Compact)+'\n');c.waitForBytesWritten(2000);QByteArray buf;QElapsedTimer t;t.start();while(!buf.contains('\n')&&t.elapsed()<45000){c.waitForReadyRead(100);buf+=c.readAll();}auto r=QJsonDocument::fromJson(buf.trimmed()).object();record("independent-actual-reply",r);return r;};
 auto stop=[&]{request("shutdown");engineProcess.waitForFinished(15000);};
 check("real-engine-start",start());
 QLocalServer relay;check("relay-listens",relay.listen(front));
 QObject::connect(&relay,&QLocalServer::newConnection,&app,[&]{while(auto client=relay.nextPendingConnection()){
  auto upstream=new QLocalSocket(client);auto input=new QByteArray;auto output=new QByteArray;
  QObject::connect(client,&QLocalSocket::readyRead,&app,[=]{auto bytes=client->readAll();*input+=bytes;while(input->contains('\n')){auto i=input->indexOf('\n');record("actual-client-request",QJsonDocument::fromJson(input->left(i)).object());input->remove(0,i+1);}upstream->write(bytes);});
  QObject::connect(upstream,&QLocalSocket::readyRead,&app,[=]{auto bytes=upstream->readAll();*output+=bytes;while(output->contains('\n')){auto i=output->indexOf('\n');record("actual-service-reply",QJsonDocument::fromJson(output->left(i)).object());output->remove(0,i+1);}client->write(bytes);});
  QObject::connect(upstream,&QLocalSocket::disconnected,client,&QLocalSocket::disconnectFromServer);
  QObject::connect(client,&QLocalSocket::disconnected,&app,[=]{upstream->abort();delete input;delete output;client->deleteLater();});
  upstream->connectToServer(sock);upstream->waitForConnected(2000);
 }});
 {
  Backend b({front,service,probe,own+"/models",data,root+"/tools/model_download.py"});QQmlApplicationEngine qml;qml.rootContext()->setContextProperty("backend",&b);qml.load(QUrl::fromLocalFile(root+"/src/ui/Main.qml"));
  observed=[&]{return QJsonObject{{"message",b.message()},{"connected",b.connected()},{"ready",b.modelReady()},{"results",QJsonArray::fromVariantList(b.results())},{"roots",QJsonArray::fromVariantList(b.roots())},{"children",QJsonArray::fromStringList(children(engineProcess.processId()))}};};
  check("backend-real-ready",waitFor([&]{return b.connected()&&b.modelReady();}));b.addRoot(QUrl::fromLocalFile(own+"/corpus"));check("root-real-indexed",waitFor([&]{return b.indexed()==1&&!b.busy();}));b.setQuery("RECOVERY-7491");check("normal-text-positive",waitFor([&]{return !b.results().isEmpty();}),{{"results",QJsonArray::fromVariantList(b.results())}});
  check("actual-heavy-worker-idle-exit",waitFor([&]{return children(engineProcess.processId()).isEmpty();},15000),{{"children",QJsonArray::fromStringList(children(engineProcess.processId()))}});
  redirect(wrong);b.setQuery("RECOVERY-7491 changed");check("real-reload-failure-visible",waitFor([&]{return !b.modelReady()&&b.message().contains("mismatch");}),{{"message",b.message()},{"connected",b.connected()},{"ready",b.modelReady()}});
  int before=events.size();b.retryService();check("invalid-compatible-retry-remains-closed",waitFor([&]{return events.size()>before+1&&b.message().contains("mismatch");}),{{"message",b.message()},{"ready",b.modelReady()}});
  redirect(root+"/models/embeddinggemma-2-Q8_0.gguf");b.retryService();check("explicit-compatible-retry-real-ready",waitFor([&]{return b.modelReady()&&!b.busy()&&!b.results().isEmpty();}),{{"message",b.message()},{"results",QJsonArray::fromVariantList(b.results())}});
  // Mutate only this disposable index, after the actual engine has stopped.
  stop();QProcess mutate;mutate.start("python3",{"-c","import sqlite3,json,sys\nc=sqlite3.connect(sys.argv[1]);v=json.loads(c.execute(\"select value from meta where key='identity'\").fetchone()[0]);v['representation']=999;c.execute(\"update meta set value=? where key='identity'\",(json.dumps(v),));c.commit()",data+"/index.sqlite"});mutate.waitForFinished(3000);check("synthetic-incompatible-index-prepared",mutate.exitCode()==0);
  check("incompatible-engine-start",start());check("incompatible-source-error-visible",waitFor([&]{return b.connected()&&!b.modelReady()&&b.message().contains("stored identity mismatch");}),{{"message",b.message()},{"roots",QJsonArray::fromVariantList(b.roots())}});
  b.retryService();check("incompatible-retry-rejected-with-source-error",waitFor([&]{return b.message().contains("stored identity mismatch");}));auto raw=request("status");
  QProcess inspect;inspect.start("python3",{"-c","import sqlite3,sys;print(sqlite3.connect(sys.argv[1]).execute('select count(*) from segments').fetchone()[0])",data+"/index.sqlite"});inspect.waitForFinished(3000);int retained=inspect.readAllStandardOutput().trimmed().toInt();check("incompatible-retry-preserves-owned-index",!b.modelReady()&&QFileInfo::exists(data+"/index.sqlite")&&retained>0,{{"retained_segments",retained}});
  {
   Backend verified({front,service,probe,root+"/models",data,root+"/tools/model_download.py"});check("verified-helper-client-connected-unready",waitFor([&]{return verified.connected()&&!verified.modelReady()&&verified.message().contains("stored identity mismatch");}));
   int requestsBefore=0;for(auto e:events)if(e.toObject()["event"]=="actual-client-request"&&e.toObject()["op"]=="scan")++requestsBefore;
   verified.downloadModel();check("existing-readonly-model-helper-finishes-and-emits-scan",waitFor([&]{int n=0;for(auto e:events)if(e.toObject()["event"]=="actual-client-request"&&e.toObject()["op"]=="scan")++n;return !verified.downloading()&&n>requestsBefore;},20000));
   check("helper-retry-shows-source-error-without-starting-search-claim",waitFor([&]{return verified.message().contains("stored identity mismatch");}),{{"helper_message",verified.message()}});
  }
  auto window=qml.rootObjects().value(0);auto dialog=window?window->findChild<QObject*>("wipeConfirmation"):nullptr;check("wipe-confirmation-found",dialog);
  if(dialog){QMetaObject::invokeMethod(dialog,"open");delay(300);QMetaObject::invokeMethod(dialog,"reject");delay(500);check("wipe-cancel-preserves-index",QFileInfo::exists(data+"/index.sqlite")&&!b.modelReady());
   redirect(wrong);QMetaObject::invokeMethod(dialog,"open");delay(300);QMetaObject::invokeMethod(dialog,"accept");check("explicit-wipe-removal-with-verification-failure",waitFor([&]{return !QFileInfo::exists(data+"/index.sqlite")&&b.message().contains("mismatch");}),{{"message",b.message()},{"ready",b.modelReady()}});
  }
  raw=request("status");check("failed-wipe-status-empty-and-closed",raw["status"].toObject()["roots"].toArray().isEmpty()&&!raw["status"].toObject()["ready"].toBool());
  redirect(root+"/models/embeddinggemma-2-Q8_0.gguf");b.retryService();check("retry-after-deliberate-removal-ready",waitFor([&]{return b.modelReady();}),{{"message",b.message()}});
  observed={};
 }
 check("ordinary-client-close-leaves-common-service",engineProcess.state()!=QProcess::NotRunning&&request("status")["ok"].toBool());
 stop();relay.close();QLocalServer::removeServer(front);delay(300);check("closure",engineProcess.state()==QProcess::NotRunning&&!QFileInfo::exists(sock)&&!QFileInfo::exists(front));
 auto out=QJsonDocument(QJsonObject{{"success",success},{"events",events}}).toJson();fwrite(out.constData(),1,out.size(),stdout);return success?0:1;
}
