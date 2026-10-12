#pragma once
#include <QObject>
#include <QJsonArray>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QJsonObject>
#include <QTemporaryDir>

struct AppPaths {
    QString socket, service, probe, models, data, downloader;
};

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool modelReady READ modelReady NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(int downloadPercent READ downloadPercent NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY changed)
    Q_PROPERTY(QVariantList roots READ roots NOTIFY changed)
    Q_PROPERTY(QVariantList failures READ failures NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY previewChanged)
    Q_PROPERTY(QString previewMessage READ previewMessage NOTIFY previewChanged)
    Q_PROPERTY(int indexed READ indexed NOTIFY changed)
    Q_PROPERTY(int errors READ errors NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
public:
    explicit Backend(AppPaths paths, QObject *parent = nullptr);
    ~Backend() override;
    bool connected() const { return m_socket.state() == QLocalSocket::ConnectedState; }
    bool modelReady() const { return m_status.value("model_ready").toBool(); }
    bool busy() const { return m_status.value("busy").toBool(); }
    bool paused() const { return m_status.value("paused").toBool(); }
    bool downloading() const { return m_download.state() != QProcess::NotRunning; }
    int downloadPercent() const { return m_downloadPercent; }
    QString message() const { return m_message; }
    QString query() const { return m_query; }
    QString kind() const { return m_kind; }
    QVariantList roots() const { return m_status.value("roots").toArray().toVariantList(); }
    QVariantList failures() const { return m_status.value("failures").toArray().toVariantList(); }
    QVariantList results() const { return m_results; }
    QUrl previewUrl() const { return m_previewUrl; }
    QString previewMessage() const { return m_previewMessage; }
    int indexed() const { return m_status.value("indexed").toInt(); }
    int errors() const { return m_status.value("errors").toInt(); }
    int progress() const { return m_status.value("progress").toInt(); }
    int total() const { return m_status.value("total").toInt(); }
    void setQuery(const QString &text);
    void setKind(const QString &kind);
    Q_INVOKABLE void addRoot(const QUrl &url);
    Q_INVOKABLE void removeRoot(const QString &path);
    Q_INVOKABLE void setExclusions(const QString &path, const QString &text);
    Q_INVOKABLE void scan();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void wipe();
    Q_INVOKABLE void downloadModel();
    Q_INVOKABLE QUrl localUrl(const QString &path) const;
    Q_INVOKABLE void openResult(const QVariantMap &result);
    Q_INVOKABLE void reveal(const QString &path);
    Q_INVOKABLE void previewResult(const QVariantMap &result);
    Q_INVOKABLE void closePreview();
    void shutdownOwnedService();
signals:
    void changed();
    void resultsChanged();
    void previewChanged();
private:
    AppPaths m_paths;
    QLocalSocket m_socket;
    QProcess m_download;
    QProcess m_preview;
    QTemporaryDir m_previewDirectory;
    QTimer m_previewTimeout;
    QUrl m_previewUrl;
    QString m_previewMessage, m_previewOutput;
    quint64 m_previewSequence = 0;
    QTimer m_poll, m_debounce, m_retry;
    QJsonObject m_status;
    QVariantList m_results;
    QByteArray m_buffer, m_downloadBuffer;
    QString m_message, m_query, m_kind, m_searchId, m_statusId;
    quint64 m_sequence = 0;
    int m_downloadPercent = 0, m_launchAttempts = 0;
    bool m_startedService = false;
    QString send(const QString &op, QJsonObject args = {});
    void connectService();
    void receive();
    void search();
    void invalidateResults();
    void control(const QString &op, QJsonObject args = {});
};
