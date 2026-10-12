#include "engine.h"
#include <QtNetwork>
#include <unistd.h>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    QCoreApplication::setApplicationName("kde-index-service");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({
        {
            "data-dir","Private engine data directory","directory"
        }
        ,{
            "socket","Local socket name","name","kde-index-"+QString::number(getuid())
        }
        ,{
            "probe","Verified embedding probe","file"
        }
        ,{
            "text-model","Pinned text model","file"
        }
        ,{
            "image-model","Pinned image model","file"
        }
        ,{
            "threads","CPU threads 1..16","count","6"
        }
        ,{
            "probe-sha256","Expected accepted/build-derived producer SHA256","digest","f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf"
        }
    }
    );
    parser.process(app);
    EngineOptions options{
        parser.value("data-dir"),parser.value("probe"),parser.value("text-model"),parser.value("image-model"),parser.value("threads").toInt()
    }
    ;
    options.probeSha=parser.value("probe-sha256");
    if(!QRegularExpression("^[a-f0-9]{64}$").match(options.probeSha).hasMatch()){
        qCritical("invalid producer SHA256");
        return 2;
    }
    for(auto p:{
        options.data,options.probe,options.textModel,options.imageModel
    }
    )if(p.isEmpty()||!QDir::isAbsolutePath(p)){
        qCritical("all paths must be explicit and absolute");
        return 2;
    }
    if(options.threads<1||options.threads>16){
        qCritical("threads must be 1..16");
        return 2;
    }
    if(QFileInfo(options.data).isSymLink()||!QDir().mkpath(options.data)||!QFile::setPermissions(options.data,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner)){
        qCritical("cannot prepare private data directory");
        return 2;
    }
    QLockFile lock(options.data+"/service.lock");
    lock.setStaleLockTime(0);
    if(!lock.tryLock(0)){
        qCritical("data directory already owned by a service");
        return 2;
    }
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    QString socketName=parser.value("socket");
    if(!server.listen(socketName)){
        QLocalSocket existing;
        existing.connectToServer(socketName);
        if(existing.waitForConnected(300)){
            qCritical("socket already owned by a running service");
            return 2;
        }
        QLocalServer::removeServer(socketName);
        if(!server.listen(socketName)){
            qCritical()<<server.errorString();
            return 2;
        }
    }
    Engine engine(options);
    bool shuttingDown=false;
    QFileSystemWatcher watcher;
    QTimer debounce,sync;
    debounce.setSingleShot(true);
    debounce.setInterval(350);
    sync.setInterval(500);
    bool rescanPending=false;
    QString watcherError;
    auto schedule=[&]{
        rescanPending=true;
        debounce.start();
    }
    ;
    QObject::connect(&watcher,&QFileSystemWatcher::directoryChanged,&app,schedule);
    QObject::connect(&watcher,&QFileSystemWatcher::fileChanged,&app,schedule);
    QObject::connect(&debounce,&QTimer::timeout,&app,[&]{
        auto s=engine.status();
        if(rescanPending&&s.value("ready").toBool()&&!s.value("busy").toBool()&&!s.value("paused").toBool()){
            rescanPending=false;
            engine.submit({
                {
                    "id","watch-reconcile"
                }
                ,{
                    "op","scan"
                }
            }
            ,"watcher",[](QJsonObject){
            }
            );
        }
    }
    );
    QObject::connect(&sync,&QTimer::timeout,&app,[&]{
        auto paths=engine.watchPaths();
        QSet<QString> desired(paths.begin(),paths.end());
        auto existing=watcher.files()+watcher.directories();
        QSet<QString> present(existing.begin(),existing.end());
        QStringList remove,add;
        for(auto p:present)if(!desired.contains(p))remove<<p;
        for(auto p:desired)if(!present.contains(p))add<<p;
        if(!remove.isEmpty())watcher.removePaths(remove);
        if(!add.isEmpty()){
            auto failed=watcher.addPaths(add);
            watcherError=failed.isEmpty()?QString():"native watch capacity/path failure; explicit scan remains available";
        }
        if(rescanPending&&!engine.status().value("busy").toBool())debounce.start();
    }
    );
    sync.start();
    QObject::connect(&server,&QLocalServer::newConnection,&app,[&]{
        while(auto socket=server.nextPendingConnection()){
            auto buffer=std::make_shared<QByteArray>();
            auto pending=std::make_shared<QSet<QString>>();
            auto canceledIds=std::make_shared<QSet<QString>>();
            QString owner=QUuid::createUuid().toString();
            QObject::connect(socket,&QLocalSocket::disconnected,&app,[&,socket,owner]{
                engine.disconnect(owner);
                socket->deleteLater();
            }
            );
            QObject::connect(socket,&QLocalSocket::readyRead,&app,[&,socket,buffer,pending,canceledIds,owner]{
                *buffer+=socket->readAll();
                if(buffer->size()>65536){
                    socket->disconnectFromServer();
                    return;
                }
                while(buffer->contains('\n')){
                    int n=buffer->indexOf('\n');
                    auto line=buffer->left(n);
                    buffer->remove(0,n+1);
                    QJsonParseError err;
                    auto doc=QJsonDocument::fromJson(line,&err);
                    auto r=doc.object();
                    QString id=r.value("id").toString();
                    if(err.error!=QJsonParseError::NoError||!doc.isObject()||id.isEmpty()||id.size()>128||!r.value("op").isString()||pending->contains(id)||pending->size()>=64){
                        socket->write(QJsonDocument(QJsonObject{
                            {
                                "protocol",1
                            }
                            ,{
                                "id",id
                            }
                            ,{
                                "ok",false
                            }
                            ,{
                                "error","invalid request, duplicate outstanding id, or client limit"
                            }
                        }
                        ).toJson(QJsonDocument::Compact)+"\n");
                        continue;
                    }
                    pending->insert(id);
                    QString op=r.value("op").toString();
                    auto reply=[&,guard=QPointer<QLocalSocket>(socket),pending,canceledIds,id,op,target=r.value("target").toString()](QJsonObject out){
                        QMetaObject::invokeMethod(&app,[&,guard,pending,canceledIds,id,op,target,out]()mutable{
                            pending->remove(id);
                            if(op=="cancel"&&out.value("accepted").toBool())canceledIds->insert(target);
                            bool wasCanceled=canceledIds->remove(id);if(op=="search"&&(wasCanceled||(out.contains("generation")&&out.value("generation").toVariant().toULongLong()!=engine.currentGeneration())))out={
                                {
                                    "ok",false
                                }
                                ,{
                                    "error","canceled"
                                }
                            }
                            ;
                            if(!guard||guard->state()!=QLocalSocket::ConnectedState)return;
                            out["protocol"]=1;
                            out["id"]=id;
                            if(!out.contains("ok"))out["ok"]=true;
                            guard->write(QJsonDocument(out).toJson(QJsonDocument::Compact)+"\n");
                        }
                        ,Qt::QueuedConnection);
                    }
                    ;
                    if(op=="status"){
                        auto status=engine.status();
                        status["watch_error"]=watcherError;
                        reply({
                            {
                                "status",status
                            }
                        }
                        );
                    }
                    else if(op=="shutdown"){
                        if(!shuttingDown){
                            shuttingDown=true;
                            reply({
                                {
                                    "accepted",true
                                }
                            }
                            );
                            server.close();
                            QTimer::singleShot(100,&app,&QCoreApplication::quit);
                        }
                    }
                    else if(op=="cancel"&&(!pending->contains(r.value("target").toString())||r.value("target").toString()==id))reply({
                        {
                            "accepted",false
                        }
                    }
                    );
                    else if(shuttingDown)reply({
                        {
                            "ok",false
                        }
                        ,{
                            "error","stopping"
                        }
                    }
                    );
                    else engine.submit(r,owner,reply);
                }
            }
            );
        }
    }
    );
    qInfo()<<"Listening"<<server.fullServerName();
    return app.exec();
}
