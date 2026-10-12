#include "engine.h"
#include <future>
#include <iostream>
#include <chrono>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);if(argc<6)return 2;
 EngineOptions options{argv[1],argv[2],argv[3],argv[4],2,argv[5]};
 options.scanEntryLimit=qEnvironmentVariableIntValue("LOCAL_ROOT_ENTRY_LIMIT");if(!options.scanEntryLimit)options.scanEntryLimit=100000;
 options.scanDirectoryLimit=qEnvironmentVariableIntValue("LOCAL_ROOT_DIRECTORY_LIMIT");if(!options.scanDirectoryLimit)options.scanDirectoryLimit=10000;
 options.scanWatchLimit=qEnvironmentVariableIntValue("LOCAL_ROOT_WATCH_LIMIT");if(!options.scanWatchLimit)options.scanWatchLimit=10000;
 options.scanPathBytesLimit=qEnvironmentVariableIntValue("LOCAL_ROOT_PATH_LIMIT");if(!options.scanPathBytesLimit)options.scanPathBytesLimit=16*1024*1024;
 QDir().mkpath(options.data);Engine engine(options);int id=0;
 auto wait=[&](auto test){for(int n=0;n<6000;n++){auto status=engine.status();if(test(status))return status;QThread::msleep(20);}throw std::runtime_error("control wait timeout");};
 try {
  auto initial=wait([](auto s){return s.value("ready").toBool()||s.value("phase")=="failed";});
  std::cout<<QJsonDocument(QJsonObject{{"event","initial"},{"status",initial}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  if(!initial.value("ready").toBool())return 3;
  QFile input;if(!input.open(stdin,QIODevice::ReadOnly))return 4;
  while(!input.atEnd()){
   QByteArray line=input.readLine();if(line.isEmpty())break;auto req=QJsonDocument::fromJson(line).object();QString op=req.value("op").toString();
   if(op=="wait"){auto status=wait([](auto s){return !s.value("busy").toBool();});std::cout<<QJsonDocument(QJsonObject{{"event","wait"},{"status",status},{"watches",engine.watchPaths().size()}}).toJson(QJsonDocument::Compact).constData()<<std::endl;continue;}
   if(op=="status"){std::cout<<QJsonDocument(QJsonObject{{"event","status"},{"status",engine.status()},{"watches",engine.watchPaths().size()}}).toJson(QJsonDocument::Compact).constData()<<std::endl;continue;}
   if(op=="exit")break;
   if(op=="cancel_scan"){
    QString target=QString::number(++id);std::promise<QJsonObject> answer,cancel;auto a=answer.get_future(),c=cancel.get_future();
    engine.submit({{"op","scan"},{"id",target}},"private-control",[&](auto r){answer.set_value(r);});
    engine.submit({{"op","cancel"},{"id",QString::number(++id)},{"target",target}},"private-control",[&](auto r){cancel.set_value(r);});
    if(a.wait_for(std::chrono::seconds(30))!=std::future_status::ready||c.wait_for(std::chrono::seconds(30))!=std::future_status::ready)throw std::runtime_error("cancel timeout");
    std::cout<<QJsonDocument(QJsonObject{{"event",op},{"reply",a.get()},{"cancel",c.get()},{"status",engine.status()},{"watches",engine.watchPaths().size()}}).toJson(QJsonDocument::Compact).constData()<<std::endl;continue;
   }
   struct rlimit previous{};QList<int> pressure;
   bool press=op=="pressure_scan"||op=="pressure_exclusions";
   if(press){getrlimit(RLIMIT_NOFILE,&previous);struct rlimit limit{64,previous.rlim_max};if(setrlimit(RLIMIT_NOFILE,&limit)!=0)throw std::runtime_error("cannot set own fd bound");for(int n=0;n<64;n++){int fd=::open("/dev/null",O_RDONLY|O_CLOEXEC);if(fd<0)break;pressure<<fd;}for(int n=0;n<2&&!pressure.isEmpty();n++)::close(pressure.takeLast());req["op"]=op=="pressure_scan"?"scan":"set_exclusions";}
   req["id"]=QString::number(++id);std::promise<QJsonObject> promise;auto future=promise.get_future();engine.submit(req,"private-control",[&](auto reply){promise.set_value(reply);});
   if(future.wait_for(std::chrono::seconds(120))!=std::future_status::ready)throw std::runtime_error("control response timeout");
   if(press){for(int fd:pressure)::close(fd);setrlimit(RLIMIT_NOFILE,&previous);}
   std::cout<<QJsonDocument(QJsonObject{{"event",op},{"reply",future.get()},{"status",engine.status()},{"watches",engine.watchPaths().size()}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  }
 } catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 5;}
}
