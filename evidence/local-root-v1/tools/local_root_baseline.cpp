#include "engine.h"
#include <iostream>
#include <future>
#include <chrono>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(argc!=6)return 2;
    EngineOptions options{argv[1],argv[2],argv[3],argv[4],2,argv[5]};
    if(!QDir().mkpath(options.data))return 5;
    Engine engine(options);
    auto wait=[&](auto predicate) { for(int n=0;n<1500;n++) { auto s=engine.status(); if(predicate(s)) return s; QThread::msleep(20); } return engine.status(); };
    auto initial=wait([](auto s){return s.value("ready").toBool()||s.value("phase")=="failed";});
    if(!initial.value("ready").toBool()) {std::cout<<QJsonDocument(initial).toJson().constData();return 3;}
    QString root=QString::fromLocal8Bit(qgetenv("LOCAL_ROOT_FIXTURE"));
    std::promise<QJsonObject> pending;auto answer=pending.get_future(); QElapsedTimer timer;timer.start();
    engine.submit({{"id","fixture-add"},{"op","add_root"},{"path",root}},"owned-control",[&](auto r){pending.set_value(r);});
    if(answer.wait_for(std::chrono::seconds(40))!=std::future_status::ready)return 4;
    auto reply=answer.get();auto state=wait([](auto s){return !s.value("busy").toBool();});
    auto watches=engine.watchPaths();int directories=0,files=0;
    for(auto path:watches){if(QFileInfo(path).isDir())directories++;else files++;}
    QJsonObject result{{"reply",reply},{"state",state},{"watch_count",watches.size()},{"watch_directories",directories},{"watch_files",files},{"wall_ms",timer.elapsed()}};
    std::cout<<QJsonDocument(result).toJson().constData();
}
