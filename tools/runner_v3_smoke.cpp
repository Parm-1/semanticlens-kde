#include <KRunner/AbstractRunner>
#include <KRunner/QueryMatch>
#include <KRunner/RunnerContext>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QLocalServer>
#include <QLocalSocket>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QThread>
#include <atomic>
#include <thread>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    const QString socketName = QStringLiteral("kde-index-runner-smoke-%1").arg(QCoreApplication::applicationPid());
    qputenv("KDE_INDEX_SOCKET", socketName.toUtf8());
    std::atomic<bool> serviceOk{false};
    std::thread service([&] {
        QLocalServer server;
        QLocalServer::removeServer(socketName);
        if (!server.listen(socketName)) return;
        if (!server.waitForNewConnection(2500)) return;
        QLocalSocket *peer = server.nextPendingConnection();
        if (!peer->waitForReadyRead(1500)) return;
        const QByteArray requestLine = peer->readLine(64 * 1024);
        const QJsonObject request = QJsonDocument::fromJson(requestLine).object();
        if (request.value(QStringLiteral("op")).toString() != QStringLiteral("search") ||
            request.value(QStringLiteral("text")).toString() != QStringLiteral("storm shelter")) return;
        const QJsonObject row{{QStringLiteral("path"), QStringLiteral("/tmp/kde-index-fixture.pdf")},
                              {QStringLiteral("title"), QStringLiteral("Storm shelter plan.pdf")},
                              {QStringLiteral("text"), QStringLiteral("Use the east stairwell during severe weather.")},
                              {QStringLiteral("page"), 3}, {QStringLiteral("kind"), QStringLiteral("pdf")},
                              {QStringLiteral("score"), 0.75}};
        const QJsonObject reply{{QStringLiteral("protocol"), 1}, {QStringLiteral("id"), request.value(QStringLiteral("id"))},
                                {QStringLiteral("ok"), true}, {QStringLiteral("results"), QJsonArray{row}}};
        peer->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
        peer->waitForBytesWritten(500);
        serviceOk = true;
    });
    QPluginLoader loader(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath());
    const auto rawMeta = loader.metaData();
    std::cerr << QJsonDocument(rawMeta).toJson(QJsonDocument::Compact).toStdString() << "\n";
    const KPluginMetaData metadata(rawMeta.value(QStringLiteral("MetaData")).toObject(), loader.fileName());
    std::cerr << "metadata=" << metadata.isValid() << " id=" << metadata.pluginId().toStdString() << " file=" << metadata.fileName().toStdString() << "\n";
    auto loaded = KPluginFactory::instantiatePlugin<KRunner::AbstractRunner>(metadata);
    if (!loaded.plugin) { std::cerr << "load failed: " << loaded.errorString.toStdString() << "\n"; service.join(); return 3; }
    KRunner::RunnerContext context;
    context.setQuery(QStringLiteral("find storm shelter"));
    loaded.plugin->match(context);
    const auto matches = context.matches();
    service.join();
    QLocalServer::removeServer(socketName);
    if (!serviceOk || matches.size() != 1) return 4;
    const auto match = matches.first();
    if (match.text() != QStringLiteral("Storm shelter plan.pdf") ||
        !match.subtext().contains(QStringLiteral("Page 3")) || !match.subtext().contains(QStringLiteral("east stairwell")) ||
        qAbs(match.relevance() - 0.875) > 0.001 || match.urls().size() != 1 || !match.urls().first().isLocalFile()) return 5;
    qInfo("plugin loaded and synthetic service result mapped successfully");
    return 0;
}
