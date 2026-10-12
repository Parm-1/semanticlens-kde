#include "engine.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <limits>
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
    snapshot["index_revision"]=0; snapshot["identity"]=QJsonObject(); snapshot["failures"]=QJsonArray();
    snapshot["phase"]="verifying"; snapshot["recovery_attempt"]=0;
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
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return watched;
}
QJsonObject Engine::status(){
    std::lock_guard<std::mutex> lock(snapshotMutex);
    auto result=snapshot;
    result["paused"]=paused.load();
    if(paused)result["busy"]=false;
    return result;
}
void Engine::disconnect(QString owner){
    std::lock_guard<std::mutex> lock(mutex);
    if(activeOwner==owner)disconnected.insert(owner);for(const auto &task:queue)if(task.owner==owner)canceled.remove(key(owner,task.request.value("id").toString()));queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const Task &t){return t.owner==owner;}),queue.end());
}
void Engine::submit(QJsonObject r, QString owner, Reply reply) {
    std::lock_guard<std::mutex> lock(mutex);
    if (r.value("op").toString()=="cancel") {
        QString target=r.value("target").toString();
        bool owned=activeOwner==owner && activeId==target && !activeCommitted;
        for (const auto &task:queue) if(task.owner==owner && task.request.value("id").toString()==target) owned=true;
        if(owned) canceled.insert(key(owner,target));
        reply({{"accepted",owned}});
        return;
    }
    if(queue.size()>=256) { reply({{"ok",false},{"error","service queue full"}}); return; }
    // Ingress is intent only. Validation and cancellation precede committed state changes.
    queue.push_back({r,owner,reply,generation.load()});
    wake.notify_one();
}
bool Engine::validLocked(const Task &task) {
    return !stop && !canceled.contains(key(task.owner,task.request.value("id").toString()))
        && !disconnected.contains(task.owner)
        && (task.request.value("op").toString()!="search" || task.generation==generation);
}
bool Engine::valid(const Task &task) { std::lock_guard<std::mutex> lock(mutex); return validLocked(task); }
void Engine::commitControl(Task &task, const std::function<void()> &changes) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!validLocked(task)) fail("canceled");
    if(!db.transaction()) fail("control transaction failed");
    try { changes(); if(!db.commit()) fail("control commit failed"); }
    catch(...) { db.rollback(); throw; }
    activeCommitted=true;
    if(task.owner!="watcher") generation++;
    task.generation=generation.load();
    QString op=task.request.value("op").toString();
    if(op=="pause") paused=true;
    if(op=="resume") paused=false;
}
void Engine::servicePending() {
    for(int count=0;count<256 && !stop;count++) {
        Task task;
        { std::lock_guard<std::mutex> lock(mutex); if(queue.empty()) return;
          task=queue.front(); queue.pop_front(); activeOwner=task.owner;
          activeId=task.request.value("id").toString(); activeCommitted=false; }
        handle(task);
    }
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
    std::lock_guard<std::mutex> lock(snapshotMutex);
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
    try {
    if(!producer){
            if(digest(opt.textModel,309855456)!="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135"||digest(opt.imageModel,554821024)!="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f")fail("model SHA256 mismatch");
            if(digest(opt.probe)!=opt.probeSha.toLatin1())fail("unverified producer binary: hash changed before reload");
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
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); throw; }
}
QJsonObject Engine::extractorIdentity() {
    QJsonObject result;
    for(QString program:{"pdftotext","pdfinfo","pdftoppm","tesseract"}) {
        QString executable=QStandardPaths::findExecutable(program);
        if(executable.isEmpty()) { result[program]="unavailable"; continue; }
        QProcess process; process.setProcessChannelMode(QProcess::MergedChannels);
        auto environment=QProcessEnvironment::systemEnvironment(); environment.insert("LC_ALL","C");
        process.setProcessEnvironment(environment);
        process.start(executable,{program=="tesseract"?"--version":"-v"});
        if(!process.waitForStarted(3000)||!process.waitForFinished(5000)) {
            process.kill(); process.waitForFinished(3000); fail("extractor version probe failed: "+program);
        }
        if(process.exitCode()!=0) fail("extractor version probe rejected: "+program);
        QString line=QString::fromUtf8(process.readAll()).split('\n').value(0).trimmed();
        if(line.isEmpty()||line.size()>1024) fail("invalid extractor version: "+program);
        result[program]=line;
    }
    return result;
}
void Engine::verifyExtractors() {
    if(extractorIdentity()!=identity.value("extractors").toObject()) {
        setFailure("extractor identity changed; explicit wipe/rebuild required");
        fail("extractor identity changed; explicit wipe/rebuild required");
    }
}
void Engine::closeDatabase() {
    if(db.isValid()) { QString name=db.connectionName(); db.close(); db=QSqlDatabase(); QSqlDatabase::removeDatabase(name); }
}
void Engine::openDatabase() {
    if(!db.isValid()) { db=QSqlDatabase::addDatabase("QSQLITE","index-worker-"+QUuid::createUuid().toString()); db.setDatabaseName(opt.data+"/index.sqlite"); }
    if(!db.isOpen()&&!db.open()) fail(db.lastError().text());
    sql("PRAGMA foreign_keys=ON"); sql("PRAGMA journal_mode=WAL"); sql("PRAGMA synchronous=FULL");
    QByteArray bytes=QJsonDocument(identity).toJson(QJsonDocument::Compact);
    auto tables=sql("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'");
    bool existing=tables.next();
    if(existing) {
        auto query=sql("SELECT value FROM meta WHERE key='identity'");
        if(!query.next()||query.value(0).toByteArray()!=bytes) fail("stored identity mismatch; explicit wipe/rebuild required");
    }
    sql("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT)");
    qint64 seed=0; QFile checkpoint(opt.data+"/index-revision"); if(checkpoint.open(QIODevice::ReadOnly)) seed=qMax(qint64(0),checkpoint.readAll().trimmed().toLongLong());
    sql("INSERT OR IGNORE INTO meta VALUES('index_revision',?)",{QString::number(seed)});
    sql("INSERT OR IGNORE INTO meta VALUES('identity',?)",{bytes});
    sql("CREATE TABLE IF NOT EXISTS roots(path TEXT PRIMARY KEY,exclusions TEXT NOT NULL)");
    sql("CREATE TABLE IF NOT EXISTS files(path TEXT PRIMARY KEY,root TEXT NOT NULL,hash TEXT NOT NULL,error TEXT NOT NULL,warning TEXT NOT NULL DEFAULT '')");
    sql("CREATE TABLE IF NOT EXISTS pages(id INTEGER PRIMARY KEY,path TEXT NOT NULL REFERENCES files(path) ON DELETE CASCADE,page INTEGER NOT NULL,title TEXT NOT NULL,text TEXT NOT NULL,kind TEXT NOT NULL)");
    sql("CREATE TABLE IF NOT EXISTS segments(id INTEGER PRIMARY KEY,path TEXT NOT NULL REFERENCES files(path) ON DELETE CASCADE,page_id INTEGER NOT NULL REFERENCES pages(id) ON DELETE CASCADE,title TEXT NOT NULL,text TEXT NOT NULL,page INTEGER NOT NULL,kind TEXT NOT NULL,vector BLOB NOT NULL)");
    sql("CREATE VIRTUAL TABLE IF NOT EXISTS terms USING fts5(text,title,content='pages',content_rowid='id')");
    sql("CREATE TRIGGER IF NOT EXISTS pages_ai AFTER INSERT ON pages BEGIN INSERT INTO terms(rowid,text,title) VALUES(new.id,new.text,new.title); UPDATE meta SET value=CAST(value AS INTEGER)+1 WHERE key='index_revision'; END");
    sql("CREATE TRIGGER IF NOT EXISTS pages_ad AFTER DELETE ON pages BEGIN INSERT INTO terms(terms,rowid,text,title) VALUES('delete',old.id,old.text,old.title); UPDATE meta SET value=CAST(value AS INTEGER)+1 WHERE key='index_revision'; END");
    auto p=sql("SELECT value FROM meta WHERE key='paused'"); paused=p.next()&&p.value(0).toString()=="1";
}
void Engine::setFailure(QString error) {
    closeProducer(); operational=false; scan={};
    { std::lock_guard<std::mutex> lock(mutex); generation++; }
    std::lock_guard<std::mutex> lock(snapshotMutex); watched.clear();
    snapshot["ready"]=false; snapshot["model_ready"]=false; snapshot["busy"]=false;
    snapshot["current"]=""; snapshot["error"]=error; snapshot["phase"]="failed";
}
void Engine::initialize() {
    operational=false; closeProducer();
    { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["ready"]=false; snapshot["model_ready"]=false;
      snapshot["error"]=""; snapshot["phase"]="verifying"; snapshot["recovery_attempt"]=snapshot.value("recovery_attempt").toInt()+1; }
    try {
        auto info=embed({{"op","info"}}); if(!info.value("ok").toBool()) fail("producer info failed");
        identity=info.value("info").toObject(); identity.remove("load_seconds");
        if(identity.value("dimension").toInt()!=768||identity.value("max_tokens").toInt()!=2048||identity.value("backend")!="CPU"||identity.value("pipeline")!="digup-vectors1-linux-cpu-probe1"||identity.value("model")!="google/embeddinggemma-2"||identity.value("model_revision")!="bfcd298762cc34d0357ece5ebdd31791a3a374d8"||identity.value("runtime")!="llama.cpp-b11461+no-logits") fail("producer identity mismatch");
        identity["text_sha256"]="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135";
        identity["image_sha256"]="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f";
        identity["producer_sha256"]=QString::fromLatin1(opt.probeSha.toLatin1());
        identity["engine_pipeline"]="kde-index-full-pages-v3-segments1800-image140-ocr32";
        identity["representation"]=3; identity["extractors"]=extractorIdentity();
        identity["dtype"]="float32"; identity["byte_order"]=QSysInfo::ByteOrder==QSysInfo::LittleEndian?"little":"big";
        openDatabase(); operational=true;
        { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["ready"]=true; snapshot["model_ready"]=true; snapshot["error"]=""; snapshot["phase"]="ready"; }
        refresh();
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); throw; }
}
QJsonObject Engine::wipe(Task &task) {
    // Explicit removal is linearized before verification. A failed rebuild cannot restore or adopt old content.
    { std::lock_guard<std::mutex> lock(mutex); if(!validLocked(task)) fail("canceled"); activeCommitted=true; generation++; task.generation=generation.load(); }
    closeProducer(); operational=false; scan={};
    { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["ready"]=false; snapshot["model_ready"]=false; snapshot["busy"]=false; snapshot["phase"]="wiping"; }
    bool removed=false;
    qint64 revision=status().value("index_revision").toVariant().toLongLong();
    if(db.isOpen()) { QSqlQuery q(db); if(q.exec("SELECT value FROM meta WHERE key='index_revision'")&&q.next()) revision=qMax(revision,q.value(0).toLongLong()); }
    else if(QFileInfo::exists(opt.data+"/index.sqlite")) {
        QString name="wipe-revision-"+QUuid::createUuid().toString();
        { auto reader=QSqlDatabase::addDatabase("QSQLITE",name); reader.setConnectOptions("QSQLITE_OPEN_READONLY"); reader.setDatabaseName(opt.data+"/index.sqlite");
          if(reader.open()) { QSqlQuery q(reader); if(q.exec("SELECT value FROM meta WHERE key='index_revision'")&&q.next()) revision=qMax(revision,q.value(0).toLongLong()); } reader.close(); }
        QSqlDatabase::removeDatabase(name);
    }
    QFile oldCheckpoint(opt.data+"/index-revision"); if(oldCheckpoint.open(QIODevice::ReadOnly)) revision=qMax(revision,oldCheckpoint.readAll().trimmed().toLongLong());
    closeDatabase();
    try {
        if(revision==std::numeric_limits<qint64>::max()) fail("index revision overflow");
        QSaveFile checkpoint(opt.data+"/index-revision"); if(!checkpoint.open(QIODevice::WriteOnly)||checkpoint.write(QByteArray::number(revision+1)+"\n")<0||!checkpoint.commit()) fail("wipe revision checkpoint failed");
        for(QString suffix:{"","-wal","-shm"}) { QString file=opt.data+"/index.sqlite"+suffix; if(QFileInfo::exists(file)&&!QFile::remove(file)) fail("owned database removal failed"); }
        removed=true; paused=false;
        { std::lock_guard<std::mutex> lock(snapshotMutex); watched.clear(); snapshot["roots"]=QJsonArray(); snapshot["indexed"]=0; snapshot["segments"]=0; snapshot["errors"]=0; snapshot["failures"]=QJsonArray(); snapshot["index_revision"]=QJsonValue(revision+1); snapshot["current"]=""; }
        initialize();
        return {{"accepted",true},{"removed",true},{"ready",true},{"index_revision",status().value("index_revision")}};
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); return {{"accepted",true},{"removed",removed},{"ready",false},{"recovery_error",QString::fromUtf8(error.what())}}; }
}
void Engine::loop() {
    try {
        for(const auto &name:QDir(opt.data).entryList(QDir::Dirs|QDir::NoDotAndDotDot)) if(QRegularExpression("^extract-[A-Za-z0-9]{6}$").match(name).hasMatch()&&!QFileInfo(opt.data+"/"+name).isSymLink()) if(!QDir(opt.data+"/"+name).removeRecursively()) fail("cannot remove interrupted extraction snapshot");
        initialize(); if(!paused) startScan();
    } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); }
    while(!stop) {
        try {
            servicePending();
            if(operational && scan.generation && !paused) { scanStep(); continue; }
            std::unique_lock<std::mutex> lock(mutex);
            if(!wake.wait_for(lock,std::chrono::seconds(10),[this]{return stop||!queue.empty();})) { lock.unlock(); closeProducer(); }
        } catch(const std::exception &error) { setFailure(QString::fromUtf8(error.what())); }
    }
    closeProducer(); closeDatabase();
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
void Engine::startScan(Task *task){
    verifyExtractors();
    Scan next;
    next.generation=generation.load();
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
                        if(next.files.size()>=10000)fail("scan file limit exceeded");
                        next.files.append(path);
                        next.seen.insert(path);
                        nextWatches<<path;
                    }
                }
                if(stop||(task&&!valid(*task))) fail("canceled");
            }
        }
        auto q=sql("SELECT path FROM files");
        QStringList remove;
        while(q.next())if(!next.seen.contains(q.value(0).toString()))remove<<q.value(0).toString();
        auto removeRows=[&]{for(auto path:remove) sql("DELETE FROM files WHERE path=?",{path});};
        if(task) commitControl(*task,removeRows); else removeRows();
        next.generation=generation.load(); scan=std::move(next);
        { std::lock_guard<std::mutex> lock(snapshotMutex); watched=nextWatches; snapshot["current"]=""; }
        refresh();
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
            std::lock_guard<std::mutex> lock(snapshotMutex);
            snapshot["current"]="";
        }
        refresh();
        return;
    }
    QString path=scan.files[scan.position++];
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        snapshot["current"]=path;
    }
    indexFile(path,scan.generation);
    if(operational) refresh();
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
        // Remove stale searchable content only after current producer identity is validated.
        if(!producer) embed({{"op","info"}});
        servicePending(); if(gen!=generation||paused||stop||!operational)return;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if(gen!=generation||paused||stop||!operational)return;
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
                servicePending(); if(gen!=generation||paused||stop||!operational)return;
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
            servicePending(); if(gen!=generation||paused||stop||!operational)return;
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
                servicePending(); if(gen!=generation||paused||stop||!operational)return;
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
        if(gen!=generation||paused||stop||!operational)return;
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
            QHash<int,qint64> pageIds;
            for(auto value:pages) { auto page=value.toObject(); auto q=sql("INSERT INTO pages(path,page,title,text,kind) VALUES(?,?,?,?,?)",{path,page.value("page").toInt(),fi.fileName(),page.value("text").toString(),kind}); pageIds[page.value("page").toInt()]=q.lastInsertId().toLongLong(); }
            for(auto &segment:segments) sql("INSERT INTO segments(path,page_id,title,text,page,kind,vector) VALUES(?,?,?,?,?,?,?)",{path,pageIds[segment.page],fi.fileName(),segment.text,segment.page,kind,segment.vector});
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
        if(operational&&gen==generation&&!paused&&!stop&&allowed(path)){
            std::lock_guard<std::mutex> lock(mutex);
            if(gen!=generation||paused||stop||!operational)return;
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
void Engine::handle(Task task) {
    QJsonObject result;
    try {
        QString op=task.request.value("op").toString();
        if(!valid(task)) fail("canceled");
        if(op=="wipe") result=wipe(task);
        else {
            if(!operational) {
                if(op!="scan"&&op!="resume") fail(status().value("error").toString());
                initialize();
                if(!valid(task)) fail("canceled");
            }
            if(op=="search") {
                QString text=task.request.value("text").toString().trimmed();
                if(text.isEmpty()||text.size()>2048) fail("query must contain 1..2048 characters");
                QString kind=task.request.value("kind").toString();
                if(!kind.isEmpty()&&!QSet<QString>{"text","pdf","image"}.contains(kind)) fail("unknown kind");
                int limit=task.request.value("limit").toInt(20); if(limit<1||limit>50) fail("limit must be 1..50");
                auto vector=vectorBlob(embed({{"op","query"},{"text",text}}));
                if(!valid(task)) fail("canceled");
                QSet<qint64> fts;
                QStringList tokens; for(QString token:text.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts)) { token.replace('"',"\"\""); tokens<<"\""+token+"\""; }
                auto hits=sql("SELECT rowid FROM terms WHERE terms MATCH ?",{tokens.join(" AND ")}); while(hits.next()) fts.insert(hits.value(0).toLongLong());
                QHash<QString,QJsonObject> best;
                auto better=[](const QJsonObject &a,const QJsonObject &b) {
                    if(a.value("exact").toBool()!=b.value("exact").toBool()) return a.value("exact").toBool();
                    if(a.value("term_match").toBool()!=b.value("term_match").toBool()) return a.value("term_match").toBool();
                    if(a.value("semantic_score").toDouble()!=b.value("semantic_score").toDouble()) return a.value("semantic_score").toDouble()>b.value("semantic_score").toDouble();
                    return a.value("path").toString()<b.value("path").toString();
                };
                auto rows=sql("SELECT s.id,s.path,s.title,s.text,s.page,s.kind,s.vector,f.warning,p.id,p.text FROM segments s JOIN files f ON f.path=s.path JOIN pages p ON p.id=s.page_id");
                while(rows.next()) {
                    QString path=rows.value(1).toString(); if(!kind.isEmpty()&&rows.value(5).toString()!=kind) continue;
                    if(!allowed(path)) continue;
                    QByteArray blob=rows.value(6).toByteArray(); if(blob.size()!=vector.size()) fail("corrupt vector size");
                    double dot=0; for(int i=0;i<768;i++) { float a,b; std::memcpy(&a,vector.constData()+i*sizeof(float),sizeof(float)); std::memcpy(&b,blob.constData()+i*sizeof(float),sizeof(float)); if(!std::isfinite(b)) fail("corrupt nonfinite vector"); dot+=double(a)*b; }
                    QString pageText=rows.value(9).toString(),title=rows.value(2).toString();
                    int position=pageText.indexOf(text,0,Qt::CaseInsensitive);
                    bool literal=position>=0||title.contains(text,Qt::CaseInsensitive),terms=fts.contains(rows.value(8).toLongLong());
                    QString passage=rows.value(3).toString();
                    if(position>=0) { int start=qMax(0,position-200); passage=pageText.mid(start,qMin(pageText.size()-start,text.size()+400)); }
                    double fraction=std::clamp((dot+1)/2,0.0,1.0);
                    double score=literal?1.0:terms?0.90+0.09*fraction:0.89*fraction;
                    QJsonObject row{{"path",path},{"title",title},{"text",passage},{"page",rows.value(4).toInt()},{"kind",rows.value(5).toString()},{"score",score},{"semantic_score",dot},{"exact",literal},{"term_match",terms},{"match_type",literal?"literal":terms?"terms":"semantic"},{"match_scope",position>=0?"text":literal?"title":terms?"page":"segment"},{"warning",rows.value(7).toString()}};
                    if(!best.contains(path)||better(row,best[path])) best[path]=row;
                }
                QList<QJsonObject> ranked=best.values(); std::sort(ranked.begin(),ranked.end(),better);
                QJsonArray results; for(int i=0;i<qMin(limit,int(ranked.size()));i++) results.append(ranked[i]);
                auto revision=sql("SELECT value FROM meta WHERE key='index_revision'"); revision.next();
                { std::lock_guard<std::mutex> lock(mutex); if(!validLocked(task)) fail("canceled"); activeCommitted=true; }
                result={{"results",results},{"identity",identity},{"generation",double(task.generation)},{"index_revision",QJsonValue(revision.value(0).toLongLong())}};
            } else if(op=="add_root") {
                QString path=QFileInfo(task.request.value("path").toString()).canonicalFilePath();
                if(beneath(path,QDir::cleanPath(opt.data))) fail("engine data directory cannot be selected as content");
                if(path.isEmpty()||!QFileInfo(path).isDir()||!QDir::isAbsolutePath(task.request.value("path").toString())||QFileInfo(task.request.value("path").toString()).isSymLink()) fail("root must be an existing absolute non-symlink directory");
                for(auto value:roots()) { QString root=value.toObject().value("path").toString(); if(beneath(path,root)||beneath(root,path)) fail("root already selected or overlaps selection"); }
                commitControl(task,[&]{sql("INSERT INTO roots VALUES(?,?)",{path,QString("[]")});}); result["accepted"]=true;
                try { startScan(); } catch(const std::exception &error) { result["scan_error"]=QString::fromUtf8(error.what()); }
            } else if(op=="remove_root"||op=="set_exclusions") {
                QString path=QDir::cleanPath(task.request.value("path").toString()); auto root=sql("SELECT path FROM roots WHERE path=?",{path}); if(!root.next()) fail("selected root not found");
                QJsonArray exclusions;
                if(op=="set_exclusions") {
                    if(!task.request.value("exclusions").isArray()) fail("exclusions must be an array");
                    for(auto value:task.request.value("exclusions").toArray()) { QString prefix=value.toString(); if(prefix.isEmpty()||QDir::isAbsolutePath(prefix)||prefix.split('/').contains("..")||prefix.contains('\\')) fail("invalid relative exclusion prefix"); prefix=QDir::cleanPath(prefix); if(prefix==".") fail("invalid exclusion prefix"); exclusions.append(prefix); }
                }
                commitControl(task,[&]{
                    if(op=="remove_root") { sql("DELETE FROM files WHERE root=?",{path}); sql("DELETE FROM roots WHERE path=?",{path}); }
                    else { sql("UPDATE roots SET exclusions=? WHERE path=?",{QJsonDocument(exclusions).toJson(QJsonDocument::Compact),path}); auto files=sql("SELECT path FROM files WHERE root=?",{path}); QStringList remove; while(files.next()) if(!allowed(files.value(0).toString())) remove<<files.value(0).toString(); for(QString file:remove) sql("DELETE FROM files WHERE path=?",{file}); }
                }); result["accepted"]=true;
                try { startScan(); } catch(const std::exception &error) { result["scan_error"]=QString::fromUtf8(error.what()); }
            } else if(op=="pause") {
                commitControl(task,[&]{sql("INSERT OR REPLACE INTO meta VALUES('paused','1')");}); scan={};
                { std::lock_guard<std::mutex> lock(snapshotMutex); snapshot["current"]=""; }
                result["accepted"]=true;
            } else if(op=="resume") {
                commitControl(task,[&]{sql("INSERT OR REPLACE INTO meta VALUES('paused','0')");}); result["accepted"]=true;
                try { startScan(); } catch(const std::exception &error) { result["scan_error"]=QString::fromUtf8(error.what()); }
            } else if(op=="scan") { startScan(&task); result["accepted"]=true; }
            else fail("unknown operation");
        }
        if(operational) refresh();
    } catch(const std::exception &error) { result={{"ok",false},{"error",QString::fromUtf8(error.what())}}; }
    task.reply(result);
    { std::lock_guard<std::mutex> lock(mutex); canceled.remove(key(task.owner,task.request.value("id").toString())); disconnected.remove(task.owner); activeOwner.clear(); activeId.clear(); activeCommitted=false; }
}
