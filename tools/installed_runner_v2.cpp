// Direct installed-plugin discovery/query control; requires an owned config path.
#include <KRunner/RunnerManager>
#include <KRunner/AbstractRunner>
#include <KRunner/QueryMatch>
#include <KPluginMetaData>
#include <KConfig>
#include <KConfigGroup>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QElapsedTimer>
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
    if (argc != 6 && argc != 7) return 2;
    const QString root = QFileInfo(QString::fromLocal8Bit(argv[1])).canonicalFilePath();
    const QString configPath = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    if (root.isEmpty() || !QDir(root).exists() || QFileInfo(configPath).exists()) return 3;
    QCoreApplication::addLibraryPath(root);
    qputenv("KDE_INDEX_SOCKET", argv[3]);
    const QString stableId = argc == 7 ? QString::fromLocal8Bit(argv[6]) : QStringLiteral("kde-index-runner");
    const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/krunner"));
    QJsonArray discovered, allPlugins, cachedPlugins;
    for (const auto &plugin : plugins) {
        QPluginLoader loader(plugin.fileName());
        allPlugins.append(QJsonObject{{"id", plugin.pluginId()}, {"file", plugin.fileName()}, {"valid", plugin.isValid()},
                                      {"raw", plugin.rawData()}, {"qt_metadata", loader.metaData()}});
    }
    for (const auto &plugin : KPluginMetaData::findPlugins(QStringLiteral("kf6/krunner"), {}, KPluginMetaData::CacheMetaData))
        cachedPlugins.append(QJsonObject{{"id", plugin.pluginId()}, {"file", plugin.fileName()}, {"valid", plugin.isValid()}, {"raw", plugin.rawData()}});
    QJsonObject diagnostic{{"all_plugins", allPlugins}, {"cached_plugins", cachedPlugins},
                           {"library_paths", QJsonArray::fromStringList(QCoreApplication::libraryPaths())}, {"expected_id", stableId}};
    QString discoveredPath;
    int count = 0;
    for (const auto &plugin : plugins) {
        if (plugin.pluginId() != stableId) continue;
        ++count;
        discoveredPath = QFileInfo(plugin.fileName()).canonicalFilePath();
        discovered.append(QJsonObject{{"id", plugin.pluginId()}, {"file", discoveredPath}, {"valid", plugin.isValid()}});
    }
    if (count != 1 || !discoveredPath.startsWith(root + QLatin1Char('/'))) {
        diagnostic.insert("discovered", discovered); diagnostic.insert("pass", false);
        std::cout << QJsonDocument(diagnostic).toJson().toStdString();
        return 4;
    }
    diagnostic.insert("discovered", discovered);
    if (QString::fromLocal8Bit(argv[4]) == QStringLiteral("--metadata-only")) {
        diagnostic.insert("pass", true);
        std::cout << QJsonDocument(diagnostic).toJson().toStdString();
        return 0;
    }
    KConfig config(configPath, KConfig::SimpleConfig);
    KRunner::RunnerManager manager(KConfigGroup(&config, "Plugins"), KConfigGroup(&config, "State"), nullptr);
    manager.setHistoryEnabled(false);
    manager.setAllowedRunners({stableId});
    QEventLoop loop;
    bool finished = false;
    QObject::connect(&manager, &KRunner::RunnerManager::queryFinished, &loop, [&] { finished = true; loop.quit(); });
    QTimer::singleShot(14000, &loop, &QEventLoop::quit);
    QElapsedTimer timer; timer.start();
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
    diagnostic.insert("finished", finished); diagnostic.insert("off_main_thread", offMainThread);
    diagnostic.insert("query_ms", timer.elapsed()); diagnostic.insert("matches", matches); diagnostic.insert("pass", pass);
    diagnostic.insert("runner_id", runner ? runner->id() : QString());
    std::cout << QJsonDocument(diagnostic).toJson().toStdString();
    return pass ? 0 : 5;
}
