#include "backend.h"
#include <QApplication>
#include <KIconTheme>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
int main(int argc, char **argv) {
    KIconTheme::initTheme(); QApplication app(argc, argv);
    app.setApplicationName("kde-index"); app.setApplicationDisplayName("SemanticLens for KDE");
    Backend backend({"/public-clean-v2/source/native-runtime/no-service.sock", "/usr/bin/false", "/usr/bin/false", "/public-clean-v2/source/empty-models", "/public-clean-v2/source/native-data", ""});
    QQmlApplicationEngine engine; engine.rootContext()->setContextProperty("backend", &backend);
    engine.loadFromModule("org.kde.kdeindex", "Main");
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) return 2;
    window->show(); app.processEvents();
    QJsonObject result{{"application_name", app.applicationName()}, {"application_display_name", app.applicationDisplayName()}, {"window_title", window->title()}, {"native_window_visible", window->isVisible()}};
    const auto bytes=QJsonDocument(result).toJson(QJsonDocument::Compact); std::fwrite(bytes.constData(),1,size_t(bytes.size()),stdout); std::fputc('\n',stdout);
    backend.shutdownOwnedService();
    return app.applicationDisplayName()=="SemanticLens for KDE" && window->title()=="SemanticLens for KDE" && window->isVisible() ? 0 : 1;
}
