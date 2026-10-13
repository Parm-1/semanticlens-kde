#pragma once
#include <QtCore>
#include <QtSql>
#include <functional>
#include <memory>
struct FilmLease {
    QString path, copy, sha;
    QByteArray bytes;
    std::shared_ptr<QTemporaryDir> directory;
    std::function<void()> reauthorize;
};
struct FilmCallbacks {
    std::function<FilmLease(QString,QJsonArray,QString,QString,std::function<bool()>)> snapshot;
    std::function<void(QString)> admitRoot;
    std::function<int(QByteArray,std::function<bool()>)> count;
    std::function<QByteArray(QString,std::function<bool()>)> query;
    std::function<QString(QByteArray,std::function<bool()>)> begin;
    std::function<QJsonObject(QString,std::function<bool()>)> poll;
    // Discard only marks intent; no producer I/O inside linearize/promote.
    std::function<void(QString)> discard;
    std::function<QJsonObject()> workerCapabilities;
    std::function<bool()> stopping, paused;
    std::function<bool(QString)> ownerAlive;
    std::function<quint64()> epoch;
    std::function<void()> bump;
    std::function<void(std::function<void()>)> linearize;
    // registerJob runs only inside linearize; promote guards owner/cancel/epoch.
    std::function<void(QString,QString)> registerJob;
    std::function<bool(QString,QString)> jobCanceled;
    std::function<void(QString,QString,quint64,std::function<void()>)> promote;
};
class FilmStore {
public:
    FilmStore(QString data, FilmCallbacks callbacks);
    ~FilmStore();
    void initialize(QJsonObject identity, QString unavailable);
    QJsonObject handle(QJsonObject request, QString owner, std::function<bool()> valid);
    void step();
    void disconnect(QString owner);
    QJsonObject status();
    QStringList watchPaths();
    bool pending() const;
    QSet<QString> activeJobs() const;
private:
    struct Job;
    QString data, error;
    FilmCallbacks cb;
    QSqlDatabase db;
    QJsonObject identity;
    bool ready=false;
    QHash<QString,std::shared_ptr<Job>> jobs;
    QList<QString> order;
    struct Navigation {QString owner;std::shared_ptr<QTemporaryDir> directory;};
    QHash<QString,Navigation> navigation;
    QSqlQuery sql(QString text,QVariantList values={});
    QString revision();
    void advance();
    QJsonObject collection(QString id);
    FilmLease snapshot(QString collection,QString relative,QString sha,std::function<bool()> valid);
    QString newJob(QString kind,QJsonObject request,QString owner);
    void finish(Job &job,QString state,QString message={});
    void promote(Job &job);
    void reconcile(QString collection,std::function<bool()> valid);
    void withdraw(QString gid,QString source={});
    QJsonObject search(QJsonObject request,std::function<bool()> valid);
    QJsonObject evidence(QJsonObject reference,QString expected,bool reveal,std::function<bool()> valid);
    void validateAnalysis(QJsonObject analysis,QString gid);
};
