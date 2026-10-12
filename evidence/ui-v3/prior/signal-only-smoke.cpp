// Direct target-window/service integration on an explicitly supplied synthetic root.
#include "backend.h"
#include <QApplication>
#include <KIconTheme>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QFile>
#include <cstdio>
#include <atomic>

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
    QString previewFile;
    QTimer closeObservation; closeObservation.setSingleShot(true); closeObservation.setInterval(90000);
    int phase = 0; bool added = false, passed = false;
    QJsonArray checks, observations;
    std::atomic<quint64> sceneFrames{0};
    quint64 readyFrame = 0;
    bool imageFrameArmed = false;
    const auto frameObservation = QObject::connect(window, &QQuickWindow::afterFrameEnd, &app, [&] { sceneFrames.fetch_add(1); }, Qt::DirectConnection);
    QElapsedTimer elapsed; elapsed.start();
    QTimer tick; tick.setInterval(100);
    auto check = [&](QString name, bool pass, QJsonObject details = {}) {
        details["name"] = name; details["pass"] = pass; checks.append(details);
        return pass;
    };
    auto imageObservation = [&](QString event) {
        auto image = window->findChild<QQuickItem *>("previewImage");
        QJsonObject state{{"event", event}, {"milliseconds", elapsed.elapsed()}, {"scene_frames", qint64(sceneFrames.load())}, {"preview_url", backend.previewUrl().toString()}};
        if (image) {
            const double paintedWidth = image->property("paintedWidth").toDouble(), paintedHeight = image->property("paintedHeight").toDouble();
            const auto position = image->mapToScene(QPointF((image->width() - paintedWidth) / 2, (image->height() - paintedHeight) / 2));
            state["image_status"] = image->property("status").toInt(); state["visible"] = image->isVisible();
            state["width"] = image->width(); state["height"] = image->height();
            state["painted_width"] = paintedWidth; state["painted_height"] = paintedHeight;
            state["painted_scene_x"] = position.x(); state["painted_scene_y"] = position.y();
            state["window_width"] = window->width(); state["window_height"] = window->height();
            state["window_visible"] = window->isVisible(); state["window_exposed"] = window->isExposed();
            state["image_source"] = image->property("source").toUrl().toString();
        }
        observations.append(state); return state;
    };
    const auto previewObservation = QObject::connect(&backend, &Backend::previewChanged, &app, [&] { imageObservation("backend-preview-changed"); });
    QObject::connect(&tick, &QTimer::timeout, &app, [&] {
        if (elapsed.elapsed() > 90000) { check("completion-within-bound", false); app.exit(1); return; }
        if (phase == 0 && backend.modelReady()) {
            if (!added) { backend.addRoot(QUrl::fromLocalFile(corpus)); added = true; return; }
            if (backend.roots().size() == 1 && !backend.busy() && backend.indexed() >= 3) {
                check("selected-root-indexed", true, {{"roots", QJsonArray::fromVariantList(backend.roots())}, {"indexed", backend.indexed()}}); backend.setQuery("PDF-10299"); phase = 1;
            }
        } else if (phase == 1 && !backend.results().isEmpty()) {
            const auto result = backend.results().first().toMap();
            if (!check("exact-pdf-page-result", result.value("kind").toString() == "pdf" && result.value("page").toInt() == 2, {{"result", QJsonObject::fromVariantMap(result)}})) { app.exit(1); return; }
            if (!check("target-results-window-capture", window->grabWindow().save(output + "-results.png"))) { app.exit(1); return; }
            auto preview = window->findChild<QObject *>("resultPreview");
            if (!check("preview-dialog-found", preview != nullptr)) { app.exit(1); return; }
            preview->setProperty("selectedResult", result);
            QMetaObject::invokeMethod(preview, "open"); phase = 2;
        } else if (phase == 2 && !backend.previewUrl().isEmpty()) {
            const auto file = backend.previewUrl().toLocalFile();
            if (previewFile.isEmpty()) {
                previewFile = file;
                if (!check("actual-pdf-raster", !QImage(file).isNull())) { app.exit(1); return; }
                QFile::remove(output + "-raster.png");
                if (!QFile::copy(file, output + "-raster.png")) { check("retain-source-raster", false); app.exit(1); return; }
                imageObservation("first-preview-url");
                window->grabWindow().save(output + "-premature.png");
            }
            auto image = window->findChild<QQuickItem *>("previewImage");
            // URL publication and QImage(file) do not imply asynchronous QML texture readiness.
            const bool ready = image && image->property("status").toInt() == 1
                && image->isVisible() && image->opacity() > 0 && image->width() > 0 && image->height() > 0
                && image->property("paintedWidth").toDouble() > 0 && image->property("paintedHeight").toDouble() > 0
                && image->property("source").toUrl() == backend.previewUrl();
            if (!ready) { imageObservation("waiting-image-ready-and-geometry"); return; }
            if (!imageFrameArmed) {
                imageFrameArmed = true; readyFrame = sceneFrames.load();
                imageObservation("image-ready-frame-armed"); window->update();
                window->grabWindow(); // A supported scene readback also submits a frame when unexposed.
                return;
            }
            if (sceneFrames.load() <= readyFrame) { imageObservation("waiting-subsequent-scene-frame"); window->update(); window->grabWindow(); return; }
            const auto rendered = imageObservation("capture-after-ready-scene-frame");
            if (!check("target-page-window-capture", window->grabWindow().save(output + "-page.png"), rendered)) { app.exit(1); return; }
            auto dialog = window->findChild<QObject *>("resultPreview");
            QObject::connect(dialog, SIGNAL(closed()), &closeObservation, SLOT(stop()));
            closeObservation.start();
            QMetaObject::invokeMethod(dialog, "close");
            phase = 5;
        } else if (phase == 5 && !closeObservation.isActive() && !window->findChild<QObject *>("resultPreview")->property("visible").toBool() && backend.previewUrl().isEmpty()) {
            if (!check("preview-temporary-file-removed", !QFileInfo::exists(previewFile), {{"closed_signal_observed", !closeObservation.isActive()}, {"url_empty", backend.previewUrl().isEmpty()}, {"file", previewFile}, {"file_exists", QFileInfo::exists(previewFile)}})) { app.exit(1); return; }
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
    QObject::disconnect(frameObservation); QObject::disconnect(previewObservation);
    backend.closePreview(); backend.shutdownOwnedService();
    const auto report = QJsonDocument(QJsonObject{{"pass", passed}, {"milliseconds", elapsed.elapsed()}, {"checks", checks}, {"render_observations", observations}}).toJson();
    fwrite(report.constData(), 1, size_t(report.size()), stdout);
    fflush(stdout);
    return code;
}
