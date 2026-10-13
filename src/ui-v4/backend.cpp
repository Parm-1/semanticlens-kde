#include "backend.h"
#include "media_group.h"
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
#include <sys/prctl.h>
#include <QSet>
#include <QThread>
#include <QSocketNotifier>

struct MediaJob : QObject {
    QProcess process;
    QTimer timeout;
    std::shared_ptr<QTemporaryDir> directory = std::make_shared<QTemporaryDir>(QDir::tempPath() + "/semanticlens-media-XXXXXX");
    QVariantMap result;
    QString key;
    quint64 sequence = 0;
    bool preview = false, canceled = false;
    std::shared_ptr<MediaGroup> group;
    int identitySocket[2] {-1, -1};
    QSocketNotifier *identityNotifier = nullptr;
    QString unavailableReason;
    ~MediaJob() override { for (auto fd : identitySocket) if (fd >= 0) ::close(fd); }
    void receiveIdentity(bool wait = false) {
        if (identitySocket[0] < 0) return;
        if (wait) { struct pollfd ready {identitySocket[0], POLLIN, 0}; ::poll(&ready, 1, 1000); }
        bool unsupported = false; group = receiveMediaGroup(identitySocket[0], &unsupported);
        if (identityNotifier) identityNotifier->setEnabled(false);
        acknowledgeMediaGroup(identitySocket[0], bool(group) && !canceled);
        for (auto &fd : identitySocket) { if (fd >= 0) ::close(fd); fd = -1; }
        if (!group) unavailableReason = unsupported ? "Local media previews require Linux 6.9 or newer with pidfd process-group signaling." : "Unable to establish safe local media cleanup.";
        if (canceled && group) group->signal(SIGKILL);
    }
    explicit MediaJob(QObject *parent) : QObject(parent) { timeout.setSingleShot(true); }
    void cancel() {
        canceled = true; timeout.stop();
        if (group) group->signal(SIGKILL);
    }
};
static bool beneath(const QString &path, const QString &root) {
    return root == "/" ? path.startsWith('/') : path == root || path.startsWith(root + "/");
}

QString Backend::indexingNotice() const {
    if (!m_status.value("scan_error").toString().isEmpty())
        return "Indexing is incomplete. Search shows the last indexed contents.";
    if (m_status.value("scan_skipped_providers").toDouble() > 0
        || m_status.value("scan_skipped_names").toDouble() > 0
        || !m_status.value("scan_warnings").toArray().isEmpty())
        return "Some folders or files were skipped. See the warnings for details.";
    if (!m_status.value("watch_error").toString().isEmpty())
        return "Automatic updates are incomplete. Use Refresh.";
    return {};
}

QVariantList Backend::failures() const {
    auto items = m_status.value("failures").toArray().toVariantList();
    const auto scanError = m_status.value("scan_error").toString();
    if (!scanError.isEmpty())
        items.append(QVariantMap{{"path", ""}, {"error", "Folder scan: " + scanError}});
    const auto warnings = m_status.value("scan_warnings").toArray().toVariantList();
    for (const auto &warning : warnings) items.append(warning);
    const auto watchError = m_status.value("watch_error").toString();
    if (!watchError.isEmpty())
        items.append(QVariantMap{{"path", ""}, {"error", "Folder updates: " + watchError}});
    return items;
}

