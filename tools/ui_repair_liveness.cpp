#include "backend.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QLockFile>
#include <QLocalServer>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QCryptographicHash>
#include <functional>
#include <cstdio>
QJsonArray events;bool pass=true;
void record(QString name,QJsonObject x={}){x["event"]=name;events.append(x);}
bool wait(std::function<bool()> p,int ms=40000){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();if(p())return true;QThread::msleep(5);}return p();}
void check(QString name,bool value,QJsonObject x={}){x["check"]=value;record(name,x);pass&=value;}
QStringList processChildren(qint64 pid){QStringList result;QDir tasks(QString("/proc/%1/task").arg(pid));for(auto task:tasks.entryList(QDir::Dirs|QDir::NoDotAndDotDot)){QFile f(tasks.filePath(task+"/children"));if(f.open(QIODevice::ReadOnly))result+=QString::fromLatin1(f.readAll()).trimmed().split(' ',Qt::SkipEmptyParts);}return result;}
int main(int argc,char**argv){QApplication app(argc,argv);if(argc!=2)return 2;QString root=QFileInfo(argv[1]).absoluteFilePath(),own=root+"/local/ui-v2-liveness-final";QDir().mkpath(own+"/corpus");QFile f(own+"/corpus/source.txt");f.open(QIODevice::WriteOnly);f.write("REPLACEMENT-8249 original source content");f.close();
 QString socket=own+"/service.sock",data=own+"/data",service=root+"/evidence/engine-v2/candidate/bin/kde-index-service",probe=root+"/evidence/ui-v1/candidate/bin/embedding-probe",models=root+"/models";
 QFile binary(probe);binary.open(QIODevice::ReadOnly);QString sha=QString::fromLatin1(QCryptographicHash::hash(binary.readAll(),QCryptographicHash::Sha256).toHex());
 auto raw=[&](QString op){QLocalSocket c;c.connectToServer(socket);if(!c.waitForConnected(1000))return QJsonObject{};c.write(QJsonDocument(QJsonObject{{"id","liveness-control"},{"op",op}}).toJson(QJsonDocument::Compact)+'\n');c.waitForBytesWritten(1000);QByteArray buf;QElapsedTimer t;t.start();while(!buf.contains('\n')&&t.elapsed()<10000){c.waitForReadyRead(100);buf+=c.readAll();}auto result=QJsonDocument::fromJson(buf.trimmed()).object();record("actual-raw-reply",result);return result;};
 QProcess replacement;replacement.setProcessChannelMode(QProcess::MergedChannels);replacement.setStandardOutputFile(own+"/replacement-service.log");
 {
  Backend b({socket,service,probe,models,data,""});check("original-owned-ready",wait([&]{return b.modelReady();}));b.addRoot(QUrl::fromLocalFile(own+"/corpus"));check("owned-root-indexed",wait([&]{return b.indexed()==1&&!b.busy();}));b.setQuery("REPLACEMENT-8249");check("original-query-ready",wait([&]{return !b.results().isEmpty();}));
  QLockFile lock(data+"/service.lock");qint64 original=0;QString host,name;lock.getLockInfo(&original,&host,&name);record("original-owned-identity",{{"pid",double(original)}});
  check("actual-service-loss",raw("shutdown")["accepted"].toBool());check("client-observed-disconnect",wait([&]{return !b.connected();},3000));
  // Independent successor wins before the client's first retry; same exact engine/data.
  replacement.start(service,{"--data-dir",data,"--socket",socket,"--probe",probe,"--probe-sha256",sha,"--text-model",models+"/embeddinggemma-2-Q8_0.gguf","--image-model",models+"/mmproj-embeddinggemma-2-Q8_0.gguf"});check("replacement-started",replacement.waitForStarted(3000));
  check("original-client-reconnects-current-query",wait([&]{return b.connected()&&b.modelReady()&&!b.results().isEmpty();}));record("replacement-current-state",{{"pid",double(replacement.processId())},{"query",b.query()},{"results",QJsonArray::fromVariantList(b.results())}});
  QLockFile next(data+"/service.lock");qint64 actual=0;next.getLockInfo(&actual,&host,&name);check("no-duplicate-owned-engine",actual==replacement.processId(),{{"data_owner_pid",double(actual)},{"replacement_pid",double(replacement.processId())}});
  b.shutdownOwnedService();wait([]{return false;},1200);check("owned-hook-cannot-shutdown-replacement",replacement.state()!=QProcess::NotRunning&&b.connected()&&raw("status")["ok"].toBool());
 }
 check("ordinary-close-preserves-common-service",replacement.state()!=QProcess::NotRunning&&raw("status")["ok"].toBool());record("common-process-children-before-idle",{{"children",QJsonArray::fromStringList(processChildren(replacement.processId()))}});bool idle=wait([&]{return processChildren(replacement.processId()).isEmpty();},15000);check("common-heavy-worker-idle-exits",idle,{{"children_after",QJsonArray::fromStringList(processChildren(replacement.processId()))}});
 raw("shutdown");check("replacement-closed",replacement.waitForFinished(15000));check("socket-absent",!QFileInfo::exists(socket));
 // A live QLockFile owner with no available socket must not trigger a launch.
 QString held=own+"/held";QDir().mkpath(held);QLockFile heldLock(held+"/service.lock");check("live-data-lock-held",heldLock.tryLock());QString sentinel=own+"/unexpected-launch";QFile helper(own+"/launch-helper");helper.open(QIODevice::WriteOnly);helper.write("#!/bin/sh\ntouch '"+sentinel.toUtf8()+"'\n");helper.close();helper.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
 {Backend b({own+"/unavailable.sock",own+"/launch-helper",probe,models,held,""});check("bounded-reconnect-stops",wait([&]{return b.message().contains("Use Retry search");},10000));check("live-owner-no-launch",!QFileInfo::exists(sentinel));b.retryService();wait([]{return false;},1500);check("explicit-retry-no-launch-against-live-owner",!QFileInfo::exists(sentinel));}
 heldLock.unlock();record("completion",{{"pass",pass}});auto output=QJsonDocument(QJsonObject{{"pass",pass},{"events",events}}).toJson();fwrite(output.constData(),1,output.size(),stdout);return pass?0:1;}
