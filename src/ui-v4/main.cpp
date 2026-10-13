#include "backend.h"
#include "film_backend.h"
#include <QApplication>
#include <KIconTheme>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QImageReader>
#include <QIcon>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <unistd.h>

int main(int argc, char **argv) {
    KIconTheme::initTheme();
    // Follow the desktop's controls and palette, while allowing Qt's explicit
    // style override for accessibility, development and other desktops.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle("org.kde.desktop");
    QImageReader::setAllocationLimit(64);
    QApplication app(argc, argv);
    app.setApplicationName("kde-index"); app.setApplicationDisplayName("SemanticLens for KDE"); app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("org.kdeindex.Search");
    app.setWindowIcon(QIcon::fromTheme("system-search"));
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    const auto data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const auto bin = QCoreApplication::applicationDirPath();
    auto option = [&parser](const QString &name, const QString &description, const QString &value) {
        parser.addOption(QCommandLineOption(name, description, "value", value));
    };
    option("socket", "Local search service socket", QString("kde-index-%1").arg(getuid()));
    option("data-dir", "Private search data directory", data);
    option("models", "Verified public model directory", QDir(data).filePath("models"));
    option("service", "Search service executable", QDir(bin).filePath("kde-index-service"));
    option("probe", "CPU embedding executable", QDir(bin).filePath("embedding-probe"));
    auto downloader = QDir(bin).filePath("../share/kde-index/model_download.py");
    if (!QFileInfo::exists(downloader)) downloader = QDir(bin).filePath("../../tools/model_download.py");
    option("downloader", "Public model setup helper", downloader);
    option("capture", "Save a window image and exit (validation)", "");
    option("capture-delay", "Milliseconds before saving a validation window image", "2000");
    option("query", "Initial search text", "");
    parser.addOption(QCommandLineOption("film-view", "Open the offline film-pilot view"));
    parser.process(app);
    auto absolute = [&parser](const QString &name) { return QFileInfo(parser.value(name)).absoluteFilePath(); };
    Backend backend({parser.value("socket"), absolute("service"), absolute("probe"),
                     absolute("models"), absolute("data-dir"), absolute("downloader")});
    FilmBackend filmBackend(parser.value("socket"), absolute("data-dir"));
    backend.setQuery(parser.value("query"));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("filmBackend", &filmBackend);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("org.kde.kdeindex", "Main");
    if (parser.isSet("film-view") && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("filmMode", true);
    if (!parser.value("capture").isEmpty()) {
        bool validDelay = false;
        const auto captureDelay = parser.value("capture-delay").toInt(&validDelay);
        if (!validDelay || captureDelay < 0 || captureDelay > 120000) return 2;
        QTimer::singleShot(captureDelay, &app, [&] {
            const auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
            const bool saved = window && window->grabWindow().save(parser.value("capture"));
            backend.shutdownOwnedService(); app.exit(saved ? 0 : 2);
        });
    }
    return app.exec();
}
