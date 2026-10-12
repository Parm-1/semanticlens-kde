#include "backend.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QFile>
#include <QLockFile>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

Backend::Backend(AppPaths paths, QObject *parent) : QObject(parent), m_paths(std::move(paths)) {
    m_previewTimeout.setSingleShot(true);
    connect(&m_previewTimeout, &QTimer::timeout, this, [this] {
        m_preview.kill(); m_previewMessage = "Page preview timed out."; emit previewChanged();
    });
    connect(&m_preview, &QProcess::finished, this, [this](int code, auto state) {
        m_previewTimeout.stop();
        if (code == 0 && state == QProcess::NormalExit && QFileInfo::exists(m_previewOutput)) {
            m_previewUrl = QUrl::fromLocalFile(m_previewOutput); m_previewMessage.clear();
        } else if (m_previewMessage == "Rendering page…") m_previewMessage = "Page preview unavailable. The extracted passage is shown below.";
        emit previewChanged();
    });
    connect(&m_preview, &QProcess::errorOccurred, this, [this](auto) {
        m_previewTimeout.stop(); m_previewMessage = "Page preview unavailable. Check that pdftoppm is installed."; emit previewChanged();
    });
    m_poll.setInterval(1000);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    m_retry.setSingleShot(true);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (connected() && m_statusId.isEmpty()) m_statusId = send("status");
    });
    connect(&m_debounce, &QTimer::timeout, this, &Backend::search);
    connect(&m_retry, &QTimer::timeout, this, &Backend::connectService);
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        m_launchAttempts = 0; m_serviceLaunches = 0; m_retry.stop(); m_statusId = send("status"); m_poll.start(); emit changed();
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, &Backend::receive);
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        m_poll.stop(); m_statusId.clear(); m_buffer.clear(); m_status = {}; invalidateResults();
        m_message = "Search service disconnected. Reconnecting…"; emit changed(); if (!m_stoppingOwnedService) m_retry.start(1000);
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](auto) {
        if (!connected() && !m_stoppingOwnedService) {
            if (m_launchAttempts < 12) m_retry.start(500);
            else { m_message = "Unable to connect to the search service. Use Retry search to try again."; emit changed(); }
        }
    });
    connect(&m_download, &QProcess::readyReadStandardOutput, this, [this] {
        m_downloadBuffer += m_download.readAllStandardOutput();
        while (m_downloadBuffer.contains('\n')) {
            const auto split = m_downloadBuffer.indexOf('\n');
            const auto row = QJsonDocument::fromJson(m_downloadBuffer.left(split)).object();
            m_downloadBuffer.remove(0, split + 1);
            const auto total = row.value("total").toDouble();
            if (total > 0) m_downloadPercent = int(100 * row.value("received").toDouble() / total);
            m_message = row.value("state").toString() == "error" ? row.value("error").toString()
                         : QString("Downloading search model… %1%").arg(m_downloadPercent);
        }
        emit changed();
    });
    connect(&m_download, &QProcess::finished, this, [this](int exit, auto) {
        if (exit == 0) {
            m_message = "Model files verified.";
            retryService();
        }
        else if (!m_message.contains("failed", Qt::CaseInsensitive)) m_message = "Model setup failed. You can retry the download.";
        emit changed();
    });
    connect(&m_download, &QProcess::errorOccurred, this, [this](auto) {
        m_message = "Unable to start model setup: " + m_download.errorString(); emit changed();
    });
    QTimer::singleShot(0, this, &Backend::connectService);
}

Backend::~Backend() {
    // Member destructors can emit signals after later members have been destroyed.
    // Disconnect while every callback's state is still alive.
    m_socket.disconnect(this);
    m_download.disconnect(this);
    m_preview.disconnect(this);
    m_poll.stop(); m_debounce.stop(); m_retry.stop(); m_previewTimeout.stop();
    m_socket.abort();
    if (m_download.state() != QProcess::NotRunning) {
        m_download.kill(); m_download.waitForFinished(1000);
    }
    closePreview();
}

