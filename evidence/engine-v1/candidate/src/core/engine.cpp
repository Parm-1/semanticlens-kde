#include "engine.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
static_assert(sizeof(float)==4,"vector ABI requires IEEE float32");
namespace {
    [[noreturn]] void fail(QString message){
        throw std::runtime_error(message.toStdString());
    }
    QByteArray digest(QString path,qint64 expected=-1){
        QFile f(path);
        if(!f.open(QIODevice::ReadOnly))fail("cannot read "+path);
        if(expected>=0&&f.size()!=expected)fail("model byte size mismatch");
        QCryptographicHash h(QCryptographicHash::Sha256);
        if(!h.addData(&f))fail("hash read failed");
        return h.result().toHex();
    }
    bool beneath(QString path,QString root){
        return path==root||path.startsWith(root+"/");
    }
    QString key(QString owner,QString id){
        return owner+"\n"+id;
    }
    QByteArray vectorBlob(QJsonObject response){
        if(!response.value("ok").toBool())fail(response.value("error").toString("embedding failure"));
        auto a=response.value("vector").toArray();
        if(a.size()!=768)fail("wrong vector dimension");
        QByteArray blob(768*int(sizeof(float)),Qt::Uninitialized);
        double norm=0;
        for(int i=0;i<768;i++){
            if(!a[i].isDouble())fail("invalid vector field");
            float v=float(a[i].toDouble());
            if(!std::isfinite(v))fail("nonfinite vector");
            norm+=double(v)*v;
            std::memcpy(blob.data()+i*sizeof(float),&v,sizeof(float));
        }
        if(std::abs(norm-1)>0.002)fail("unnormalized vector");
        return blob;
    }
    QString extract(QString program,QStringList args,int seconds=30){
        QProcess p;
        p.setProcessChannelMode(QProcess::SeparateChannels);
        p.start(program,args);
        if(!p.waitForStarted(5000))fail("extractor unavailable: "+program);
        QByteArray out;
        QElapsedTimer t;
        t.start();
        while(p.state()!=QProcess::NotRunning){
            p.waitForReadyRead(100);
            out+=p.readAllStandardOutput();
            p.readAllStandardError();
            if(out.size()>2*1024*1024||t.elapsed()>seconds*1000){
                p.kill();
                p.waitForFinished(5000);
                fail("extraction limit exceeded");
            }
        }
        out+=p.readAllStandardOutput();
        if(p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0)fail("extractor failed: "+program);
        if(out.size()>2*1024*1024)fail("extraction output too large");
        return QString::fromUtf8(out);
    }
}
Engine::Engine(EngineOptions options):opt(options){
    snapshot={
        {
            "ready",false
        }
        ,{
            "model_ready",false
        }
        ,{
            "busy",false
        }
        ,{
            "paused",false
        }
        ,{
            "roots",QJsonArray()
        }
        ,{
            "indexed",0
        }
        ,{
            "segments",0
        }
        ,{
            "errors",0
        }
        ,{
            "current",""
        }
        ,{
            "progress",0
        }
        ,{
            "total",0
        }
        ,{
            "error",""
        }
    }
    ;
    thread=std::thread([this]{
        loop();
    }
    );
}
Engine::~Engine(){
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop=true;
        generation++;
    }
    wake.notify_all();
    if(thread.joinable())thread.join();
}
QStringList Engine::watchPaths(){
    std::lock_guard<std::mutex> lock(mutex);
    return watched;
}
QJsonObject Engine::status(){
    std::lock_guard<std::mutex> lock(mutex);
    auto result=snapshot;
    result["paused"]=paused.load();
    if(paused)result["busy"]=false;
    return result;
}
void Engine::disconnect(QString owner){
    std::lock_guard<std::mutex> lock(mutex);
    if(activeOwner==owner)disconnected.insert(owner);for(const auto &task:queue)if(task.owner==owner)canceled.remove(key(owner,task.request.value("id").toString()));queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const Task &t){return t.owner==owner;}),queue.end());
}
void Engine::submit(QJsonObject r,QString owner,Reply reply){
    QString op=r.value("op").toString();
    if(op=="cancel"){
        {
            std::lock_guard<std::mutex> lock(mutex);
            QString target=r.value("target").toString();bool owned=(activeOwner==owner&&activeId==target);for(const auto &task:queue)if(task.owner==owner&&task.request.value("id").toString()==target)owned=true;if(owned)canceled.insert(key(owner,target));
        }
        reply({
            {
                "accepted",true
            }
        }
        );
        return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    if(queue.size()>=256){reply({{"ok",false},{"error","service queue full"}});return;}
    if(op=="pause")paused=true;
    if(op=="resume")paused=false;
    if(op=="pause"||op=="resume"||(op=="scan"&&owner!="watcher")||op=="add_root"||op=="remove_root"||op=="set_exclusions"||op=="wipe")generation++;
    queue.push_back({
        r,owner,reply,generation.load()
    }
    );
    wake.notify_one();
}
bool Engine::valid(const Task &t){
    std::lock_guard<std::mutex> lock(mutex);
    return !stop&&t.generation==generation&&!canceled.contains(key(t.owner,t.request.value("id").toString()))&&!disconnected.contains(t.owner);
}
QSqlQuery Engine::sql(QString statement,QVariantList values){
    QSqlQuery q(db);
    if(!q.prepare(statement))fail(q.lastError().text());
    for(auto &v:values)q.addBindValue(v);
    if(!q.exec())fail(q.lastError().text());
    return q;
}
QJsonArray Engine::roots(){
    QJsonArray result;
    auto q=sql("SELECT path,exclusions FROM roots ORDER BY path");
    while(q.next())result.append(QJsonObject{
        {
            "path",q.value(0).toString()
        }
        ,{
            "exclusions",QJsonDocument::fromJson(q.value(1).toByteArray()).array()
        }
    }
    );
    return result;
}
void Engine::refresh(){
    auto rr=roots();
    QJsonArray failures;
    auto f=sql("SELECT path,error,warning FROM files WHERE error<>'' OR warning<>'' LIMIT 20");
    while(f.next())failures.append(QJsonObject{
        {
            "path",f.value(0).toString()
        }
        ,{
            "error",f.value(1).toString()
        }
        ,{
            "warning",f.value(2).toString()
        }
    }
    );
    auto count=[this](QString s){
        auto q=sql(s);
        q.next();
        return q.value(0).toInt();
    }
    ;
    auto revisionQuery=sql("SELECT value FROM meta WHERE key='index_revision'");revisionQuery.next();
    QJsonObject update{
        {
            "index_revision",QJsonValue(revisionQuery.value(0).toLongLong())
        },{
            "roots",rr
        }
        ,{
            "indexed",count("SELECT count(*) FROM files WHERE error='' ")
        }
        ,{
            "errors",count("SELECT count(*) FROM files WHERE error<>''")
        }
        ,{
            "segments",count("SELECT count(*) FROM segments")
        }
        ,{
            "busy",scan.generation!=0&&!paused
        }
        ,{
            "paused",paused.load()
        }
        ,{
            "progress",scan.position
        }
        ,{
            "total",scan.files.size()
        }
        ,{
            "identity",identity
        }
        ,{
            "failures",failures
        }
    }
    ;
    std::lock_guard<std::mutex> lock(mutex);
    for(auto i=update.begin();i!=update.end();++i)snapshot[i.key()]=i.value();
}
void Engine::closeProducer(){
    if(!producer)return;
    if(producer->state()!=QProcess::NotRunning){
        producer->closeWriteChannel();
        if(!producer->waitForFinished(1500)){
            producer->terminate();
            if(!producer->waitForFinished(1500)){
                producer->kill();
                producer->waitForFinished(5000);
            }
        }
    }
    producer.reset();
    producerBuffer.clear();
}
QJsonObject Engine::embed(QJsonObject request){
    if(!producer){
            if(digest(opt.textModel,309855456)!="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135"||digest(opt.imageModel,554821024)!="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f")fail("model SHA256 mismatch");
            if(digest(opt.probe)!=opt.probeSha.toLatin1())fail("producer hash changed before reload");
        producer=std::make_unique<QProcess>();
        producer->setProcessChannelMode(QProcess::SeparateChannels);
        producer->start(opt.probe,{
            opt.textModel,opt.imageModel,QString::number(opt.threads)
        }
        );
        if(!producer->waitForStarted(5000)){
            closeProducer();
            fail("producer failed to start");
        }
    }
    QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    request["id"]=id;
    producer->write(QJsonDocument(request).toJson(QJsonDocument::Compact)+"\n");
    QElapsedTimer timer;
    timer.start();
    while(!producerBuffer.contains('\n')){
        producer->waitForReadyRead(100);
        producerBuffer+=producer->readAllStandardOutput();
        producer->readAllStandardError();
        if(stop||timer.elapsed()>90000||producerBuffer.size()>128*1024||producer->state()==QProcess::NotRunning){
            closeProducer();
            fail(stop?"stopping":"producer timeout or exit");
        }
    }
    int n=producerBuffer.indexOf('\n');
    auto line=producerBuffer.left(n);
    producerBuffer.remove(0,n+1);
    QJsonParseError error;
    auto doc=QJsonDocument::fromJson(line,&error);
    auto r=doc.object();
    if(error.error!=QJsonParseError::NoError||r.value("id").toString()!=id||!r.value("ok").isBool()){
        closeProducer();
        fail("producer protocol mismatch");
    }
    return r;
}
void Engine::loop(){
    try{
        for(const auto &name:QDir(opt.data).entryList(QDir::Dirs|QDir::NoDotAndDotDot)){if(QRegularExpression("^extract-[A-Za-z0-9]{6}$").match(name).hasMatch()&&!QFileInfo(opt.data+"/"+name).isSymLink())if(!QDir(opt.data+"/"+name).removeRecursively())fail("cannot remove interrupted extraction snapshot");}
        if(digest(opt.textModel,309855456)!="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135"||digest(opt.imageModel,554821024)!="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f")fail("model SHA256 mismatch");
        auto probeHash=digest(opt.probe);
        if(probeHash!=opt.probeSha.toLatin1())fail("unverified producer binary; rebuild requires accepted successor identity");
        auto info=embed({
            {
                "op","info"
            }
        }
        );
        if(!info.value("ok").toBool())fail("producer info failed");
        identity=info.value("info").toObject();
        identity.remove("load_seconds");
        if(identity.value("dimension").toInt()!=768||identity.value("max_tokens").toInt()!=2048||identity.value("backend")!="CPU"||identity.value("pipeline")!="digup-vectors1-linux-cpu-probe1"||identity.value("model")!="google/embeddinggemma-2"||identity.value("model_revision")!="bfcd298762cc34d0357ece5ebdd31791a3a374d8"||identity.value("runtime")!="llama.cpp-b11461+no-logits")fail("producer identity mismatch");
        identity["text_sha256"]="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135";
        identity["image_sha256"]="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f";
        identity["producer_sha256"]=QString::fromLatin1(probeHash);
        identity["engine_pipeline"]="kde-index-segments-1800-v2-image140-ocr32";
        identity["dtype"]="float32";identity["byte_order"]=QSysInfo::ByteOrder==QSysInfo::LittleEndian?"little":"big";
        db=QSqlDatabase::addDatabase("QSQLITE","index-worker-"+QUuid::createUuid().toString());
        db.setDatabaseName(opt.data+"/index.sqlite");
        if(!db.open())fail(db.lastError().text());
        sql("PRAGMA foreign_keys=ON");
        sql("PRAGMA journal_mode=WAL");
        sql("PRAGMA synchronous=FULL");
        sql("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT)");
        sql("INSERT OR IGNORE INTO meta VALUES('index_revision','0')");
        sql("CREATE TABLE IF NOT EXISTS roots(path TEXT PRIMARY KEY,exclusions TEXT NOT NULL)");
        sql("CREATE TABLE IF NOT EXISTS files(path TEXT PRIMARY KEY,root TEXT NOT NULL,hash TEXT NOT NULL,error TEXT NOT NULL,warning TEXT NOT NULL DEFAULT '')");
        sql("CREATE TABLE IF NOT EXISTS segments(id INTEGER PRIMARY KEY,path TEXT NOT NULL REFERENCES files(path) ON DELETE CASCADE,title TEXT NOT NULL,text TEXT NOT NULL,page INTEGER NOT NULL,kind TEXT NOT NULL,vector BLOB NOT NULL)");
        sql("CREATE VIRTUAL TABLE IF NOT EXISTS terms USING fts5(text,title,content='segments',content_rowid='id')");
        sql("CREATE TRIGGER IF NOT EXISTS segments_ai AFTER INSERT ON segments BEGIN INSERT INTO terms(rowid,text,title) VALUES(new.id,new.text,new.title); END");
        sql("CREATE TRIGGER IF NOT EXISTS segments_revision_insert AFTER INSERT ON segments BEGIN UPDATE meta SET value=CAST(value AS INTEGER)+1 WHERE key='index_revision'; END");
        sql("CREATE TRIGGER IF NOT EXISTS segments_revision_delete AFTER DELETE ON segments BEGIN UPDATE meta SET value=CAST(value AS INTEGER)+1 WHERE key='index_revision'; END");
        sql("CREATE TRIGGER IF NOT EXISTS segments_ad AFTER DELETE ON segments BEGIN INSERT INTO terms(terms,rowid,text,title) VALUES('delete',old.id,old.text,old.title); END");
        QByteArray identityBytes=QJsonDocument(identity).toJson(QJsonDocument::Compact);
        auto q=sql("SELECT value FROM meta WHERE key='identity'");
        if(q.next()){
            if(q.value(0).toByteArray()!=identityBytes)fail("stored identity mismatch; use a fresh data directory");
        }
        else sql("INSERT INTO meta VALUES('identity',?)",{
            identityBytes
        }
        );
        auto p=sql("SELECT value FROM meta WHERE key='paused'");
        if(p.next())paused=p.value(0).toString()=="1";
        {
            std::lock_guard<std::mutex> lock(mutex);
            snapshot["ready"]=true;
            snapshot["model_ready"]=true;
        }
        refresh();
        if(!paused)startScan();
        while(!stop){
            Task task;
            bool have=false;
            {
                std::unique_lock<std::mutex> lock(mutex);
                if(queue.empty()&&(scan.generation==0||paused)){
                    if(!wake.wait_for(lock,std::chrono::seconds(10),[this]{
                        return stop||!queue.empty();
                    }
                    )){
                        lock.unlock();
                        closeProducer();
                        continue;
                    }
                }
                if(stop)break;
                if(!queue.empty()){
                    task=queue.front();
                    activeOwner=task.owner;activeId=task.request.value("id").toString();
                    queue.pop_front();
                    have=true;
                }
            }
            if(have)handle(task);
            else if(scan.generation&&!paused)scanStep();
        }
    }
    catch(const std::exception &e){
        {
            std::lock_guard<std::mutex> lock(mutex);
            snapshot["error"]=e.what();
            snapshot["ready"]=false;
            snapshot["busy"]=false;
        }
        while(!stop){
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock,[this]{
                    return stop||!queue.empty();
                }
                );
                if(stop)break;
                task=queue.front();
                queue.pop_front();
            }
            task.reply({
                {
                    "ok",false
                }
                ,{
                    "error",status().value("error")
                }
            }
            );
        }
    }
    closeProducer();
    if(db.isValid()){
        QString name=db.connectionName();
        db.close();
        db=QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}
