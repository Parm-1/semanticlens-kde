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
#include <cmath>
#include <limits>
#include <unistd.h>

namespace {
constexpr qsizetype MaxReply = 256 * 1024;
constexpr int ConnectTimeoutMs = 250;
constexpr int ReplyTimeoutMs = 8000;

QString socketName()
{
    const QString overrideName = qEnvironmentVariable("KDE_INDEX_SOCKET");
    if (!overrideName.isEmpty()) return overrideName;
    return QStringLiteral("kde-index-%1").arg(QString::number(getuid()));
}

QString clipped(QString text, int maxChars);

struct ExchangeResult {
    enum class Failure { None, Cancelled, NotFound, Connect, Timeout, InvalidResponse, ServiceError };
    Failure failure = Failure::None;
    QJsonObject response;
    QString detail;
};

ExchangeResult exchange(const QString &query, const QString &id, const KRunner::RunnerContext &context)
{
    QLocalSocket socket;
    socket.connectToServer(socketName(), QIODevice::ReadWrite);
    if (!socket.waitForConnected(ConnectTimeoutMs)) {
        if (!context.isValid()) return {ExchangeResult::Failure::Cancelled, {}, {}};
        const auto failure = socket.error() == QLocalSocket::ServerNotFoundError
                ? ExchangeResult::Failure::NotFound : ExchangeResult::Failure::Connect;
        return {failure, {}, socket.errorString()};
    }
    const QJsonObject request{{QStringLiteral("id"), id}, {QStringLiteral("op"), QStringLiteral("search")},
                              {QStringLiteral("text"), query}, {QStringLiteral("limit"), 12}};
    const QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    if (line.size() > 64 * 1024 || socket.write(line) != line.size() || !socket.waitForBytesWritten(ConnectTimeoutMs)) {
        if (!context.isValid()) return {ExchangeResult::Failure::Cancelled, {}, {}};
        return {ExchangeResult::Failure::Connect, {}, socket.errorString()};
    }

    QElapsedTimer timer;
    timer.start();
    QByteArray buffer;
    while (timer.elapsed() < ReplyTimeoutMs) {
        if (!context.isValid()) {
            socket.abort();
            return {ExchangeResult::Failure::Cancelled, {}, {}};
        }
        if (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(30)) {
            if (socket.state() == QLocalSocket::UnconnectedState)
                return {ExchangeResult::Failure::InvalidResponse, {}, QStringLiteral("peer closed before a complete response")};
            continue;
        }
        buffer += socket.readAll();
        if (buffer.size() > MaxReply)
            return {ExchangeResult::Failure::InvalidResponse, {}, QStringLiteral("response exceeds size limit")};
        const qsizetype newline = buffer.indexOf('\n');
        if (newline < 0) continue;
        const QJsonDocument document = QJsonDocument::fromJson(buffer.left(newline));
        if (!document.isObject())
            return {ExchangeResult::Failure::InvalidResponse, {}, QStringLiteral("response is not a JSON object")};
        const QJsonObject obj = document.object();
        const QJsonValue protocol = obj.value(QStringLiteral("protocol"));
        if (!protocol.isDouble() || protocol.toDouble() != 1.0 ||
            !obj.value(QStringLiteral("id")).isString() || obj.value(QStringLiteral("id")).toString() != id ||
            !obj.value(QStringLiteral("ok")).isBool())
            return {ExchangeResult::Failure::InvalidResponse, {}, QStringLiteral("response protocol or request identity is invalid")};
        if (!obj.value(QStringLiteral("ok")).toBool()) {
            const QString error = obj.value(QStringLiteral("error")).toString().trimmed();
            if (error.isEmpty()) return {ExchangeResult::Failure::InvalidResponse, {}, QStringLiteral("failure response has no error")};
            return {ExchangeResult::Failure::ServiceError, {}, clipped(error, 180)};
        }
        return {ExchangeResult::Failure::None, obj, {}};
    }
    if (!context.isValid()) {
        socket.abort();
        return {ExchangeResult::Failure::Cancelled, {}, {}};
    }
    return {ExchangeResult::Failure::Timeout, {}, {}};
}

QString clipped(QString text, int maxChars)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (text.size() > maxChars) text = text.left(maxChars - 1) + QChar(0x2026);
    return text;
}

