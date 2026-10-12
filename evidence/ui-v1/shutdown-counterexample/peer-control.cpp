// Regression for destruction with a connected socket and initialized callback state.
#include "backend.h"
#include <QCoreApplication>
#include <QLocalServer>
#include <QJsonDocument>
#include <QFileInfo>
#include <memory>
#include <cstdio>
#include <unistd.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    const QString name = QString("/tmp/kde-index-ui-lifetime-%1").arg(getpid());
    QLocalServer server;
    if (!server.listen(name)) return 2;
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        auto peer = server.nextPendingConnection();
        auto buffer = std::make_shared<QByteArray>();
        QObject::connect(peer, &QLocalSocket::readyRead, peer, [peer, buffer] {
            *buffer += peer->readAll();
            while (buffer->contains('\n')) {
                auto split = buffer->indexOf('\n');
                auto request = QJsonDocument::fromJson(buffer->left(split)).object();
                buffer->remove(0, split + 1);
                QJsonObject reply{{"protocol", 1}, {"id", request.value("id")}, {"ok", true},
                    {"status", QJsonObject{{"model_ready", true}, {"ready", true}, {"index_revision", 0}}}};
                peer->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
            }
        });
    });
    auto backend = std::make_unique<Backend>(AppPaths{name, "", "",
        QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath(), "", ""});
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (!backend->connected() || !backend->modelReady()) return;
        backend->scan(); // Leave a just-written request pending at socket destruction.
        backend.reset();
        puts("connected-state destruction completed"); fflush(stdout);
        app.exit(0);
    });
    poll.start(10);
    QTimer::singleShot(3000, &app, [] { QCoreApplication::exit(1); });
    return app.exec();
}
