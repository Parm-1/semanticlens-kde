#include <KRunner/RunnerManager>
#include <KRunner/QueryMatch>
#include <KPluginMetaData>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>
#include <QThread>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 6) return 2;
    qputenv("KDE_INDEX_SOCKET", argv[2]);
    QPluginLoader loader(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath());
    const KPluginMetaData metadata(loader.metaData().value(QStringLiteral("MetaData")).toObject(), loader.fileName());
    if (metadata.pluginId() != QStringLiteral("kde-index-runner")) return 3;
    KRunner::RunnerManager manager;
    auto *runner = manager.loadRunner(metadata);
    if (!runner) return 4;
    const bool runnerOnMainThread = runner->thread() == QThread::currentThread();
    const int resetAfterMs = QString::fromLocal8Bit(argv[5]).toInt();
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(14000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&manager, &KRunner::RunnerManager::queryFinished, &loop, &QEventLoop::quit);
    QTimer heartbeat;
    int ticks = 0;
    QObject::connect(&heartbeat, &QTimer::timeout, &loop, [&] { ++ticks; });
    heartbeat.start(10);
    bool resetObserved = false;
    QElapsedTimer elapsed;
    QElapsedTimer resetClock;
    QTimer resetTimer;
    resetTimer.setSingleShot(true);
    if (resetAfterMs > 0) {
        resetTimer.setInterval(resetAfterMs);
        QObject::connect(&resetTimer, &QTimer::timeout, &loop, [&] {
            resetObserved = true;
            resetClock.start();
            manager.reset();
            loop.quit();
        });
    }
    elapsed.start();
    timeout.start();
    manager.launchQuery(QString::fromLocal8Bit(argv[3]));
    if (resetAfterMs > 0) resetTimer.start();
    loop.exec();
    if (resetObserved) {
        QEventLoop afterReset;
        QTimer::singleShot(150, &afterReset, &QEventLoop::quit);
        afterReset.exec();
    }

    QJsonArray matches;
    for (const auto &match : manager.matches()) {
        matches.append(QJsonObject{{QStringLiteral("text"), match.text()},
                                   {QStringLiteral("subtext"), match.subtext()},
                                   {QStringLiteral("relevance"), match.relevance()},
                                   {QStringLiteral("data"), QJsonObject::fromVariantMap(match.data().toMap())}});
    }
    const QString expectedPath = QString::fromLocal8Bit(argv[4]);
    bool passed = resetAfterMs > 0 ? (resetObserved && matches.isEmpty())
            : (matches.size() == 1 && matches.first().toObject().value(QStringLiteral("relevance")).toDouble() == 1.0 &&
               matches.first().toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("path")).toString() == expectedPath);
    const QJsonObject result{{QStringLiteral("pass"), passed},
                             {QStringLiteral("runner_on_main_thread"), runnerOnMainThread},
                             {QStringLiteral("reset_observed"), resetObserved},
                             {QStringLiteral("reset_to_final_observation_ms"), resetObserved ? resetClock.elapsed() : 0},
                             {QStringLiteral("elapsed_ms"), elapsed.elapsed()},
                             {QStringLiteral("main_timer_ticks"), ticks},
                             {QStringLiteral("matches"), matches}};
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << std::endl;
    return passed && !runnerOnMainThread ? 0 : 5;
}
