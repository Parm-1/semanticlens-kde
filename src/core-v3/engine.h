#pragma once
#include <QtCore>
#include <QtSql>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
struct EngineOptions {
    QString data, probe, textModel, imageModel;
    int threads = 6;
    QString probeSha;
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
    QStringList watchPaths();
    quint64 currentGeneration() const { return generation.load(); }
private:
    struct Task { QJsonObject request; QString owner; Reply reply; quint64 generation; };
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
    std::atomic<quint64> generation{1};
    std::atomic<bool> stop{false}, paused{false};
    QSqlDatabase db;
    std::unique_ptr<QProcess> producer;
    QByteArray producerBuffer;
    struct Scan { QStringList files; QSet<QString> seen; int position=0; quint64 generation=0; } scan;
    void loop();
    void initialize();
    void openDatabase();
    void closeDatabase();
    void setFailure(QString error);
    QJsonObject extractorIdentity();
    void verifyExtractors();
    void handle(Task task);
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
    bool valid(const Task &task);
    bool validLocked(const Task &task);
    QSqlQuery sql(QString statement, QVariantList values={});
    QJsonArray roots();
    bool allowed(QString path);
    bool eligible(QString path);
    QString rootFor(QString path);
};
