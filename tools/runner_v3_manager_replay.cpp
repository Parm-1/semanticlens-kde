#include <KRunner/RunnerManager>
#include <KRunner/AbstractRunner>
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
#include <QThread>
#include <QFileInfo>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>

using Clock = std::chrono::steady_clock;
qint64 nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count(); }
struct Case { QString name; QString query; QString mode; };

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if(argc != 2) return 2;
    QPluginLoader loader(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath());
    KPluginMetaData meta(loader.metaData().value("MetaData").toObject(), loader.fileName());
    QList<Case> cases{
        {"positive-text", "find storm shelter", "text"},
        {"positive-pdf-fragmented", "find storm shelter", "pdf"},
        {"positive-image-empty-passage", "find storm shelter", "image"},
        {"score-endpoints", "find storm shelter", "scores"},
        {"wrong-score-string", "find storm shelter", "string-score"},
        {"missing-score", "find storm shelter", "missing-score"},
        {"invalid-kind-page", "find storm shelter", "invalid-kind"},
        {"fractional-pdf-page", "find storm shelter", "fractional-pdf-page"},
        {"zero-pdf-page", "find storm shelter", "zero-pdf-page"},
        {"nonzero-text-page", "find storm shelter", "nonzero-text-page"},
        {"wrong-title-type", "find storm shelter", "wrong-title-type"},
        {"missing-title", "find storm shelter", "missing-title"},
        {"missing-page", "find storm shelter", "missing-page"},
        {"wrong-kind-type", "find storm shelter", "wrong-kind-type"},
        {"missing-text", "find storm shelter", "missing-text"},
        {"empty-text", "find storm shelter", "empty-text"},
        {"wrong-path-type", "find storm shelter", "wrong-path-type"},
        {"relative-path", "find storm shelter", "relative"},
        {"wrong-id", "find storm shelter", "wrong-id"},
        {"wrong-protocol", "find storm shelter", "wrong-protocol"},
        {"server-error", "find storm shelter", "error"},
        {"connected-timeout", "find storm shelter", "timeout"},
        {"context-reset", "find storm shelter", "cancel"},
        {"unavailable", "find storm shelter", "unavailable"},
        {"uppercase-trigger", "Find storm shelter", "text"},
        {"unprefixed", "storm shelter", "text"}
    };
    for(const auto &c: cases) {
        QString name = QString("kde-index-v3-review-%1-%2").arg(app.applicationPid()).arg(c.name);
        qputenv("KDE_INDEX_SOCKET", name.toUtf8());
        std::mutex mutex; std::condition_variable readyCv; bool ready = false;
        std::atomic<bool> received{false}, connected{false}, closed{false};
        std::atomic<qint64> closeTs{0};
        QJsonObject request, response;
        std::thread fixture([&]{
            QLocalServer server;
            QLocalServer::removeServer(name);
            bool listen = c.mode != "unavailable" && server.listen(name);
            { std::lock_guard<std::mutex> lock(mutex); ready = true; } readyCv.notify_one();
            if(!listen || !server.waitForNewConnection(700)) return;
            auto *peer = server.nextPendingConnection(); connected = true;
            QByteArray line;
            while(!line.contains('\n') && peer->waitForReadyRead(500)) line += peer->readAll();
            request = QJsonDocument::fromJson(line).object(); received = true;
            QJsonObject row{{"path", "/tmp/kde-index-v3-review-source.txt"},{"title", "Storm shelter plan"},
                {"text", "Use the east stairwell during severe weather."},{"kind", "text"},{"page", 0},
                {"score", 0.25},{"semantic_score", 0.25},{"exact", false}};
            if(c.mode == "pdf") { row["path"]="/tmp/kde-index-v3-review-source.pdf"; row["kind"]="pdf"; row["page"]=3; }
            if(c.mode == "image") { row["path"]="/tmp/kde-index-v3-review-source.png"; row["kind"]="image"; row["text"]=""; }
            if(c.mode == "string-score") row["score"]="bad";
            if(c.mode == "missing-score") row.remove("score");
            if(c.mode == "invalid-kind") { row["kind"]="executable"; row["page"]=-3; }
            if(c.mode == "relative") row["path"]="relative.txt";
            if(c.mode == "fractional-pdf-page") { row["kind"]="pdf"; row["page"]=3.5; }
            if(c.mode == "zero-pdf-page") { row["kind"]="pdf"; row["page"]=0; }
            if(c.mode == "nonzero-text-page") row["page"]=1;
            if(c.mode == "wrong-title-type") row["title"]=17;
            if(c.mode == "missing-title") row.remove("title");
            if(c.mode == "missing-page") row.remove("page");
            if(c.mode == "wrong-kind-type") row["kind"]=17;
            if(c.mode == "missing-text") row.remove("text");
            if(c.mode == "empty-text") row["text"]="";
            if(c.mode == "wrong-path-type") row["path"]=17;
            QJsonArray rows{row};
            if(c.mode == "scores") { rows={}; for(double score: {-1.0,0.0,1.0,1.01}) { auto r=row; r["score"]=score; r["semantic_score"]=score; r["path"]=QString("/tmp/score-%1.txt").arg(score); if(score==1.0) {r["exact"]=true; r["semantic_score"]=-0.5;} rows.append(r); } }
            response={{"protocol",1},{"id",request.value("id")},{"ok",true},{"results",rows},
                {"identity", QJsonObject{{"backend","CPU"},{"dimension",768}}},{"generation",1}};
            if(c.mode == "wrong-id") response["id"]="not-the-request";
            if(c.mode == "wrong-protocol") response["protocol"]=2;
            if(c.mode == "error") { response={{"protocol",1},{"id",request.value("id")},{"ok",false},{"error","wrong vector dimension"}}; }
            if(c.mode != "timeout" && c.mode != "cancel") {
                QByteArray bytes=QJsonDocument(response).toJson(QJsonDocument::Compact)+'\n';
                if(c.mode == "pdf") { peer->write(bytes.left(30)); peer->waitForBytesWritten(500); std::this_thread::sleep_for(std::chrono::milliseconds(10)); peer->write(bytes.mid(30)); }
                else peer->write(bytes);
                peer->waitForBytesWritten(500);
            }
            if(peer->state()!=QLocalSocket::UnconnectedState) peer->waitForDisconnected(10000);
            closed=peer->state()==QLocalSocket::UnconnectedState;
            closeTs=nowMs();
            server.close();
        });
        {std::unique_lock<std::mutex> lock(mutex); readyCv.wait(lock,[&]{return ready;});}
        QJsonObject out{{"case",c.name},{"mode",c.mode},{"query",c.query}};
        qint64 start=nowMs(), reset=0; int heartbeats=0;
        {
            KRunner::RunnerManager manager;
            auto *runner=manager.loadRunner(meta);
            if(!runner) return 3;
            out["runner_on_main_thread"]=runner->thread()==QThread::currentThread();
            QEventLoop loop; QTimer guard, heartbeat, cancel;
            guard.setSingleShot(true); guard.setInterval(12000);
            QObject::connect(&guard,&QTimer::timeout,&loop,&QEventLoop::quit);
            QObject::connect(&manager,&KRunner::RunnerManager::queryFinished,&loop,&QEventLoop::quit);
            QObject::connect(&heartbeat,&QTimer::timeout,&loop,[&]{heartbeats++;}); heartbeat.start(10);
            if(c.mode=="cancel") { QObject::connect(&cancel,&QTimer::timeout,&loop,[&]{if(received){reset=nowMs(); manager.reset();cancel.stop();}}); cancel.start(5); }
            guard.start(); manager.launchQuery(c.query); loop.exec();
            if(c.mode=="cancel") { QEventLoop afterReset; QTimer::singleShot(100,&afterReset,&QEventLoop::quit); afterReset.exec(); }
            out["match_return_ms"]=double(nowMs()-start); out["main_heartbeats"]=heartbeats;
            QJsonArray matches;
            for(const auto &match: manager.matches()) matches.append(QJsonObject{{"id",match.id()},{"text",match.text()},{"subtext",match.subtext()},{"relevance",match.relevance()},{"data",QJsonObject::fromVariantMap(match.data().toMap())}});
            out["matches"]=matches;
        }
        fixture.join(); QLocalServer::removeServer(name);
        out["request"]=request; if(c.mode!="timeout"&&c.mode!="cancel"&&connected)out["response"]=response;
        out["connected"]=connected.load(); out["received"]=received.load(); out["peer_observed_close"]=closed.load();
        if(reset)out["reset_to_peer_close_ms"]=double(closeTs.load()-reset);
        std::cout<<QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString()<<std::endl;
    }
}
