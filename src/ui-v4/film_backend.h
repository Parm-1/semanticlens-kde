#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QHash>
#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
#include <QVariantList>

// A separate client namespace: film results never enter legacy file/media state.
class FilmBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool searching READ searching NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(bool includeUnverifiedPrior READ includeUnverifiedPrior WRITE setIncludeUnverifiedPrior NOTIFY changed)
    Q_PROPERTY(QString collectionId READ collectionId NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY changed)
    Q_PROPERTY(QVariantList warnings READ warnings NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap evidence READ evidence NOTIFY evidenceChanged)
public:
    FilmBackend(QString socketName, QString dataDirectory, QObject *parent=nullptr);
    ~FilmBackend() override;
    bool available() const { return m_available; }
    bool searching() const { return m_debounce.isActive() || !m_searchId.isEmpty(); }
    QString message() const { return m_message; }
    QString query() const { return m_query; }
    QString mode() const { return m_mode; }
    bool includeUnverifiedPrior() const { return m_includeUnverifiedPrior; }
    QString collectionId() const { return m_collection; }
    QVariantList collections() const { return m_status.value("collections").toArray().toVariantList(); }
    QVariantList jobs() const { return m_status.value("jobs").toArray().toVariantList(); }
    QVariantList warnings() const {
        auto result=m_status.value("warnings").toArray().toVariantList();
        const auto watchError=m_status.value("watch_error").toString();
        if(!watchError.isEmpty())result.append("Automatic change monitoring is incomplete: "+watchError);
        return result;
    }
    QVariantList results() const { return m_results; }
    QVariantMap evidence() const { return m_evidence; }
    void setQuery(QString text);
    void setMode(QString mode);
    void setIncludeUnverifiedPrior(bool include);
    Q_INVOKABLE void setEnabled(bool enabled);
    Q_INVOKABLE void selectCollection(QString collection);
    Q_INVOKABLE void addCollection(QUrl url);
    Q_INVOKABLE void importFile(QUrl url, bool analysis);
    Q_INVOKABLE void removeCollection();
    Q_INVOKABLE void setExclusions(QString prefixes);
    Q_INVOKABLE void reconcile();
    Q_INVOKABLE void cancelJob(QString job);
    Q_INVOKABLE void revealEvidence(QVariantMap reference);
    Q_INVOKABLE void navigate(QString action);
    Q_INVOKABLE void clearSelection();
signals:
    void changed();
    void resultsChanged();
    void evidenceChanged();
private:
    QString m_socketName,m_data,m_query,m_mode="hybrid",m_collection,m_revision,m_epoch;
    QString m_message="Film pilot is not connected.",m_searchId,m_statusId,m_capId,m_navigationToken;
    QLocalSocket m_socket;
    QTimer m_poll,m_retry,m_debounce,m_deadlines;
    QByteArray m_buffer;
    QJsonObject m_status;
    QVariantList m_results;
    QVariantMap m_evidence,m_reference;
    quint64 m_sequence=0,m_selection=0;
    bool m_available=false,m_supported=false,m_enabled=false;
    bool m_includeUnverifiedPrior=false;
    QElapsedTimer m_clock;
    QSet<QString> m_abandonedNavigation;
    struct Pending { QString op; quint64 selection; qint64 sent; QJsonObject args; bool analysis=false; };
    QHash<QString,Pending> m_pending;
    QString send(QString op,QJsonObject args={});
    void receive();
    void invalidate();
    void search();
    void releaseNavigation();
};
