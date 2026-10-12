// Native RunnerManager fixture. Serialized rows are synthetic, never target proof.
#include <KRunner/RunnerManager>
#include <KRunner/QueryMatch>
#include <KPluginMetaData>
#include <QPluginLoader>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QTextDocument>
#include <QRegularExpression>
#include <QUrl>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
struct Case { QString name, title, passage, kind, path; bool error=false; int page=0; };
QJsonObject classify(const QString &s) {
    QString tooltip=s; tooltip.remove(QRegularExpression("&(?=\\S)"));
    return {{"text",s},{"mightBeRichText",Qt::mightBeRichText(s)},
            {"stock_tooltip_text",tooltip},{"stock_tooltip_mightBeRichText",Qt::mightBeRichText(tooltip)}};
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); if(argc!=2) return 2;
    QPluginLoader loader(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath());
    KPluginMetaData meta(loader.metaData().value("MetaData").toObject(),loader.fileName());
    QList<Case> cases{
        {"normal-id","notes-IDENTIFIER-77331.txt","IDENTIFIER-77331 ordinary passage","text","/tmp/notes-IDENTIFIER-77331.txt"},
        {"title-tag","<b>TITLE-77331.txt","normal passage","text","/tmp/<b>TITLE-77331.txt"},
        {"passage-image","passage.txt","DESKTOP-77331 <img src=\"http://127.0.0.1:43573/passage\" /> tail","text","/tmp/passage.txt"},
        {"doctype","doctype.txt","<!DOCTYPE html><img src=x>","text","/tmp/doctype.txt"},
        {"xml","xml.txt","<?xml version='1.0'?><img src=x>","text","/tmp/xml.txt"},
        {"literal-entities","&lt;entity&gt;.txt","&lt;img src=x&gt; &amp; &#60;b&#62;","text","/tmp/&lt;entity&gt;.txt"},
        {"ordinary-ampersands","R&D A & B.txt","R&D A & B && x&","text","/tmp/R&D A & B.txt"},
        {"pdf","PDF <b>&.pdf","<img src=x> page passage &lt;","pdf","/tmp/PDF <b>&.pdf",false,3},
        {"path-fallback","image.png","","image","/tmp/<b>&lt;image&gt;.png"},
        {"error","","cannot decode <img src=x> &lt;source&gt; R&D","text","",true},
        {"clipping","title "+QString(200,'x')+"<img src=x>","<img src=x> "+QString(300,'y'),"text","/tmp/clip.txt"},
    };
    for(const auto &c:cases) {
        const auto name=QString("runner-privacy-%1-%2").arg(app.applicationPid()).arg(c.name);
        qputenv("KDE_INDEX_SOCKET",name.toUtf8());
        std::mutex mutex; std::condition_variable cv; bool ready=false,closed=false;
        QJsonObject request,response;
        std::thread service([&]{
            QLocalServer server; if(!server.listen(name)) std::abort();
            {std::lock_guard<std::mutex> lock(mutex);ready=true;} cv.notify_one();
            if(!server.waitForNewConnection(3000)) return;
            auto *peer=server.nextPendingConnection(); QByteArray line;
            while(!line.contains('\n')&&peer->waitForReadyRead(1000)) line+=peer->readAll();
            request=QJsonDocument::fromJson(line).object();
            QJsonObject raw{{"path",c.path},{"title",c.title},{"text",c.passage},{"kind",c.kind},{"page",c.page},{"score",0.25}};
            response={{"protocol",1},{"id",request.value("id")},{"ok",!c.error}};
            if(c.error) response["error"]=c.passage; else response["results"]=QJsonArray{raw};
            peer->write(QJsonDocument(response).toJson(QJsonDocument::Compact)+'\n'); peer->waitForBytesWritten(1000);
            if(peer->state()!=QLocalSocket::UnconnectedState) peer->waitForDisconnected(3000);
            closed=peer->state()==QLocalSocket::UnconnectedState;server.close();
        });
        {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return ready;});}
        QJsonArray matches;
        {
            KRunner::RunnerManager manager; auto *runner=manager.loadRunner(meta); if(!runner)return 3;
            QEventLoop loop; QTimer guard;guard.setSingleShot(true);guard.setInterval(4000);
            QObject::connect(&guard,&QTimer::timeout,&loop,&QEventLoop::quit);
            QObject::connect(&manager,&KRunner::RunnerManager::queryFinished,&loop,&QEventLoop::quit);
            guard.start();manager.launchQuery("find IDENTIFIER-77331");loop.exec();
            for(const auto &m:manager.matches()) {
                QJsonArray urls;for(const auto &u:m.urls())urls.append(QJsonObject{{"url",u.toString(QUrl::FullyEncoded)},{"local_file",u.toLocalFile()}});
                matches.append(QJsonObject{{"id",m.id()},{"title",classify(m.text())},{"subtext",classify(m.subtext())},
                    {"data",QJsonObject::fromVariantMap(m.data().toMap())},{"urls",urls},{"relevance",m.relevance()}});
            }
        }
        service.join();QLocalServer::removeServer(name);
        QJsonObject out{{"case",c.name},{"qt_version",qVersion()},{"request",request},{"response",response},{"matches",matches},
            {"raw_title",classify(c.title)},{"raw_passage",classify(c.passage)},{"raw_path",classify(c.path)},{"peer_observed_close",closed}};
        std::cout<<QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString()<<std::endl;
    }
}