QString Engine::rootFor(QString path){
    auto rr=roots();
    for(auto v:rr){
        auto r=v.toObject();
        if(beneath(path,r.value("path").toString()))return r.value("path").toString();
    }
    return {
    }
    ;
}
bool Engine::allowed(QString path){
    QFileInfo info(path);
    if(beneath(path,QDir::cleanPath(opt.data))||!info.exists()||info.isSymLink()||!info.isFile()||info.canonicalFilePath()!=path)return false;
    QString root=rootFor(path);
    if(root.isEmpty()||path==root)return false;
    QString rel=path.mid(root.size()+1);
    if(QSet<QString>{"credentials","secrets","passwords","id_rsa"}.contains(info.completeBaseName().toLower()))return false;
    static QSet<QString> deny={
        "node_modules","build","dist","target","vendor","cache","__pycache__","credentials","secrets","private_keys","venv","passwords","cmakefiles","_build"
    }
    ;
    for(auto part:rel.split('/'))if(part.startsWith('.')||part.toLower().startsWith("build-")||deny.contains(part.toLower()))return false;
    auto q=sql("SELECT exclusions FROM roots WHERE path=?",{
        root
    }
    );
    q.next();
    for(auto v:QJsonDocument::fromJson(q.value(0).toByteArray()).array())if(beneath(rel,v.toString()))return false;
    return QSet<QString>{
        "txt","md","pdf","jpg","jpeg","png"
    }
    .contains(info.suffix().toLower());
}
void Engine::startScan(){
    try{
        scan={
        }
        ;
        scan.generation=generation.load();
        QStringList nextWatches;
        auto rr=roots();
        for(auto v:rr){
            QString root=v.toObject().value("path").toString();
            QStringList dirs{
                root
            }
            ;
            while(!dirs.isEmpty()){
                QString dir=dirs.takeLast();
                nextWatches<<dir;
                QDir d(dir);
                for(auto fi:d.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
                    if(fi.isSymLink()||fi.fileName().startsWith('.'))continue;
                    QString path=fi.absoluteFilePath();
                    if(fi.isDir()){
                        QString rel=path.mid(root.size()+1);
                        bool exclude=false;
                        static QSet<QString> deny={
                            "node_modules","build","dist","target","vendor","cache","__pycache__","credentials","secrets","private_keys","venv","passwords","cmakefiles","_build"
                        }
                        ;
                        if(beneath(path,QDir::cleanPath(opt.data))||deny.contains(fi.fileName().toLower())||fi.fileName().toLower().startsWith("build-"))exclude=true;
                        for(auto e:v.toObject().value("exclusions").toArray())if(beneath(rel,e.toString()))exclude=true;
                        if(!exclude)dirs.append(path);
                    }
                    else if(allowed(path)){
                        if(scan.files.size()>=10000)fail("scan file limit exceeded");
                        scan.files.append(path);
                        scan.seen.insert(path);
                        nextWatches<<path;
                    }
                }
                if(generation!=scan.generation||stop){
                    scan={
                    }
                    ;
                    refresh();
                    return;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            watched=nextWatches;
        }
        auto q=sql("SELECT path FROM files");
        QStringList remove;
        while(q.next())if(!scan.seen.contains(q.value(0).toString()))remove<<q.value(0).toString();
        for(auto path:remove)sql("DELETE FROM files WHERE path=?",{
            path
        }
        );
        refresh();
    }
    catch(...){
        scan={
        }
        ;
        refresh();
        throw;
    }
}
void Engine::scanStep(){
    if(scan.generation!=generation){
        scan={
        }
        ;
        refresh();
        return;
    }
    if(scan.position>=scan.files.size()){
        scan.generation=0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            snapshot["current"]="";
        }
        refresh();
        return;
    }
    QString path=scan.files[scan.position++];
    {
        std::lock_guard<std::mutex> lock(mutex);
        snapshot["current"]=path;
    }
    indexFile(path,scan.generation);
    refresh();
}
void Engine::indexFile(QString path,quint64 gen){
    QString sourceHash("");
    try{
        if(!allowed(path))return;
        QFileInfo fi(path);
        if(fi.size()>32*1024*1024)fail("file exceeds 32 MiB");
        sourceHash=QString::fromLatin1(digest(path));
        auto old=sql("SELECT hash,error FROM files WHERE path=?",{
            path
        }
        );
        if(old.next()&&old.value(0).toString()==sourceHash&&old.value(1).toString().isEmpty())return;
        // Remove stale searchable content before attempting replacement.
        {
            std::lock_guard<std::mutex> lock(mutex);
            if(gen!=generation||paused||stop)return;
            sql("DELETE FROM files WHERE path=?",{
                path
            }
            );
        }
        QTemporaryDir temp(opt.data+"/extract-XXXXXX");
        if(!temp.isValid())fail("temporary directory failed");
        QString copy=temp.path()+"/input."+fi.suffix();
        if(!QFile::copy(path,copy)||digest(copy)!=sourceHash.toLatin1())fail("source changed while snapshotting");
        QString kind=fi.suffix().toLower()=="pdf"?"pdf":QSet<QString>{
            "jpg","jpeg","png"
        }
        .contains(fi.suffix().toLower())?"image":"text";
        QString text,warning("");
        QJsonArray pages;
        if(kind=="pdf"){
            text=extract("pdftotext",{
                "-layout","-enc","UTF-8",copy,"-"
            }
            );
            QString meta=extract("pdfinfo",{
                copy
            }
            );
            auto match=QRegularExpression(R"(Pages:\s+(\d+))").match(meta);
            if(!match.hasMatch())fail("cannot determine PDF page count");
            int count=match.captured(1).toInt();
            auto parts=text.split(QChar('\f'));
            for(int page=1;page<=count;page++){
                if(gen!=generation||paused||stop)return;
                QString part=page<=parts.size()?parts[page-1].trimmed():QString();
                if(part.isEmpty()){
                    if(page>32){
                        warning="OCR page limit: empty pages after32 were skipped";
                        continue;
                    }
                    try{
                        QString prefix=temp.path()+"/page";
                        extract("pdftoppm",{
                            "-f",QString::number(page),"-l",QString::number(page),"-singlefile","-scale-to","1600","-png",copy,prefix
                        }
                        ,30);
                        part=extract("tesseract",{
                            prefix+".png","stdout","--psm","3"
                        }
                        ,30).trimmed();
                        if(part.isEmpty())warning="PDF page "+QString::number(page)+": OCR produced no readable text";
                    }
                    catch(const std::exception &e){
                        warning="PDF page "+QString::number(page)+": "+QString::fromUtf8(e.what());
                    }
                }
                if(!part.isEmpty())pages.append(QJsonObject{
                    {
                        "page",page
                    }
                    ,{
                        "text",part
                    }
                }
                );
            }
            if(pages.isEmpty())fail(warning.isEmpty()?"PDF produced no readable text":warning);
        }
        else if(kind=="text"){
            QFile f(copy);
            if(!f.open(QIODevice::ReadOnly))fail("text read failed");
            auto bytes=f.readAll();
            if(bytes.contains('\0')||bytes.size()>2*1024*1024)fail("binary text or extraction limit");
            text=QString::fromUtf8(bytes);
            if(text.trimmed().isEmpty())fail("empty text");
            pages.append(QJsonObject{
                {
                    "page",0
                }
                ,{
                    "text",text
                }
            }
            );
        }
        else{
            try{
                text=extract("tesseract",{
                    copy,"stdout","--psm","3"
                }
                ,30).trimmed();
            }
            catch(const std::exception &e){
                text="";
                warning="Image OCR: "+QString::fromUtf8(e.what());
            }
            pages.append(QJsonObject{
                {
                    "page",0
                }
                ,{
                    "text",text
                }
            }
            );
        }
        struct Segment{
            QString text;
            int page;
            QByteArray vector;
        }
        ;
        QList<Segment> segments;
        for(auto v:pages){
            if(gen!=generation||paused||stop)return;
            auto page=v.toObject();
            QString source=page.value("text").toString();
            if(kind=="image"){
                segments.append({
                    source.left(1800),page.value("page").toInt(),vectorBlob(embed({
                        {
                            "op","image"
                        }
                        ,{
                            "path",copy
                        }
                        ,{
                            "budget",140
                        }
                    }
                    ))
                }
                );
            }
            for(int start=0;start<source.size();start+=1800){
                if(segments.size()>=256)fail("segment limit exceeded");
                if(gen!=generation||paused||stop)return;
                QString passage=source.mid(start,1800);
                segments.append({
                    passage,page.value("page").toInt(),vectorBlob(embed({
                        {
                            "op","document"
                        }
                        ,{
                            "title",fi.fileName()
                        }
                        ,{
                            "text",passage
                        }
                    }
                    ))
                }
                );
            }
        }
        if(gen!=generation||paused||stop||!allowed(path)||digest(path)!=sourceHash.toLatin1())return;
        std::lock_guard<std::mutex> commitLock(mutex);
        if(gen!=generation||paused||stop)return;
        if(!db.transaction())fail("transaction failed");
        try{
            sql("INSERT INTO files(path,root,hash,error) VALUES(?,?,?,?)",{
                path,rootFor(path),sourceHash,QString("")
            }
            );
            sql("UPDATE files SET warning=? WHERE path=?",{
                warning,path
            }
            );
            for(auto &s:segments)sql("INSERT INTO segments(path,title,text,page,kind,vector) VALUES(?,?,?,?,?,?)",{
                path,fi.fileName(),s.text,s.page,kind,s.vector
            }
            );
            if(gen!=generation||paused||stop){
                db.rollback();
                return;
            }
            if(!db.commit())fail("commit failed");
        }
        catch(...){
            db.rollback();
            throw;
        }
    }
    catch(const std::exception &e){
        if(gen==generation&&!paused&&!stop&&allowed(path)){
            std::lock_guard<std::mutex> lock(mutex);
            if(gen!=generation||paused||stop)return;
            sql("DELETE FROM files WHERE path=?",{
                path
            }
            );
            sql("INSERT INTO files(path,root,hash,error) VALUES(?,?,?,?)",{
                path,rootFor(path),sourceHash,QString::fromUtf8(e.what())
            }
            );
        }
    }
}
void Engine::handle(Task t){
    QJsonObject result;
    try{
        QString op=t.request.value("op").toString();
        if(!valid(t))fail("canceled");
        if(op=="search"){
            QString text=t.request.value("text").toString().trimmed();
            if(text.isEmpty()||text.size()>2048)fail("query must contain 1..2048 characters");
            QString kind=t.request.value("kind").toString();
            if(!kind.isEmpty()&&!QSet<QString>{
                "text","pdf","image"
            }
            .contains(kind))fail("unknown kind");
            int limit=t.request.value("limit").toInt(20);
            if(limit<1||limit>50)fail("limit must be 1..50");
            auto query=vectorBlob(embed({
                {
                    "op","query"
                }
                ,{
                    "text",text
                }
            }
            ));
            if(!valid(t))fail("canceled");
            QSet<qint64> fts;
            QStringList tokens;
            for(auto token:text.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts)){
                token.replace('"',"\"\"");
                tokens<<"\""+token+"\"";
            }
            auto hit=sql("SELECT rowid FROM terms WHERE terms MATCH ?",{
                tokens.join(" AND ")
            }
            );
            while(hit.next())fts.insert(hit.value(0).toLongLong());
            QHash<QString,QJsonObject> best;
            auto q=sql("SELECT s.id,s.path,s.title,s.text,s.page,s.kind,s.vector,f.warning FROM segments s JOIN files f ON f.path=s.path");
            while(q.next()){
                QString path=q.value(1).toString();
                if(!kind.isEmpty()&&q.value(5).toString()!=kind)continue;
                if(!allowed(path))continue;
                QByteArray blob=q.value(6).toByteArray();
                if(blob.size()!=query.size())fail("corrupt vector size");
                double dot=0;
                for(int i=0;i<768;i++){
                    float a,b;
                    std::memcpy(&a,query.constData()+i*sizeof(float),sizeof(float));
                    std::memcpy(&b,blob.constData()+i*sizeof(float),sizeof(float));
                    if(!std::isfinite(b))fail("corrupt nonfinite vector");
                    dot+=double(a)*b;
                }
                bool exact=q.value(3).toString().contains(text,Qt::CaseInsensitive)||q.value(2).toString().contains(text,Qt::CaseInsensitive)||fts.contains(q.value(0).toLongLong());
                double score=exact?1.0:std::clamp(dot,-1.0,1.0);
                QJsonObject row{
                    {
                        "path",path
                    }
                    ,{
                        "title",q.value(2).toString()
                    }
                    ,{
                        "text",q.value(3).toString()
                    }
                    ,{
                        "page",q.value(4).toInt()
                    }
                    ,{
                        "kind",q.value(5).toString()
                    }
                    ,{
                        "score",score
                    }
                    ,{
                        "semantic_score",dot
                    }
                    ,{
                        "exact",exact
                    }
                    ,{
                        "warning",q.value(7).toString()
                    }
                }
                ;
                if(!best.contains(path)||best[path].value("score").toDouble()<score)best[path]=row;
            }
            QList<QJsonObject> rows=best.values();
            std::sort(rows.begin(),rows.end(),[](auto a,auto b){
                if(a.value("exact").toBool()!=b.value("exact").toBool())return a.value("exact").toBool();
                double x=a.value("score").toDouble(),y=b.value("score").toDouble();
                return x==y?a.value("path").toString()<b.value("path").toString():x>y;
            }
            );
            QJsonArray results;
            for(int i=0;i<qMin(limit,int(rows.size()));i++)results.append(rows[i]);
            if(!valid(t))fail("canceled");
            auto revision=sql("SELECT value FROM meta WHERE key='index_revision'");revision.next();
            result={
                {"index_revision",QJsonValue(revision.value(0).toLongLong())},{
                    "results",results
                }
                ,{
                    "identity",identity
                }
                ,{
                    "generation",double(t.generation)
                }
            }
            ;
        }
        else if(op=="add_root"){
            QString path=QFileInfo(t.request.value("path").toString()).canonicalFilePath();
            if(beneath(path,QDir::cleanPath(opt.data)))fail("engine data directory cannot be selected as content");
            if(path.isEmpty()||!QFileInfo(path).isDir()||!QDir::isAbsolutePath(t.request.value("path").toString())||QFileInfo(t.request.value("path").toString()).isSymLink())fail("root must be an existing absolute non-symlink directory");
            for(auto v:roots()){
                QString r=v.toObject().value("path").toString();
                if(beneath(path,r)||beneath(r,path))fail("root already selected or overlaps selection");
            }
            sql("INSERT INTO roots VALUES(?,?)",{
                path,QString("[]")
            }
            );
            startScan();
            result["accepted"]=true;
        }
        else if(op=="remove_root"||op=="set_exclusions"){
            QString path=QDir::cleanPath(t.request.value("path").toString());
            auto q=sql("SELECT path FROM roots WHERE path=?",{
                path
            }
            );
            if(!q.next())fail("selected root not found");
            if(op=="remove_root"){
                sql("DELETE FROM files WHERE root=?",{
                    path
                }
                );
                sql("DELETE FROM roots WHERE path=?",{
                    path
                }
                );
            }
            else{
                if(!t.request.value("exclusions").isArray())fail("exclusions must be an array");
                QJsonArray exclusions;
                for(auto v:t.request.value("exclusions").toArray()){
                    QString s=v.toString();
                    if(s.isEmpty()||QDir::isAbsolutePath(s)||s.split('/').contains("..")||s.contains('\\'))fail("invalid relative exclusion prefix");
                    s=QDir::cleanPath(s);
                    if(s==".")fail("invalid exclusion prefix");
                    exclusions.append(s);
                }
                sql("UPDATE roots SET exclusions=? WHERE path=?",{
                    QJsonDocument(exclusions).toJson(QJsonDocument::Compact),path
                }
                );
                auto f=sql("SELECT path FROM files WHERE root=?",{
                    path
                }
                );
                QStringList remove;
                while(f.next())if(!allowed(f.value(0).toString()))remove<<f.value(0).toString();
                for(auto p:remove)sql("DELETE FROM files WHERE path=?",{
                    p
                }
                );
            }
            startScan();
            result["accepted"]=true;
        }
        else if(op=="scan"||op=="resume"){
            if(op=="resume")sql("INSERT OR REPLACE INTO meta VALUES('paused','0')");
            startScan();
            result["accepted"]=true;
        }
        else if(op=="pause"){
            scan={
            }
            ;
            sql("INSERT OR REPLACE INTO meta VALUES('paused','1')");
            result["accepted"]=true;
        }
        else if(op=="wipe"){
            scan={
            }
            ;
            {
                std::lock_guard<std::mutex> lock(mutex);
                watched.clear();
            }
            sql("DELETE FROM files");
            sql("DELETE FROM roots");
            result["accepted"]=true;
        }
        else fail("unknown operation");
        refresh();
    }
    catch(const std::exception &e){
        result={
            {
                "ok",false
            }
            ,{
                "error",QString::fromUtf8(e.what())
            }
        }
        ;
    }
    t.reply(result);
    {
        std::lock_guard<std::mutex> lock(mutex);
        canceled.remove(key(t.owner,t.request.value("id").toString()));disconnected.remove(t.owner);activeOwner.clear();activeId.clear();
    }
}