// Milou forwards these fields to Plasma ToolTip labels using AutoText and strips
// ASCII ampersands for mnemonics. Keep source passages readable without letting
// Qt recognize HTML tags/entities. This is presentation only: IDs, URLs and
// action data retain the original source strings.
QString displayText(QString text)
{
    text.replace(QLatin1Char('<'), QChar(0xff1c));
    text.replace(QLatin1Char('>'), QChar(0xff1e));
    text.replace(QLatin1Char('&'), QChar(0xff06));
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
        if (!input.startsWith(QStringLiteral("find "))) return;
        const QString query = input.mid(5).trimmed();
        if (query.isEmpty() || query.size() > 2048) return;

        const QString id = QString::number(++m_serial);
        const ExchangeResult exchangeResult = exchange(query, id, context);
        if (exchangeResult.failure == ExchangeResult::Failure::Cancelled || !context.isValid()) return;
        if (exchangeResult.failure != ExchangeResult::Failure::None) {
            KRunner::QueryMatch diagnostic(this);
            QString matchId;
            QString title;
            QString subtext;
            bool setup = false;
            switch (exchangeResult.failure) {
            case ExchangeResult::Failure::NotFound:
                matchId = QStringLiteral("service-not-found");
                title = QStringLiteral("KDE Index service is not running");
                subtext = QStringLiteral("Open KDE Index to configure the local service and selected folders");
                setup = true;
                break;
            case ExchangeResult::Failure::Timeout:
                matchId = QStringLiteral("service-timeout");
                title = QStringLiteral("KDE Index search timed out");
                subtext = QStringLiteral("The service connected but did not finish this search in time");
                break;
            case ExchangeResult::Failure::InvalidResponse:
                matchId = QStringLiteral("service-invalid-response");
                title = QStringLiteral("KDE Index service returned an invalid response");
                subtext = exchangeResult.detail;
                break;
            case ExchangeResult::Failure::ServiceError:
                matchId = QStringLiteral("service-error");
                title = QStringLiteral("KDE Index service could not complete the search");
                subtext = exchangeResult.detail;
                break;
            case ExchangeResult::Failure::Connect:
                matchId = QStringLiteral("service-connection-error");
                title = QStringLiteral("Could not connect to KDE Index service");
                subtext = QStringLiteral("The local service socket rejected the connection");
                break;
            case ExchangeResult::Failure::None:
            case ExchangeResult::Failure::Cancelled:
                return;
            }
            diagnostic.setId(matchId);
            diagnostic.setText(displayText(title));
            diagnostic.setSubtext(displayText(subtext));
            diagnostic.setIconName(QStringLiteral("dialog-information"));
            diagnostic.setRelevance(0.2);
            diagnostic.setData(QVariantMap{{QStringLiteral("setup"), setup}});
            context.addMatch(diagnostic);
            return;
        }
        const QJsonObject reply = exchangeResult.response;
        if (!context.isValid()) return;
        const QJsonValue resultsValue = reply.value(QStringLiteral("results"));
        if (!resultsValue.isArray()) return;
        const QJsonArray rows = resultsValue.toArray();
        QList<KRunner::QueryMatch> matches;
        for (const QJsonValue &value : rows) {
            if (!context.isValid()) return;
            if (!value.isObject()) continue;
            const QJsonObject row = value.toObject();
            const QJsonValue pathValue = row.value(QStringLiteral("path"));
            const QJsonValue titleValue = row.value(QStringLiteral("title"));
            const QJsonValue textValue = row.value(QStringLiteral("text"));
            const QJsonValue kindValue = row.value(QStringLiteral("kind"));
            const QJsonValue pageValue = row.value(QStringLiteral("page"));
            const QJsonValue scoreValue = row.value(QStringLiteral("score"));
            if (!pathValue.isString() || !titleValue.isString() || !textValue.isString() ||
                !kindValue.isString() || !pageValue.isDouble() || !scoreValue.isDouble()) continue;
            const QString path = pathValue.toString();
            const QString title = titleValue.toString();
            const QString kind = kindValue.toString();
            const double pageNumber = pageValue.toDouble();
            const double score = scoreValue.toDouble();
            if (path.isEmpty() || !QFileInfo(path).isAbsolute() || title.trimmed().isEmpty() ||
                !qIsFinite(pageNumber) || pageNumber < 0.0 || pageNumber > std::numeric_limits<int>::max() ||
                std::floor(pageNumber) != pageNumber || !qIsFinite(score) || score < -1.0 || score > 1.0 ||
                (kind != QStringLiteral("text") && kind != QStringLiteral("pdf") && kind != QStringLiteral("image"))) continue;
            const int page = static_cast<int>(pageNumber);
            if ((kind == QStringLiteral("pdf") && page < 1) || (kind != QStringLiteral("pdf") && page != 0)) continue;
            const QString displayTitle = clipped(title, 160);
            const QString sourceText = textValue.toString();
            if (kind != QStringLiteral("image") && sourceText.isEmpty()) continue;
            QString passage = clipped(sourceText, 240);
            QString detail = kind == QStringLiteral("pdf") && page > 0
                    ? QStringLiteral("Page %1 · %2").arg(page).arg(passage)
                    : passage;
            if (detail.isEmpty()) detail = path;
            KRunner::QueryMatch match(this);
            match.setId(path + QLatin1Char('#') + QString::number(page));
            match.setText(displayText(displayTitle));
            match.setSubtext(displayText(detail));
            match.setIconName(kind == QStringLiteral("pdf") ? QStringLiteral("application-pdf") : QStringLiteral("text-x-generic"));
            // `score` is the producer ranking score: exact hits are 1; semantic_score remains a separate dot diagnostic.
            // Map the producer contract range monotonically into KRunner relevance [0,1].
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