// Linux process start ticks bind ownership across PID reuse. Zombies have exited.
static QByteArray processStart(qint64 pid) {
    if (pid <= 0) return {};
    QFile stat(QString("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) return {};
    const auto row = stat.readAll();
    const auto fields = row.mid(row.lastIndexOf(')') + 2).simplified().split(' ');
    if (fields.size() <= 19 || fields[0] == "Z" || fields[0] == "X") return {};
    return fields[19]; // field 22; field 3 is the first after the process name.
}
bool Backend::ownedServiceAlive() const {
    return !m_ownedServiceStart.isEmpty() && processStart(m_ownedServicePid) == m_ownedServiceStart;
}
bool Backend::connectedToOwnedService() const {
    if (!connected() || !ownedServiceAlive()) return false;
    struct ucred peer {};
    socklen_t size = sizeof(peer);
    return ::getsockopt(int(m_socket.socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0
           && peer.uid == getuid() && peer.pid == m_ownedServicePid;
}
bool Backend::modelSetupNeeded() const {
    return !QFileInfo::exists(QDir(m_paths.models).filePath("embeddinggemma-2-Q8_0.gguf"))
        || !QFileInfo::exists(QDir(m_paths.models).filePath("mmproj-embeddinggemma-2-Q8_0.gguf"));
}
void Backend::connectService() {
    if (m_stoppingOwnedService || connected() || m_socket.state() == QLocalSocket::ConnectingState) return;
    if (m_launchAttempts >= 12) {
        m_message = "Unable to connect to the search service. Use Retry search to try again."; emit changed(); return;
    }
    // Always attempt the common socket first. Launch only following a failed attempt,
    // with no live owned process or live owner of this data directory.
    const bool unknownLiveLaunch = m_ownedServicePid > 0 && m_ownedServiceStart.isEmpty()
                                   && (::kill(pid_t(m_ownedServicePid), 0) == 0 || errno == EPERM);
    if (m_launchAttempts > 0 && !ownedServiceAlive() && !unknownLiveLaunch && m_serviceLaunches < 2) {
        QLockFile lock(QDir(m_paths.data).filePath("service.lock"));
        qint64 lockPid = 0; QString host, application;
        const bool dataOwned = lock.getLockInfo(&lockPid, &host, &application)
                               && lockPid > 0 && (::kill(pid_t(lockPid), 0) == 0 || errno == EPERM);
        if (!dataOwned) {
            if (modelSetupNeeded()) {
                m_message = "Download the search model to begin. Your files and searches stay on this device."; emit changed(); return;
            }
            QFile producer(m_paths.probe);
            QCryptographicHash producerHash(QCryptographicHash::Sha256);
            if (!producer.open(QIODevice::ReadOnly) || !producerHash.addData(&producer)) {
                m_message = "Unable to read the embedding executable. Check the installation."; emit changed(); return;
            }
            const QStringList args {"--data-dir", m_paths.data, "--socket", m_paths.socket, "--probe", m_paths.probe,
                                    "--probe-sha256", QString::fromLatin1(producerHash.result().toHex()),
                                    "--text-model", QDir(m_paths.models).filePath("embeddinggemma-2-Q8_0.gguf"),
                                    "--image-model", QDir(m_paths.models).filePath("mmproj-embeddinggemma-2-Q8_0.gguf")};
            ++m_serviceLaunches;
            qint64 pid = 0;
            if (!QProcess::startDetached(m_paths.service, args, QString(), &pid)) {
                m_message = "Unable to start the search service. Check the installation."; emit changed(); return;
            }
            m_ownedServicePid = pid; m_ownedServiceStart = processStart(pid);
        }
    }
    ++m_launchAttempts;
    m_socket.abort(); m_socket.connectToServer(m_paths.socket);
}
void Backend::retryService() {
    if (connected()) {
        m_message = "Retrying search service verification…";
        control("scan"); emit changed();
    } else {
        m_stoppingOwnedService = false; m_launchAttempts = 0; m_serviceLaunches = 0;
        m_retry.stop(); connectService();
    }
}

QString Backend::send(const QString &op, QJsonObject args) {
    if (!connected()) return {};
    const auto id = "ui-" + QString::number(++m_sequence);
    args.insert("id", id); args.insert("op", op);
    m_socket.write(QJsonDocument(args).toJson(QJsonDocument::Compact) + '\n');
    return id;
}

void Backend::receive() {
    m_buffer += m_socket.readAll();
    if (m_buffer.size() > 4 * 1024 * 1024) { m_message = "Search response exceeded its size limit."; m_socket.abort(); emit changed(); return; }
    while (m_buffer.contains('\n')) {
        const auto split = m_buffer.indexOf('\n'); QJsonParseError error;
        const auto document = QJsonDocument::fromJson(m_buffer.left(split), &error); m_buffer.remove(0, split + 1);
        const auto reply = document.object();
        if (error.error != QJsonParseError::NoError || reply.value("protocol").toInt() != 1) {
            m_message = "Unsupported search service response."; m_socket.abort(); emit changed(); return;
        }
        const auto id = reply.value("id").toString();
        if (id == m_statusId) {
            m_statusId.clear();
            if (reply.value("ok").toBool()) {
                const bool wasBusy = busy(), wasReady = modelReady();
                const auto previousRevision = m_status.value("index_revision");
                m_status = reply.value("status").toObject();
                const bool indexChanged = !previousRevision.isUndefined() && previousRevision != m_status.value("index_revision");
                if (indexChanged || (wasReady && !modelReady())) invalidateResults();
                m_poll.setInterval(busy() ? 500 : 2000);
                m_message = m_status.value("error").toString();
                if (m_message.isEmpty()) m_message = !modelReady() ? (m_status.value("phase").toString() == "verifying" ? "Verifying search service…" : "Search service is not ready. Use Retry search to try again.") : busy() ? "Indexing selected folders…" : paused() ? "Indexing paused" : "Ready";
                if ((indexChanged || (wasBusy && !busy()) || (!wasReady && modelReady())) && !m_query.trimmed().isEmpty()) m_debounce.start();
                emit changed();
            } else { m_message = reply.value("error").toString(); emit changed(); }
        } else if (id == m_searchId) {
            m_searchId.clear();
            if (reply.value("ok").toBool()) {
                const auto observed = reply.value("index_revision"), known = m_status.value("index_revision");
                if (observed.isDouble() && known.isDouble() && observed.toDouble() < known.toDouble()) {
                    m_results.clear(); m_debounce.start();
                } else m_results = reply.value("results").toArray().toVariantList();
            }
            else { m_results.clear(); m_message = reply.value("error").toString(); emit changed(); }
            emit resultsChanged();
        } else if (!reply.value("ok").toBool() && reply.value("error").toString() != "canceled") {
            m_message = reply.value("error").toString(); emit changed();
        } else if (reply.value("accepted").toBool()) {
            if (!reply.value("recovery_error").toString().isEmpty()) { m_message = reply.value("recovery_error").toString(); emit changed(); }
            m_statusId.clear(); m_statusId = send("status");
        }
    }
}

void Backend::invalidateResults() {
    if (!m_searchId.isEmpty()) send("cancel", {{"target", m_searchId}});
    m_searchId.clear(); m_results.clear(); emit resultsChanged();
}
void Backend::setQuery(const QString &text) { if (m_query == text) return; m_query = text; invalidateResults(); m_debounce.start(); emit changed(); }
void Backend::setKind(const QString &kind) { if (m_kind == kind) return; m_kind = kind; invalidateResults(); m_debounce.start(); emit changed(); }
void Backend::search() {
    if (m_query.trimmed().isEmpty() || !connected() || !modelReady()) return;
    if (m_query.trimmed().size() > 2048) { m_message = "Search text must be at most 2048 characters."; emit changed(); return; }
    m_searchId = send("search", {{"text", m_query.trimmed()}, {"kind", m_kind}, {"limit", 20}});
}
void Backend::control(const QString &op, QJsonObject args) { invalidateResults(); send(op, args); }
void Backend::addRoot(const QUrl &url) { if (url.isLocalFile()) control("add_root", {{"path", url.toLocalFile()}}); }
void Backend::removeRoot(const QString &path) { control("remove_root", {{"path", path}}); }
void Backend::setExclusions(const QString &path, const QString &text) {
    QJsonArray exclusions;
    for (auto item : text.split('\n')) { item = item.trimmed(); if (!item.isEmpty()) exclusions.append(item); }
    control("set_exclusions", {{"path", path}, {"exclusions", exclusions}});
}
void Backend::scan() { control("scan"); }
void Backend::togglePause() { control(paused() ? "resume" : "pause"); }
void Backend::wipe() { control("wipe"); }
QUrl Backend::localUrl(const QString &path) const { return QUrl::fromLocalFile(path); }
void Backend::openResult(const QVariantMap &result) {
    const auto path = result.value("path").toString();
    const auto page = result.value("page").toInt();
    if (page > 0 && !QStandardPaths::findExecutable("okular").isEmpty())
        QProcess::startDetached("okular", {"--page", QString::number(page), path});
    else QDesktopServices::openUrl(localUrl(path));
}
void Backend::reveal(const QString &path) {
    if (!QStandardPaths::findExecutable("dolphin").isEmpty()) QProcess::startDetached("dolphin", {"--select", path});
    else QDesktopServices::openUrl(localUrl(QFileInfo(path).absolutePath()));
}
void Backend::closePreview() {
    m_previewTimeout.stop();
    if (m_preview.state() != QProcess::NotRunning) { m_preview.kill(); m_preview.waitForFinished(1000); }
    if (!m_previewOutput.isEmpty()) QFile::remove(m_previewOutput);
    m_previewOutput.clear(); m_previewUrl = QUrl(); m_previewMessage.clear(); emit previewChanged();
}
void Backend::previewResult(const QVariantMap &result) {
    closePreview();
    const auto path = result.value("path").toString();
    if (result.value("kind").toString() == "image") { m_previewUrl = localUrl(path); emit previewChanged(); return; }
    const auto page = result.value("page").toInt();
    if (result.value("kind").toString() != "pdf" || page < 1) return;
    const auto renderer = QStandardPaths::findExecutable("pdftoppm");
    if (renderer.isEmpty() || !m_previewDirectory.isValid()) {
        m_previewMessage = "Page preview unavailable. Check that pdftoppm is installed."; emit previewChanged(); return;
    }
    const auto prefix = m_previewDirectory.filePath(QString::number(++m_previewSequence));
    m_previewOutput = prefix + ".png"; m_previewMessage = "Rendering page…";
    m_preview.start(renderer, {"-f", QString::number(page), "-l", QString::number(page), "-singlefile", "-scale-to", "1600", "-png", path, prefix});
    m_previewTimeout.start(30000); emit previewChanged();
}
void Backend::downloadModel() {
    if (downloading()) return;
    m_downloadPercent = 0; m_downloadBuffer.clear(); m_message = "Verifying model files and downloading missing files…";
    m_download.start("python3", {m_paths.downloader, m_paths.models}); emit changed();
}
void Backend::shutdownOwnedService() {
    if (connectedToOwnedService()) {
        m_stoppingOwnedService = true; m_retry.stop(); send("shutdown"); m_socket.flush();
    }
}
