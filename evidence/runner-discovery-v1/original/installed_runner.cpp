// Direct installed-plugin discovery/query control; requires an owned config path.
#include <KRunner/RunnerManager>
#include <KRunner/AbstractRunner>
#include <KRunner/QueryMatch>
#include <KPluginMetaData>
#include <KConfig>
#include <KConfigGroup>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 6) return 2;
    const QString root = QFileInfo(QString::fromLocal8Bit(argv[1])).canonicalFilePath();
    const QString configPath = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    if (root.isEmpty() || !QDir(root).exists() || QFileInfo(configPath).exists()) return 3;
    QCoreApplication::addLibraryPath(root);
    qputenv("KDE_INDEX_SOCKET", argv[3]);
    const QString stableId = QStringLiteral("kde-index-runner");
    const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/krunner"));
    QJsonArray discovered;
    QString discoveredPath;
    int count = 0;
    for (const auto &plugin : plugins) {
        if (plugin.pluginId() != stableId) continue;
        ++count;
        discoveredPath = QFileInfo(plugin.fileName()).canonicalFilePath();
        discovered.append(QJsonObject{{"id", plugin.pluginId()}, {"file", discoveredPath}, {"valid", plugin.isValid()}});
    }
    if (count != 1 || !discoveredPath.startsWith(root + QLatin1Char('/'))) {
        std::cout << QJsonDocument(QJsonObject{{"discovered", discovered}, {"pass", false}}).toJson().toStdString();
        return 4;
    }
    KConfig config(configPath, KConfig::SimpleConfig);
    KRunner::RunnerManager manager(KConfigGroup(&config, "Plugins"), KConfigGroup(&config, "State"), nullptr);
    manager.setHistoryEnabled(false);
    manager.setAllowedRunners({stableId});
    QEventLoop loop;
    bool finished = false;
    QObject::connect(&manager, &KRunner::RunnerManager::queryFinished, &loop, [&] { finished = true; loop.quit(); });
    QTimer::singleShot(14000, &loop, &QEventLoop::quit);
    manager.launchQuery(QString::fromLocal8Bit(argv[4]), stableId);
    loop.exec();
    auto *runner = manager.runner(stableId);
    QJsonArray matches;
    for (const auto &match : manager.matches()) {
        matches.append(QJsonObject{{"text", match.text()}, {"subtext", match.subtext()},
                                  {"relevance", match.relevance()}, {"data", QJsonObject::fromVariantMap(match.data().toMap())}});
    }
    const bool offMainThread = runner && runner->thread() != QThread::currentThread();
    const bool pathMatched = !matches.isEmpty() && matches.first().toObject().value("data").toObject().value("path").toString() == QString::fromLocal8Bit(argv[5]);
    const bool pass = finished && offMainThread && pathMatched;
    std::cout << QJsonDocument(QJsonObject{{"discovered", discovered}, {"finished", finished},
                                          {"off_main_thread", offMainThread}, {"matches", matches}, {"pass", pass}}).toJson().toStdString();
    return pass ? 0 : 5;
}
