#include <KRunner/RunnerManager>
#include <KRunner/QueryMatch>
#include <KPluginMetaData>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPluginLoader>
#include <QTimer>
#include <QThread>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 4) return 2;
    qputenv("KDE_INDEX_SOCKET", argv[3]);
    QPluginLoader loader(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath());
    const KPluginMetaData metadata(loader.metaData().value(QStringLiteral("MetaData")).toObject(), loader.fileName());
    KRunner::RunnerManager manager;
    auto *runner = manager.loadRunner(metadata);
    if (!runner) return 3;
    const bool runnerOnMainThread = runner->thread() == QThread::currentThread();
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&manager, &KRunner::RunnerManager::queryFinished, &loop, &QEventLoop::quit);
    timeout.start(10000);
    manager.launchQuery(QStringLiteral("find RUNNER-SERVICE-MARKER-81273"));
    loop.exec();
    QJsonArray matches;
    for (const auto &match : manager.matches())
        matches.append(QJsonObject{{QStringLiteral("text"), match.text()},
                                   {QStringLiteral("subtext"), match.subtext()},
                                   {QStringLiteral("relevance"), match.relevance()},
                                   {QStringLiteral("data"), QJsonObject::fromVariantMap(match.data().toMap())}});
    const bool passed = matches.size() == 1 && matches.first().toObject().value(QStringLiteral("relevance")).toDouble() == 1.0 &&
        matches.first().toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("path")).toString() == argv[2];
    std::cout << QJsonDocument(QJsonObject{{QStringLiteral("pass"), passed}, {QStringLiteral("runner_on_main_thread"), runnerOnMainThread},
                                           {QStringLiteral("matches"), matches}}).toJson(QJsonDocument::Compact).toStdString() << std::endl;
    return passed ? 0 : 4;
}
