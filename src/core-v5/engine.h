#pragma once
#include <QtCore>
#include <QtSql>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include "../film-v1/store.h"
struct EngineOptions {
    QString data, probe, textModel, imageModel;
    int threads = 6;
    QString probeSha;
    QString filmTokenizer, filmTokenizerSha, filmWorker, filmWorkerSha;
    quint64 scanEntryLimit = 100000, scanDirectoryLimit = 10000, scanWatchLimit = 10000;
    quint64 scanPathBytesLimit = 16*1024*1024;
};
class Engine {
public:
    using Reply = std::function<void(QJsonObject)>;
    Engine(EngineOptions options);
    ~Engine();
    void submit(QJsonObject request, QString owner, Reply reply);
    void disconnect(QString owner);
    QJsonObject status();
    QJsonObject filmStatus();
    quint64 currentFilmEpoch() const { return filmEpoch.load(); }
    void filmChanged();
    QStringList watchPaths();
    quint64 currentGeneration() const { return generation.load(); }
private:
    struct Task { QJsonObject request; QString owner; Reply reply; quint64 generation; quint64 filmEpoch; bool filmCancelAccepted=false; };
    struct RequestFrame {QString owner,id;bool committed=false;Task *filmTask=nullptr;};
    EngineOptions opt;
    std::thread thread;
    std::mutex mutex, snapshotMutex;
    std::condition_variable wake;
    QString activeOwner, activeId;
    bool activeCommitted = false, operational = false;
    QStringList watched;
    std::deque<Task> queue;
    QSet<QString> canceled, disconnected;
    QJsonObject snapshot, identity;
    QJsonObject filmSnapshot;
    QStringList filmWatched;
    QSet<QString> filmDisconnected;
    QHash<QString,QString> filmJobOwners;
    QSet<QString> filmJobCanceled;
    std::unique_ptr<FilmStore> film;
    Task *filmTask=nullptr;
    std::unique_ptr<QProcess> tokenizer;
    QByteArray tokenizerBuffer;
    std::atomic<quint64> filmEpoch{1};
    std::atomic<quint64> generation{1};
    std::atomic<bool> stop{false}, paused{false};
    QSqlDatabase db;
    std::unique_ptr<QProcess> producer;
    QByteArray producerBuffer;
    std::unique_ptr<QProcess> filmWorker;
    QByteArray filmWorkerBuffer;
    QJsonObject filmWorkerInfo, legacyProducerInfo, currentQueryProvider, lastQueryProvider;
    QString filmWorkerError, filmTicket;
    QByteArray filmTicketVector;
    QString filmTicketError;
    bool filmTicketReady=false, filmTicketCanceled=false;
    void ensureFilmWorker();
    QJsonObject workerCall(QJsonObject request);
    void closeFilmWorker(bool drain=true);
    void workerFailure(QString error);
    void pollFilmTicket();
    void drainFilmTicket();
    QString beginFilmVector(QByteArray bytes,std::function<bool()> valid);
    QJsonObject pollFilmVector(QString ticket,std::function<bool()> valid);
    void discardFilmVector(QString ticket);
    QByteArray queryVector(QString text,bool legacy,std::function<bool()> valid);
    QJsonObject queryProvenance(bool shared,bool legacy) const;
    QJsonObject workerCapabilities() const;
    void recordQueryProvider();
    void refreshQueryStatus();
    bool compatibleWorkerQueries() const;

    struct Scan { QStringList files; QSet<QString> seen; int position=0; quint64 generation=0; } scan;
    void loop();
    void initialize();
    void openDatabase();
    void closeDatabase();
    void setFailure(QString error);
    QJsonObject extractorIdentity();
    void verifyExtractors();
    void handle(Task task,RequestFrame parent);
    void servicePending();
    void commitControl(Task &task, const std::function<void()> &changes);
    QJsonObject wipe(Task &task);
    void startScan(Task *task=nullptr);
    void scanStep();
    void indexFile(QString path, quint64 gen);
    void refresh();
    void purgeInadmissibleRows();
    void closeProducer();
    QJsonObject embed(QJsonObject request);
    QJsonObject tokenize(QJsonObject request);
    int filmCount(QByteArray text, std::function<bool()> valid);
    void initializeFilm();
    void refreshFilm();
    void closeTokenizer();
    bool valid(const Task &task);
    bool validLocked(const Task &task);
    QSqlQuery sql(QString statement, QVariantList values={});
    QJsonArray roots();
    bool allowed(QString path);
    bool eligible(QString path);
    bool definitelyExcluded(QString path);
    QString rootFor(QString path);
};
