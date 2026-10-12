#include "backend.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QFileInfo>
#include <QFile>
#include <cstdio>
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);
 if(argc!=6)return 2;
 Backend backend({argv[4],argv[1],argv[2],argv[3],argv[5],""});
 QElapsedTimer elapsed;elapsed.start();
 auto emitRow=[&](QJsonObject row){row["ms"]=elapsed.elapsed();auto bytes=QJsonDocument(row).toJson(QJsonDocument::Compact);puts(bytes.constData());fflush(stdout);};
 auto snapshot=[&](QString event){emitRow({{"event",event},{"connected",backend.connected()},{"model_ready_predicate",backend.modelReady()},{"model_setup_needed",backend.modelSetupNeeded()},{"message",backend.message()},{"busy",backend.busy()},{"indexed",backend.indexed()}});};
 QObject::connect(&backend,&Backend::changed,&app,[&]{snapshot("backend-changed");});
 QLocalSocket raw;QByteArray buffer;int seq=0;
 QObject::connect(&raw,&QLocalSocket::readyRead,&app,[&]{buffer+=raw.readAll();while(buffer.contains('\n')){int i=buffer.indexOf('\n');auto line=buffer.left(i);buffer.remove(0,i+1);emitRow({{"event","raw-service-reply"},{"raw",QJsonDocument::fromJson(line).object()}});}});
 QTimer poll;poll.setInterval(100);
 bool shutting=false,passed=false;int finishAt=0;
 QObject::connect(&poll,&QTimer::timeout,&app,[&]{
  if(raw.state()==QLocalSocket::UnconnectedState)raw.connectToServer(argv[4]);
  if(raw.state()==QLocalSocket::ConnectedState){auto request=QJsonObject{{"id",QString("observer-%1").arg(++seq)},{"op","status"}};emitRow({{"event","raw-service-request"},{"raw",request}});raw.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');}
  if(!shutting&&(backend.modelReady()||elapsed.elapsed()>8500)){passed=backend.modelReady();snapshot("terminal-observation");backend.shutdownOwnedService();shutting=true;finishAt=elapsed.elapsed()+1500;}
  if(shutting&&elapsed.elapsed()>finishAt){snapshot("post-shutdown");app.exit(passed?0:1);}
 });snapshot("initial");poll.start();return app.exec();
}
