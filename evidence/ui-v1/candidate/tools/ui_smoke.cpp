// Direct target-window/service integration on an explicitly supplied synthetic root.
#include "backend.h"
#include <QApplication>
#include <KIconTheme>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QFile>
#include <cstdio>

int main(int argc, char **argv) {
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    if (argc != 8) { fprintf(stderr, "ui-smoke SERVICE PROBE MODELS DATA SOCKET CORPUS OUTPUT\n"); return 2; }
    auto absolute = [](const char *arg) { return QFileInfo(QString::fromLocal8Bit(arg)).absoluteFilePath(); };
    Backend backend({QString::fromLocal8Bit(argv[5]), absolute(argv[1]), absolute(argv[2]), absolute(argv[3]), absolute(argv[4]), ""});
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.loadFromModule("org.kde.kdeindex", "Main");
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) return 2;
    const auto corpus = absolute(argv[6]), output = absolute(argv[7]);
    int phase = 0; bool added = false, passed = false;
    QJsonArray checks;
    QElapsedTimer elapsed; elapsed.start();
    QTimer tick; tick.setInterval(100);
    auto check = [&](QString name, bool pass) {
        checks.append(QJsonObject{{"name", name}, {"pass", pass}});
        return pass;
    };
    QObject::connect(&tick, &QTimer::timeout, &app, [&] {
        if (elapsed.elapsed() > 90000) { check("completion-within-bound", false); app.exit(1); return; }
        if (phase == 0 && backend.modelReady()) {
            if (!added) { backend.addRoot(QUrl::fromLocalFile(corpus)); added = true; return; }
            if (backend.roots().size() == 1 && !backend.busy() && backend.indexed() >= 3) {
                check("selected-root-indexed", true); backend.setQuery("PDF-10299"); phase = 1;
            }
        } else if (phase == 1 && !backend.results().isEmpty()) {
            const auto result = backend.results().first().toMap();
            if (!check("exact-pdf-page-result", result.value("kind").toString() == "pdf" && result.value("page").toInt() == 2)) { app.exit(1); return; }
            if (!check("target-results-window-capture", window->grabWindow().save(output + "-results.png"))) { app.exit(1); return; }
            auto preview = window->findChild<QObject *>("resultPreview");
            if (!check("preview-dialog-found", preview != nullptr)) { app.exit(1); return; }
            preview->setProperty("selectedResult", result);
            QMetaObject::invokeMethod(preview, "open"); phase = 2;
        } else if (phase == 2 && !backend.previewUrl().isEmpty()) {
            const auto file = backend.previewUrl().toLocalFile();
            if (!check("actual-pdf-raster", !QImage(file).isNull())) { app.exit(1); return; }
            if (!check("target-page-window-capture", window->grabWindow().save(output + "-page.png"))) { app.exit(1); return; }
            QMetaObject::invokeMethod(window->findChild<QObject *>("resultPreview"), "close");
            if (!check("preview-temporary-file-removed", !QFileInfo::exists(file))) { app.exit(1); return; }
            backend.setQuery("new-query");
            if (!check("previous-results-cleared-immediately", backend.results().isEmpty())) { app.exit(1); return; }
            backend.setQuery("PDF-10299"); phase = 3;
        } else if (phase == 3 && !backend.results().isEmpty() && backend.results().first().toMap().value("kind").toString() == "pdf") {
            if (!check("delete-owned-synthetic-pdf", QFile::remove(corpus + "/manual.pdf"))) { app.exit(1); return; }
            phase = 4;
        } else if (phase == 4 && backend.indexed() == 2 && !backend.busy() && !backend.results().isEmpty()) {
            for (const auto &row : backend.results()) if (row.toMap().value("path").toString().endsWith("/manual.pdf")) return;
            check("cached-results-refresh-after-fast-deletion", true);
            passed = true; app.exit(0);
        }
    });
    tick.start();
    const int code = app.exec();
    backend.closePreview(); backend.shutdownOwnedService();
    const auto report = QJsonDocument(QJsonObject{{"pass", passed}, {"milliseconds", elapsed.elapsed()}, {"checks", checks}}).toJson();
    fwrite(report.constData(), 1, size_t(report.size()), stdout);
    fflush(stdout);
    return code;
}