Backend::Backend(AppPaths paths, QObject *parent) : QObject(parent), m_paths(std::move(paths)) {
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

    m_poll.stop(); m_debounce.stop(); m_retry.stop();
    m_socket.abort();
    if (m_download.state() != QProcess::NotRunning) {
        m_download.kill(); m_download.waitForFinished(1000);
    }
    closePreview(); clearThumbnails();
    for (auto job : m_mediaJobs) {
        job->process.disconnect(this);
        // A starting child is blocked before exec until descriptor adoption.
        // Reject that gate during shutdown, rather than waiting for GUI events.
        job->cancel();
        if (!job->group) job->receiveIdentity(true);
        if (job->group) job->group->signal(SIGKILL);
        job->process.waitForFinished(1000);
        if (job->group) { QThread::msleep(25); job->group->signal(SIGKILL); }
        delete job;
    }
    m_mediaJobs.clear();
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
            QStringList args {"--data-dir", m_paths.data, "--socket", m_paths.socket, "--probe", m_paths.probe,
                                    "--probe-sha256", QString::fromLatin1(producerHash.result().toHex()),
                                    "--text-model", QDir(m_paths.models).filePath("embeddinggemma-2-Q8_0.gguf"),
                                    "--image-model", QDir(m_paths.models).filePath("mmproj-embeddinggemma-2-Q8_0.gguf")};
            const auto tokenizerPath = QDir(QFileInfo(m_paths.service).absolutePath()).filePath("semanticlens-film-tokenizer");
            QFile tokenizer(tokenizerPath);
            QCryptographicHash tokenizerHash(QCryptographicHash::Sha256);
            if (tokenizer.open(QIODevice::ReadOnly) && tokenizerHash.addData(&tokenizer))
                args << "--film-tokenizer" << tokenizerPath << "--film-tokenizer-sha256" << QString::fromLatin1(tokenizerHash.result().toHex());
            const auto filmWorkerPath = QDir(QFileInfo(m_paths.service).absolutePath()).filePath("semanticlens-film-worker");
            QFile filmWorker(filmWorkerPath);
            QCryptographicHash filmWorkerHash(QCryptographicHash::Sha256);
            if (filmWorker.open(QIODevice::ReadOnly) && filmWorkerHash.addData(&filmWorker))
                args << "--film-worker" << filmWorkerPath << "--film-worker-sha256" << QString::fromLatin1(filmWorkerHash.result().toHex());
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
                const auto previousRevision = m_status.value("index_revision"), previousRoots = m_status.value("roots");
                m_status = reply.value("status").toObject();
                const bool indexChanged = !previousRevision.isUndefined() && previousRevision != m_status.value("index_revision");
                const bool rootsChanged = previousRoots.isArray() && previousRoots != m_status.value("roots");
                if (indexChanged || rootsChanged || (wasReady && !modelReady())) invalidateResults();
                m_poll.setInterval(busy() ? 500 : 2000);
                m_message = m_status.value("error").toString();
                if (m_message.isEmpty()) m_message = !modelReady() ? (m_status.value("phase").toString() == "verifying" ? "Verifying search service…" : "Search service is not ready. Use Retry search to try again.") : busy() ? "Indexing selected folders…" : paused() ? "Indexing paused" : "Ready";
                if ((indexChanged || rootsChanged || (wasBusy && !busy()) || (!wasReady && modelReady())) && !m_query.trimmed().isEmpty()) m_debounce.start();
                emit changed();
            } else { m_message = reply.value("error").toString(); emit changed(); }
        } else if (id == m_searchId) {
            m_searchId.clear(); closePreview();
            if (reply.value("ok").toBool()) {
                const auto observed = reply.value("index_revision"), known = m_status.value("index_revision");
                if (observed.isDouble() && known.isDouble() && observed.toDouble() < known.toDouble()) {
                    m_results.clear(); m_debounce.start();
                } else m_results = reply.value("results").toArray().toVariantList().mid(0, 20);
            }
            else { m_results.clear(); m_message = reply.value("error").toString(); emit changed(); }
            prepareThumbnails(); emit resultsChanged(); emit changed();
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
    m_searchId.clear(); m_results.clear(); closePreview(); clearThumbnails(); emit resultsChanged(); emit changed();
}
void Backend::setQuery(const QString &text) { if (m_query == text) return; m_query = text; invalidateResults(); m_debounce.start(); emit changed(); }
void Backend::setKind(const QString &kind) { if (m_kind == kind) return; m_kind = kind; invalidateResults(); m_debounce.start(); emit changed(); }
void Backend::search() {
    if (m_query.trimmed().isEmpty() || !connected() || !modelReady()) { emit changed(); return; }
    if (m_query.trimmed().size() > 2048) { m_message = "Search text must be at most 2048 characters."; emit changed(); return; }
    m_searchId = send("search", {{"text", m_query.trimmed()}, {"kind", m_kind}, {"limit", 20}}); emit changed();
}
void Backend::control(const QString &op, QJsonObject args) { invalidateResults(); send(op, args); }
void Backend::addRoot(const QUrl &url) {
    if (!url.isLocalFile()) {
        m_message = "Choose a local folder."; emit changed(); return;
    }
    const auto rawPath = QByteArray::fromPercentEncoding(url.path(QUrl::FullyEncoded).toLatin1());
    if (rawPath.contains(char(0)) || QString::fromUtf8(rawPath).toUtf8() != rawPath) {
        m_message = "This folder name is not supported. Choose a folder with a valid UTF-8 name.";
        emit changed(); return;
    }
    control("add_root", {{"path", url.toLocalFile()}});
}
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
QString Backend::resultKey(const QVariantMap &result) const {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonObject::fromVariantMap(result)).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
bool Backend::currentLocalResult(const QVariantMap &result) const {
    // Membership compares the complete service-produced row, including page and kind.
    bool current = false;
    for (const auto &value : m_results) if (value.toMap() == result) { current = true; break; }
    if (!current || !modelReady()) return false;
    const auto path = result.value("path").toString();
    if (!QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path || path.contains(QChar(char16_t(0)))
        || path.toUtf8().size() > 4096 || QString::fromUtf8(path.toUtf8()) != path || beneath(path, QDir::cleanPath(m_paths.data))) return false;
    QVariantMap selected;
    for (const auto &value : roots()) {
        const auto root = value.toMap(); const auto prefix = root.value("path").toString();
        if (!prefix.isEmpty() && beneath(path, prefix) && prefix.size() > selected.value("path").toString().size()) selected = root;
    }
    const auto root = selected.value("path").toString();
    if (root.isEmpty() || path == root) return false;
    const auto relative = path.mid(root == "/" ? 1 : root.size() + 1);
    for (const auto &value : selected.value("exclusions").toList()) if (beneath(relative, value.toString())) return false;
    return true; // Actual descriptor/mount admission occurs in the helper before any source read.
}
QUrl Backend::thumbnailUrl(const QVariantMap &result) const {
    return currentLocalResult(result) ? m_thumbnails.value(resultKey(result)) : QUrl();
}
void Backend::clearThumbnails() {
    ++m_thumbnailSequence; m_thumbnailQueue.clear();
    if (m_thumbnail) m_thumbnail->cancel();
    m_thumbnails.clear(); m_thumbnailDirectories.clear(); ++m_thumbnailRevision; emit thumbnailsChanged();
}
void Backend::prepareThumbnails() {
    clearThumbnails();
    for (const auto &value : m_results.mid(0, 20)) {
        const auto result = value.toMap(); const auto kind = result.value("kind").toString();
        if (currentLocalResult(result) && (kind == "image" || (kind == "pdf" && result.value("page").toInt() > 0))) m_thumbnailQueue.append(result);
    }
    nextThumbnail();
}
void Backend::nextThumbnail() {
    if (m_thumbnail || m_thumbnailQueue.isEmpty()) return;
    const auto result = m_thumbnailQueue.takeFirst().toMap();
    if (!currentLocalResult(result)) { nextThumbnail(); return; }
    startMedia(result, false);
}
void Backend::closePreview() {
    ++m_previewSequence; m_pendingPreview.clear();
    if (m_preview) m_preview->cancel();
    m_previewDirectory.reset(); m_previewUrl = QUrl(); m_previewMessage.clear(); emit previewChanged();
}
void Backend::previewResult(const QVariantMap &result) {
    closePreview();
    if (!currentLocalResult(result)) return;
    const auto kind = result.value("kind").toString();
    if (kind != "image" && (kind != "pdf" || result.value("page").toInt() < 1)) return;
    m_pendingPreview = result; m_previewMessage = kind == "pdf" ? "Rendering page…" : "Loading preview…"; emit previewChanged();
    startPreview();
}
void Backend::startPreview() {
    if (m_preview || m_pendingPreview.isEmpty()) return;
    const auto result = m_pendingPreview; m_pendingPreview.clear();
    if (currentLocalResult(result)) startMedia(result, true);
}
void Backend::startMedia(const QVariantMap &result, bool preview) {
    auto job = new MediaJob(this); job->result = result; job->key = resultKey(result); job->preview = preview;
    job->sequence = preview ? m_previewSequence : m_thumbnailSequence;
    m_mediaJobs.append(job);
    if (preview) m_preview = job; else m_thumbnail = job;
    auto finish = [this, job](bool success) {
        job->timeout.stop();
        if (!job->group) job->receiveIdentity();
        const auto group = job->group;
        if (group) group->signal(SIGKILL);
        // Recheck after signal delivery: a renderer may have been in fork when
        // the first cancellation signal was sent. This pass never blocks the UI.
        QTimer::singleShot(25, QCoreApplication::instance(), [group] { if (group) group->signal(SIGKILL); });
        const bool valid = !job->canceled && job->sequence == (job->preview ? m_previewSequence : m_thumbnailSequence) && currentLocalResult(job->result);
        const auto output = job->directory->filePath("render.png");
        if (job->preview) {
            m_preview = nullptr;
            if (valid) {
                if (success && QFileInfo::exists(output)) { m_previewDirectory = job->directory; m_previewUrl = QUrl::fromLocalFile(output); m_previewMessage.clear(); }
                else m_previewMessage = job->unavailableReason.isEmpty() ? "Preview unavailable. The indexed passage is shown below." : job->unavailableReason;
                emit previewChanged();
            }
        } else {
            m_thumbnail = nullptr;
            if (valid && success && QFileInfo::exists(output)) {
                m_thumbnailDirectories.append(job->directory); m_thumbnails.insert(job->key, QUrl::fromLocalFile(output));
                ++m_thumbnailRevision; emit thumbnailsChanged();
            }
        }
        m_mediaJobs.removeOne(job); job->deleteLater();
        if (job->preview) startPreview(); else nextThumbnail();
    };

    connect(&job->process, &QProcess::finished, this, [finish](int code, auto state) { finish(code == 0 && state == QProcess::NormalExit); });
    connect(&job->process, &QProcess::errorOccurred, this, [finish](auto error) { if (error == QProcess::FailedToStart) finish(false); });
    connect(&job->timeout, &QTimer::timeout, this, [this, job] {
        if (job->preview && job->sequence == m_previewSequence && !job->canceled) {
            m_previewMessage = "Preview timed out. The indexed passage is shown below."; emit previewChanged();
        }
        job->cancel();
    });
    if (!job->directory->isValid()) { finish(false); return; }
    QJsonObject request = QJsonObject::fromVariantMap(result); request.insert("pixels", preview ? 1600 : 320);
    for (const auto &value : roots()) {
        const auto root = value.toMap();
        if (beneath(result.value("path").toString(), root.value("path").toString())) {
            request.insert("root", root.value("path").toString()); request.insert("exclusions", QJsonArray::fromVariantList(root.value("exclusions").toList())); break;
        }
    }
    // Private descriptor transport, created before fork. Unsupported kernels
    // stop the child before exec/render; there is no numerical group fallback.
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, job->identitySocket) != 0) {
        job->unavailableReason = "Unable to establish safe local media cleanup."; finish(false); return;
    }
    job->identityNotifier = new QSocketNotifier(job->identitySocket[0], QSocketNotifier::Read, job);
    connect(job->identityNotifier, &QSocketNotifier::activated, this, [job] { job->receiveIdentity(); });
    const pid_t owner = ::getpid();
    const int identityReader = job->identitySocket[0], identityWriter = job->identitySocket[1];
    job->process.setChildProcessModifier([owner, identityReader, identityWriter] {
        ::close(identityReader);
        if (::setpgid(0, 0) != 0 || ::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || ::getppid() != owner || !sendOwnMediaGroup(identityWriter)) ::_exit(126);
        ::close(identityWriter);
    });
    job->process.setStandardOutputFile(QProcess::nullDevice()); job->process.setStandardErrorFile(QProcess::nullDevice());
    job->process.start(QDir(QCoreApplication::applicationDirPath()).filePath("semanticlens-media"), {job->directory->path()});
    job->process.write(QJsonDocument(request).toJson(QJsonDocument::Compact)); job->process.closeWriteChannel(); job->timeout.start(30000);
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
