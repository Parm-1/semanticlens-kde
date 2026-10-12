#include <KRunner/AbstractRunner>
#include <KRunner/Action>
#include <KRunner/QueryMatch>
#include <KRunner/RunnerContext>
#include <KPluginFactory>

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QElapsedTimer>
#include <QUrl>
#include <QVariantMap>
#include <QRegularExpression>
#include <atomic>
#include <unistd.h>

namespace {
constexpr qsizetype MaxReply = 256 * 1024;
constexpr int ConnectTimeoutMs = 250;
constexpr int ReplyTimeoutMs = 1100;

QString socketName()
{
    const QString overrideName = qEnvironmentVariable("KDE_INDEX_SOCKET");
    if (!overrideName.isEmpty()) return overrideName;
    return QStringLiteral("kde-index-%1").arg(QString::number(getuid()));
}

bool exchange(const QString &query, const QString &id, const KRunner::RunnerContext &context,
              QJsonObject *response)
{
    QLocalSocket socket;
    socket.connectToServer(socketName(), QIODevice::ReadWrite);
    if (!socket.waitForConnected(ConnectTimeoutMs)) return false;
    const QJsonObject request{{QStringLiteral("id"), id}, {QStringLiteral("op"), QStringLiteral("search")},
                              {QStringLiteral("text"), query}, {QStringLiteral("limit"), 12}};
    const QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    if (line.size() > 64 * 1024 || socket.write(line) != line.size() || !socket.waitForBytesWritten(ConnectTimeoutMs)) return false;

    QElapsedTimer timer;
    timer.start();
    QByteArray buffer;
    while (timer.elapsed() < ReplyTimeoutMs && context.isValid()) {
        if (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(30)) continue;
        buffer += socket.readAll();
        if (buffer.size() > MaxReply) return false;
        const qsizetype newline = buffer.indexOf('\n');
        if (newline < 0) continue;
        const QJsonDocument document = QJsonDocument::fromJson(buffer.left(newline));
        if (!document.isObject()) return false;
        const QJsonObject obj = document.object();
        if (obj.value(QStringLiteral("protocol")).toInt(-1) != 1 ||
            obj.value(QStringLiteral("id")).toString() != id || !obj.value(QStringLiteral("ok")).isBool()) return false;
        *response = obj;
        return true;
    }
    // Closing this per-request socket is the v1 cancellation mechanism.
    socket.abort();
    return false;
}

QString clipped(QString text, int maxChars)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (text.size() > maxChars) text = text.left(maxChars - 1) + QChar(0x2026);
    return text;
}
}

class KDEIndexRunner final : public KRunner::AbstractRunner
{
    Q_OBJECT
public:
    KDEIndexRunner(QObject *parent, const KPluginMetaData &metaData) : AbstractRunner(parent, metaData)
    {
        setTriggerWords({QStringLiteral("find")});
        addSyntax(QStringLiteral("find :q:"), QStringLiteral("Search selected folders by meaning and content"));
    }

    void match(KRunner::RunnerContext &context) override
    {
        if (!context.isValid()) return;
        const QString input = context.query().trimmed();
        if (!input.startsWith(QStringLiteral("find "), Qt::CaseInsensitive)) return;
        const QString query = input.mid(5).trimmed();
        if (query.isEmpty() || query.size() > 2048) return;

        const QString id = QString::number(++m_serial);
        QJsonObject reply;
        if (!exchange(query, id, context, &reply)) {
            if (!context.isValid()) return;
            KRunner::QueryMatch setup(this);
            setup.setId(QStringLiteral("service-unavailable"));
            setup.setText(QStringLiteral("KDE Index service is not running"));
            setup.setSubtext(QStringLiteral("Open KDE Index to configure the local service and selected folders"));
            setup.setIconName(QStringLiteral("dialog-information"));
            setup.setRelevance(0.2);
            setup.setData(QVariantMap{{QStringLiteral("setup"), true}});
            context.addMatch(setup);
            return;
        }
        if (!reply.value(QStringLiteral("ok")).toBool() || !context.isValid()) return;
        const QJsonArray rows = reply.value(QStringLiteral("results")).toArray();
        QList<KRunner::QueryMatch> matches;
        for (const QJsonValue &value : rows) {
            if (!context.isValid()) return;
            if (!value.isObject()) continue;
            const QJsonObject row = value.toObject();
            const QString path = row.value(QStringLiteral("path")).toString();
            if (path.isEmpty() || !QFileInfo(path).isAbsolute()) continue;
            const QString kind = row.value(QStringLiteral("kind")).toString();
            const int page = row.value(QStringLiteral("page")).toInt();
            const double score = row.value(QStringLiteral("score")).toDouble(-1.0);
            if (score < -1.0 || score > 1.0 || !qIsFinite(score)) continue;
            const QString title = row.value(QStringLiteral("title")).toString(QFileInfo(path).fileName());
            QString passage = clipped(row.value(QStringLiteral("text")).toString(), 240);
            QString detail = kind == QStringLiteral("pdf") && page > 0
                    ? QStringLiteral("Page %1 · %2").arg(page).arg(passage)
                    : passage;
            if (detail.isEmpty()) detail = path;
            KRunner::QueryMatch match(this);
            match.setId(path + QLatin1Char('#') + QString::number(page));
            match.setText(title.isEmpty() ? QFileInfo(path).fileName() : title);
            match.setSubtext(detail);
            match.setIconName(kind == QStringLiteral("pdf") ? QStringLiteral("application-pdf") : QStringLiteral("text-x-generic"));
            // Dot scores are not probabilities. Map their documented [-1,1] range monotonically into KRunner [0,1].
            match.setRelevance(qBound(0.0, (score + 1.0) / 2.0, 1.0));
            match.setUrls({QUrl::fromLocalFile(path)});
            match.setData(QVariantMap{{QStringLiteral("path"), path}, {QStringLiteral("kind"), kind}, {QStringLiteral("page"), page}});
            match.addAction(KRunner::Action(QStringLiteral("reveal"), QStringLiteral("folder-open"), QStringLiteral("Show in Dolphin")));
            matches.append(match);
        }
        if (context.isValid()) context.addMatches(matches);
    }

    void run(const KRunner::RunnerContext &, const KRunner::QueryMatch &match) override
    {
        const QVariantMap data = match.data().toMap();
        if (data.value(QStringLiteral("setup")).toBool()) {
            const QString app = QStandardPaths::findExecutable(QStringLiteral("kde-index"));
            if (!app.isEmpty()) QProcess::startDetached(app, {});
            return;
        }
        const QString path = data.value(QStringLiteral("path")).toString();
        if (path.isEmpty()) return;
        if (match.selectedAction().id() == QStringLiteral("reveal")) {
            QProcess::startDetached(QStringLiteral("dolphin"), {QStringLiteral("--select"), path});
            return;
        }
        const int page = data.value(QStringLiteral("page")).toInt();
        if (data.value(QStringLiteral("kind")).toString() == QStringLiteral("pdf") && page > 0) {
            const QString okular = QStandardPaths::findExecutable(QStringLiteral("okular"));
            if (!okular.isEmpty() && QProcess::startDetached(okular, {QStringLiteral("--page"), QString::number(page), path})) return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
private:
    std::atomic<quint64> m_serial{0};
};

K_PLUGIN_CLASS_WITH_JSON(KDEIndexRunner, "kde-index-runner.json")
#include "runner.moc"
