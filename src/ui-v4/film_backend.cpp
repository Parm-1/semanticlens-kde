#include "film_backend.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

FilmBackend::FilmBackend(QString socketName,QString dataDirectory,QObject *parent)
    :QObject(parent),m_socketName(socketName),m_data(QDir::cleanPath(dataDirectory)) {
    m_clock.start();m_poll.setInterval(1000);m_retry.setInterval(1000);m_debounce.setSingleShot(true);m_debounce.setInterval(250);m_deadlines.setInterval(1000);
    connect(&m_socket,&QLocalSocket::connected,this,[this]{m_retry.stop();m_capId=send("film_capabilities");m_poll.start();});
    connect(&m_socket,&QLocalSocket::readyRead,this,&FilmBackend::receive);
    connect(&m_socket,&QLocalSocket::disconnected,this,[this]{m_pending.clear();m_abandonedNavigation.clear();m_statusId.clear();m_searchId.clear();m_capId.clear();m_buffer.clear();m_available=false;m_supported=false;m_includeUnverifiedPrior=false;invalidate();m_message="Film service disconnected.";if(m_enabled)m_retry.start();emit changed();});
    connect(&m_socket,&QLocalSocket::errorOccurred,this,[this](auto){if(m_enabled)m_retry.start();});
    connect(&m_retry,&QTimer::timeout,this,[this]{if(m_enabled&&m_socket.state()==QLocalSocket::UnconnectedState)m_socket.connectToServer(m_socketName);});
    connect(&m_poll,&QTimer::timeout,this,[this]{if(m_supported&&m_statusId.isEmpty())m_statusId=send("film_status");else if(m_enabled&&!m_supported&&m_capId.isEmpty())m_capId=send("film_capabilities");});
    connect(&m_debounce,&QTimer::timeout,this,&FilmBackend::search);
    connect(&m_deadlines,&QTimer::timeout,this,[this]{
        QStringList expired;for(auto it=m_pending.cbegin();it!=m_pending.cend();++it)if(m_clock.elapsed()-it->sent>100000)expired<<it.key();
        for(auto id:expired){if(m_pending[id].op=="film_navigation")m_abandonedNavigation.insert(id);m_pending.remove(id);if(m_statusId==id)m_statusId.clear();if(m_searchId==id)m_searchId.clear();if(m_capId==id)m_capId.clear();m_message="Film request timed out; retry or refresh.";emit changed();}
        if(m_abandonedNavigation.size()>64)m_socket.abort();
    });m_deadlines.start();
}
FilmBackend::~FilmBackend(){releaseNavigation();m_socket.flush();m_socket.disconnectFromServer();}
void FilmBackend::setEnabled(bool enabled){
    m_enabled=enabled;
    if(enabled){if(m_socket.state()==QLocalSocket::UnconnectedState)m_socket.connectToServer(m_socketName);m_retry.start();if(m_socket.state()==QLocalSocket::ConnectedState)m_poll.start();if(m_available)search();}
    else {m_poll.stop();m_retry.stop();m_debounce.stop();m_includeUnverifiedPrior=false;invalidate();}
}
QString FilmBackend::send(QString op,QJsonObject args){
    if(m_socket.state()!=QLocalSocket::ConnectedState)return {};
    auto id="film-ui-"+QString::number(++m_sequence);args["op"]=op;args["id"]=id;
    auto bytes=QJsonDocument(args).toJson(QJsonDocument::Compact)+'\n';if(bytes.size()>65536){m_message="Film request is too large.";emit changed();return {};}
    m_pending.insert(id,{op,m_selection,m_clock.elapsed(),args});m_socket.write(bytes);return id;
}
void FilmBackend::releaseNavigation(){if(!m_navigationToken.isEmpty())send("film_release_navigation",{{"token",m_navigationToken}});m_navigationToken.clear();}
void FilmBackend::clearSelection(){
    ++m_selection;releaseNavigation();
    QStringList obsolete;for(auto it=m_pending.cbegin();it!=m_pending.cend();++it)if(it->op=="film_evidence"||it->op=="film_navigation")obsolete<<it.key();
    for(auto id:obsolete){if(m_pending[id].op=="film_navigation")m_abandonedNavigation.insert(id);m_pending.remove(id);send("cancel",{{"target",id}});}
    if(m_abandonedNavigation.size()>64)m_socket.abort();
    m_reference.clear();m_evidence.clear();emit evidenceChanged();
}
void FilmBackend::invalidate(){
    QStringList cancel;for(auto it=m_pending.cbegin();it!=m_pending.cend();++it)if(it->op=="film_search")cancel<<it.key();
    for(auto id:cancel){m_pending.remove(id);send("cancel",{{"target",id}});}
    m_searchId.clear();m_results.clear();clearSelection();emit resultsChanged();emit changed();
}
void FilmBackend::setQuery(QString text){if(text==m_query)return;m_query=text;invalidate();m_debounce.start();}
void FilmBackend::setMode(QString mode){if(mode!= "source"&&mode!="hybrid")return;if(mode==m_mode)return;m_mode=mode;if(mode=="source")m_includeUnverifiedPrior=false;invalidate();m_debounce.start();}
void FilmBackend::setIncludeUnverifiedPrior(bool include){if(include&&m_mode!="hybrid")return;if(include==m_includeUnverifiedPrior)return;m_includeUnverifiedPrior=include;invalidate();m_debounce.start();}
void FilmBackend::selectCollection(QString collection){if(collection==m_collection)return;m_collection=collection;invalidate();m_debounce.start();}
void FilmBackend::search(){
    if(!m_enabled||!m_available||m_query.trimmed().isEmpty())return;
    QJsonObject args{{"text",m_query},{"mode",m_mode},{"limit",20},{"spoiler_policy","hide"},{"include_unverified_prior",m_includeUnverifiedPrior}};if(!m_collection.isEmpty())args["collection_id"]=m_collection;
    m_searchId=send("film_search",args);emit changed();
}
void FilmBackend::addCollection(QUrl url){if(!url.isLocalFile())return;invalidate();send("film_add_collection",{{"path",QDir::cleanPath(url.toLocalFile())}});}
void FilmBackend::importFile(QUrl url,bool analysis){
    QString root;for(auto v:collections()){auto c=v.toMap();if(c["collection_id"].toString()==m_collection)root=c["path"].toString();}
    auto path=url.toLocalFile();if(!url.isLocalFile()||root.isEmpty()||!path.startsWith(root+"/")){m_message="Choose a sidecar within the selected collection.";emit changed();return;}
    auto id=send("film_sidecar_digest",{{"collection_id",m_collection},{"relative_path",QDir(root).relativeFilePath(path)}});
    if(!id.isEmpty())m_pending[id].analysis=analysis;
}
void FilmBackend::removeCollection(){if(m_collection.isEmpty())return;invalidate();send("film_remove_collection",{{"collection_id",m_collection}});m_collection.clear();}
void FilmBackend::setExclusions(QString prefixes){QJsonArray a;for(auto s:prefixes.split('\n',Qt::SkipEmptyParts))a.append(s);invalidate();send("film_set_exclusions",{{"collection_id",m_collection},{"exclusions",a}});}
void FilmBackend::reconcile(){invalidate();QJsonObject args;if(!m_collection.isEmpty())args["collection_id"]=m_collection;send("film_reconcile",args);}
void FilmBackend::cancelJob(QString job){send("film_cancel_job",{{"job_id",job}});}
void FilmBackend::revealEvidence(QVariantMap reference){clearSelection();m_reference=reference;send("film_evidence",{{"reference",QJsonObject::fromVariantMap(reference)},{"expected_film_revision",m_revision},{"spoiler_policy","allow"}});}
void FilmBackend::navigate(QString action){if(m_reference.isEmpty()||m_evidence.isEmpty()||(action!="open"&&action!="reveal"))return;releaseNavigation();send("film_navigation",{{"reference",QJsonObject::fromVariantMap(m_reference)},{"expected_film_revision",m_revision},{"spoiler_policy","allow"},{"action",action}});}
void FilmBackend::receive(){
    m_buffer+=m_socket.readAll();if(m_buffer.size()>1024*1024){m_socket.abort();m_message="Film reply exceeded its limit.";emit changed();return;}
    while(m_buffer.contains('\n')){
        auto n=m_buffer.indexOf('\n');QJsonParseError err;auto doc=QJsonDocument::fromJson(m_buffer.left(n),&err);m_buffer.remove(0,n+1);auto reply=doc.object();
        if(err.error!=QJsonParseError::NoError||reply["protocol"].toInt()!=1){m_socket.abort();return;}
        auto id=reply["id"].toString();
        if(m_abandonedNavigation.remove(id)){auto token=reply["navigation"].toObject()["token"].toString();if(!token.isEmpty())send("film_release_navigation",{{"token",token}});continue;}
        if(!m_pending.contains(id))continue;auto pending=m_pending.take(id);
        if(id==m_statusId)m_statusId.clear();if(id==m_searchId)m_searchId.clear();if(id==m_capId)m_capId.clear();
        if(!reply["ok"].toBool()){
            if(pending.op=="film_capabilities"){m_available=false;m_supported=false;m_poll.stop();m_message="This service does not provide the offline film pilot.";}
            else if(pending.selection==m_selection)m_message=reply["error"].toString("Film operation failed.");
            emit changed();continue;
        }
        if(pending.op=="film_capabilities"){
            const auto priorOption=reply["search_options"].toObject()["include_unverified_prior"].toObject();
            m_supported=reply["capability"]=="film_v1"&&reply["schema_version"].toInt()==1&&reply["ranking_version"]=="semanticlens-film-rrf/2"&&priorOption["type"]=="boolean"&&priorOption["default"].isBool()&&!priorOption["default"].toBool()&&priorOption["modes"].toArray()==QJsonArray{"hybrid"};
            m_available=false;
            if(!m_supported){m_poll.stop();m_message="This service does not provide a compatible offline film pilot.";}
            else {m_message="Checking film collection status…";if(m_statusId.isEmpty())m_statusId=send("film_status");}
        } else if(pending.op=="film_sidecar_digest"){
            auto digest=reply["sha256"].toString();
            if(digest.size()!=64){m_message="Film service returned an invalid sidecar digest.";emit changed();continue;}
            invalidate();send(pending.analysis?"film_import_analysis":"film_import_manifest",{{"collection_id",pending.args["collection_id"]},{"relative_path",pending.args["relative_path"]},{"expected_sha256",digest}});
        } else if(pending.op=="film_status"){
            auto next=reply["status"].toObject();auto revision=next["film_revision"].toString(),epoch=next["film_epoch"].toString();
            bool changedRevision=(!m_revision.isEmpty()&&revision!=m_revision)||(!m_epoch.isEmpty()&&epoch!=m_epoch);
            bool wasAvailable=m_available;m_available=m_supported&&next["ready"].toBool()&&next["model_ready"].toBool();
            m_status=next;m_revision=revision;m_epoch=epoch;
            if(changedRevision||(!m_available&&wasAvailable)){invalidate();if(m_available)m_debounce.start();}
            if(!m_available){m_debounce.stop();m_message=next["error"].toString();if(m_message.isEmpty())m_message="Film pilot unavailable. Check the model and collection status.";}
            else if(!wasAvailable){m_message="Film pilot ready. Choose a collection and import its source manifest.";if(!m_query.trimmed().isEmpty())m_debounce.start();}
            auto error=next["error"].toString();if(!error.isEmpty())m_message=error;
        } else if(pending.op=="film_search"&&pending.selection==m_selection){
            if(!reply["include_unverified_prior"].isBool()||reply["include_unverified_prior"].toBool()!=m_includeUnverifiedPrior){invalidate();m_message="Film service returned an incompatible exploratory-search policy.";emit changed();continue;}
            auto revision=reply["film_revision"].toString(),epoch=reply["film_epoch"].toString();
            if((!m_revision.isEmpty()&&revision!=m_revision)||(!m_epoch.isEmpty()&&epoch!=m_epoch)){m_debounce.start();continue;}
            m_revision=revision;m_epoch=epoch;m_results=reply["results"].toArray().toVariantList();m_message=QString("%1 film editions returned.").arg(m_results.size());emit resultsChanged();
        } else if(pending.op=="film_evidence"&&pending.selection==m_selection){
            if(reply["film_revision"].toString()==m_revision&&reply["film_epoch"].toString()==m_epoch){m_evidence=reply["evidence"].toObject().toVariantMap();emit evidenceChanged();}
        } else if(pending.op=="film_navigation"&&pending.selection==m_selection){
            auto nav=reply["navigation"].toObject();auto path=nav["snapshot_path"].toString();
            const auto expectedPage=QJsonValue::fromVariant(m_evidence.value("page"));
            const bool matchingPage=m_evidence.contains("page")&&nav.contains("page")
                &&(expectedPage.isNull()||(expectedPage.isDouble()&&expectedPage.toInt(-1)>0))
                &&nav["page"]==expectedPage;
            const auto relative=QDir(m_data).relativeFilePath(path);const QFileInfo copy(path),directory(copy.absolutePath());
            const bool ownedCopy=QRegularExpression("^film-extract-[A-Za-z0-9]{6}/input\\.(txt|md|srt|vtt|pdf)$").match(relative).hasMatch()
                &&path==QDir::cleanPath(path)&&copy.isFile()&&!copy.isSymLink()&&!directory.isSymLink()
                &&copy.canonicalFilePath()==path&&directory.canonicalFilePath()==directory.absoluteFilePath();
            if(nav["film_revision"].toString()!=m_revision||nav["film_epoch"].toString()!=m_epoch||!ownedCopy||nav["token"].toString().isEmpty()
                ||nav["action"]!=pending.args["action"]||nav["source_revision"].toString()!=m_reference["source_revision"].toString()
                ||!matchingPage){auto token=nav["token"].toString();if(!token.isEmpty())send("film_release_navigation",{{"token",token}});m_message="Source copy is no longer valid.";emit changed();continue;}
            m_navigationToken=nav["token"].toString();auto action=nav["action"].toString();
            const auto okular=QStandardPaths::findExecutable("okular");
            if(action=="open"&&nav["page"].toInt()>0&&okular.isEmpty()){releaseNavigation();m_message="Okular is required to open the matched PDF page. Show the verified copy in Dolphin to use another viewer.";emit changed();continue;}
            bool opened=action=="reveal"?QProcess::startDetached("dolphin",{"--select",path}):nav["page"].toInt()>0?QProcess::startDetached(okular,{"--page",QString::number(nav["page"].toInt()),path}):QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            if(!opened)releaseNavigation();
            m_message=opened?"Requested opening the verified source copy.":"No application could open the source copy.";
        } else if(pending.op=="film_add_collection"){m_collection=reply["collection_id"].toString();m_statusId=send("film_status");}
        else if(reply["accepted"].toBool()){m_message=reply.contains("job_id")?"Import queued; check job progress below.":"Collection updated.";if(m_statusId.isEmpty())m_statusId=send("film_status");}
        emit changed();
    }
}
