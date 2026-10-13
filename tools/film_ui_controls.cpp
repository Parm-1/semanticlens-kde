// Native Qt controls against the real service. All mutation stays in a caller-owned output tree.
#include "backend.h"
#include "film_backend.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QSet>
#include <KIconTheme>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>

namespace {
QQuickItem *findItem(QQuickItem *parent, const QString &name) {
    if (!parent) return nullptr;
    if (parent->objectName() == name) return parent;
    for (auto *child : parent->childItems())
        if (auto *found = findItem(child, name)) return found;
    return nullptr;
}

QQuickItem *findTextItem(QQuickItem *parent, const QString &text) {
    if (!parent) return nullptr;
    if (parent->property("text").toString() == text) return parent;
    for (auto *child : parent->childItems())
        if (auto *found = findTextItem(child, text)) return found;
    return nullptr;
}

void collectTextProperties(QObject *object, QStringList *texts) {
    if (!object || !texts) return;
    const auto text = object->property("text");
    if (text.isValid()) texts->append(text.toString());
    for (auto *child : object->children()) collectTextProperties(child, texts);
}

void collectVisualTexts(QQuickItem *item, QStringList *texts) {
    if (!item || !texts) return;
    const auto text = item->property("text");
    if (text.isValid()) texts->append(text.toString());
    for (auto *child : item->childItems()) collectVisualTexts(child, texts);
}

void collectVisibleTextItems(QQuickItem *item, const QString &text, QList<QQuickItem *> *matches) {
    if (!item || !matches) return;
    if (item->isVisible() && item->property("text").toString() == text) matches->append(item);
    for (auto *child : item->childItems()) collectVisibleTextItems(child, text, matches);
}

QByteArray sha256(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

qint64 monotonicNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

bool writeJsonFile(const QString &path, const QJsonObject &object) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    return file.write(bytes) == bytes.size() && file.commit();
}

bool copyFixtureTree(const QString &source, const QString &destination) {
    QDir dest(destination);
    if (dest.exists() || !QDir().mkpath(destination)) return false;
    QDir src(source);
    for (const auto &entry : src.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        const QString target = dest.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyFixtureTree(entry.absoluteFilePath(), target)) return false;
        } else if (entry.isFile() && !QFile::copy(entry.absoluteFilePath(), target)) {
            return false;
        }
    }
    return true;
}

bool writePriorFixture(const QString &corpus, quint16 canaryPort) {
    const QString source = corpus + "/harbor/analysis.json";
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(in.readAll());
    auto sidecar = doc.object();
    sidecar["analysis_id"] = "analysis:harbor:ui-prior";
    auto claims = sidecar["claims"].toArray();
    if (claims.isEmpty()) return false;
    auto claim = claims[0].toObject();
    claim["claim_id"] = "claim:harbor:ui-prior";
    claim["basis"] = "unverified_prior";
    claim["text"] = QString("PRIOR-LANTERN-616 is an imported unverified interpretation. <img src=\"http://127.0.0.1:%1/hostile\">").arg(canaryPort);
    claim["evidence"] = QJsonArray{};
    claims = {claim};
    sidecar["claims"] = claims;
    const QString output = corpus + "/harbor/prior-ui.json";
    QFile out(output);
    return out.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && out.write(QJsonDocument(sidecar).toJson(QJsonDocument::Compact)) > 0;
}

QVariantMap mapAt(const QVariantList &list, int index) {
    return index >= 0 && index < list.size() ? list[index].toMap() : QVariantMap{};
}

QStringList heavyProcessSnapshot() {
    QStringList matches;
    for (const auto &entry : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false; const qlonglong pid = entry.toLongLong(&ok);
        if (!ok || pid == QCoreApplication::applicationPid()) continue;
        const QString target = QFileInfo("/proc/" + entry + "/exe").symLinkTarget().section('/', -1);
        if (target == "kde-index-service" || target == "embedding-probe"
            || target == "semanticlens-film-tokenizer" || target == "llama-server")
            matches << QString::number(pid) + ":" + target;
    }
    matches.sort();
    return matches;
}

QStringList ownedHeavyProcessSnapshot(const QStringList &privateArgs) {
    const qlonglong self = QCoreApplication::applicationPid();
    QStringList matches;
    for (const auto &entry : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false; const qlonglong pid = entry.toLongLong(&ok);
        if (!ok || pid == self) continue;
        const QString target = QFileInfo("/proc/" + entry + "/exe").symLinkTarget().section('/', -1);
        if (target != "kde-index-service" && target != "embedding-probe"
            && target != "semanticlens-film-tokenizer" && target != "llama-server") continue;
        QFile cmdFile("/proc/" + entry + "/cmdline");
        if (!cmdFile.open(QIODevice::ReadOnly)) continue;
        const QByteArray cmd = cmdFile.readAll();
        bool privateArgMatch = false;
        for (const auto &arg : privateArgs)
            if (!arg.isEmpty() && cmd.contains(arg.toUtf8())) privateArgMatch = true;
        qlonglong current = pid;
        bool descendant = false;
        for (int depth = 0; depth < 32 && current > 1; ++depth) {
            QFile status("/proc/" + QString::number(current) + "/status");
            if (!status.open(QIODevice::ReadOnly)) break;
            qlonglong ppid = 0;
            const auto lines = status.readAll().split('\n');
            for (const auto &line : lines)
                if (line.startsWith("PPid:")) { ppid = line.mid(5).trimmed().toLongLong(); break; }
            if (ppid == self) { descendant = true; break; }
            if (ppid <= 1 || ppid == current) break;
            current = ppid;
        }
        if ((descendant && privateArgMatch) || privateArgMatch)
            matches << QString::number(pid) + ":" + target;
    }
    matches.sort();
    return matches;
}
}

int main(int argc, char **argv) {
    KIconTheme::initTheme();
    QQuickStyle::setStyle("org.kde.desktop");
    QApplication app(argc, argv);
    const bool geometryOnly = argc == 10 && QString::fromLocal8Bit(argv[9]) == "--geometry-only";
    if (argc != 9 && !geometryOnly) {
        std::fprintf(stderr, "film-ui-controls SOURCE SERVICE PROBE MODELS DATA SOCKET FIXTURES OUTPUT [--geometry-only]\n");
        return 2;
    }

    const QString root = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString service = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    const QString probe = QFileInfo(QString::fromLocal8Bit(argv[3])).absoluteFilePath();
    const QString models = QFileInfo(QString::fromLocal8Bit(argv[4])).absoluteFilePath();
    const QString data = QFileInfo(QString::fromLocal8Bit(argv[5])).absoluteFilePath();
    const QString socket = QString::fromLocal8Bit(argv[6]);
    const QString fixtures = QFileInfo(QString::fromLocal8Bit(argv[7])).absoluteFilePath();
    const QString output = QFileInfo(QString::fromLocal8Bit(argv[8])).absoluteFilePath();
    if (!QFileInfo(socket).isAbsolute()) return 2;
    QElapsedTimer runTimer; runTimer.start();
    if (!QDir().mkpath(output)) return 2;
    const QString corpus = output + "/corpus";
    if (!geometryOnly && !copyFixtureTree(fixtures, corpus)) return 2;
    QJsonArray legacyInputPins;
    QString paymentPath;
    QString backgroundManifestPath;
    QString backgroundSourcePath;
    QString backgroundSourceSha;
    int backgroundSourceLines = 0;
    qint64 backgroundSourceBytes = 0;
    if (!geometryOnly) {
        const QString legacySource = root + "/local/film-legacy-preparation-v1/legacy-corpus";
        const QString legacyCopy = corpus + "/legacy";
        if (!QDir().mkpath(legacyCopy)) return 2;
        const QList<QPair<QString, QString>> legacyFiles {
            {"payment.txt", "5b7def7d8846d5b26f0336d6645a43c87ab398ee8cad2d3a63275207d2aad73c"},
            {"manual.pdf", "dbdcc51ab580593f572c2c8bd09e414b10f0b6c53c0c886db990987232e54de1"},
            {"picture.jpg", "243df6bca3dfce7888ba872d9a774b7d030f5f6d4c5f92bd11d0f3402c026133"},
            {"i05.png", "43d5978ee7d9cdee64efbb7235a0ba7c9107b7df4e6594ed25eb60d6028485d1"}
        };
        for (const auto &entry : legacyFiles) {
            const QString from = legacySource + "/" + entry.first;
            const QString to = legacyCopy + "/" + entry.first;
            if (QString::fromLatin1(sha256(from)) != entry.second || !QFile::copy(from, to)
                || QString::fromLatin1(sha256(to)) != entry.second) return 2;
            legacyInputPins.append(QJsonObject{{"name", entry.first}, {"sha256", entry.second},
                {"bytes", QFileInfo(to).size()}});
        }
        paymentPath = legacyCopy + "/payment.txt";

        const QString backgroundDir = corpus + "/u02";
        if (!QDir().mkpath(backgroundDir)) return 2;
        backgroundSourcePath = backgroundDir + "/background-embedding.txt";
        QByteArray background;
        background.reserve(256 * 768);
        for (int line = 0; line < 256; ++line) {
            QString value = QString("Background embedding control row %1. ").arg(line, 3, 10, QLatin1Char('0'));
            const QString sentence = "Offline harbor lantern record describes a distant vessel, a quiet crew, and the weather beyond the shoreline. ";
            for (int repeat = 0; repeat < 3; ++repeat) value += sentence;
            background += value.toUtf8();
            background += '\n';
        }
        backgroundSourceLines = background.count('\n');
        backgroundSourceBytes = background.size();
        QSaveFile backgroundFile(backgroundSourcePath);
        if (!backgroundFile.open(QIODevice::WriteOnly) || backgroundFile.write(background) != background.size()
            || !backgroundFile.commit() || backgroundSourceLines != 256 || backgroundSourceBytes > 2 * 1024 * 1024) return 2;
        backgroundSourceSha = QString::fromLatin1(QCryptographicHash::hash(background, QCryptographicHash::Sha256).toHex());
        backgroundManifestPath = backgroundDir + "/background-manifest.json";
        const QJsonObject manifest{{"schema_version", "semanticlens.film.manifest/1"},
            {"film_id", "film:u02-background-control"}, {"edition_id", "edition:u02-background-control:v1"},
            {"display_title", "Offline background embedding control"}, {"year", QJsonValue::Null},
            {"sources", QJsonArray{QJsonObject{{"source_id", "source:u02-background-control:text"},
                {"relative_path", "u02/background-embedding.txt"}, {"sha256", backgroundSourceSha},
                {"source_kind", "screenplay"}, {"language", "en"}, {"edition_relation", "exact_edition"}}}}};
        QSaveFile manifestFile(backgroundManifestPath);
        const auto manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Compact) + '\n';
        if (!manifestFile.open(QIODevice::WriteOnly) || manifestFile.write(manifestBytes) != manifestBytes.size()
            || !manifestFile.commit()) return 2;
    }

    QTcpServer canary;
    int canaryRequests = 0;
    int canaryPositiveReads = 0;
    int canaryHostileReads = 0;
    if (!geometryOnly && (!canary.listen(QHostAddress::LocalHost, 0) || !writePriorFixture(corpus, canary.serverPort()))) return 2;
    QObject::connect(&canary, &QTcpServer::newConnection, &app, [&] {
        while (auto *connection = canary.nextPendingConnection()) {
            QObject::connect(connection, &QTcpSocket::readyRead, connection, [&, connection] {
                const auto request = connection->readAll();
                if (request.contains("GET /positive")) ++canaryPositiveReads;
                if (request.contains("GET /hostile")) ++canaryHostileReads;
                ++canaryRequests;
                connection->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                connection->disconnectFromHost();
            });
        }
    });
    if (!geometryOnly) {
        QTcpSocket positive;
        positive.connectToHost(QHostAddress::LocalHost, canary.serverPort());
        const bool connected = positive.waitForConnected(1000);
        if (connected) {
            positive.write("GET /positive HTTP/1.1\r\nHost: localhost\r\n\r\n");
            positive.flush();
            QElapsedTimer positiveTimer; positiveTimer.start();
            while (canaryPositiveReads != 1 && positiveTimer.elapsed() < 1000) QTest::qWait(10);
        }
        if (!connected || canaryPositiveReads != 1) return 2;
    }

    const QStringList heavyProcessesBefore = geometryOnly ? heavyProcessSnapshot() : QStringList{};
    Backend files({socket, service, probe, models, data, root + "/tools/model_download.py"});
    FilmBackend films(socket, data);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &files);
    engine.rootContext()->setContextProperty("filmBackend", &films);
    QStringList warnings;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
        [&](const QList<QQmlError> &errors) { for (const auto &error : errors) warnings << error.toString(); });
    engine.loadFromModule("org.kde.kdeindex", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) return 2;
    window->setProperty("filmMode", true);
    window->show();

    QJsonArray checks;
    bool passed = true;
    QStringList importFailures;
    QStringList keyboardFocusObservations;
    QJsonObject u02Report;
    QJsonObject lateResponseReport;
    QJsonObject restartReport;
    auto check = [&](const QString &name, bool ok) {
        checks.append(QJsonObject{{"name", name}, {"pass", ok}});
        passed = passed && ok;
        return ok;
    };
    auto writeEarlyFailure = [&](const QString &checkName) {
        window->grabWindow().save(output + "/failure-postcondition.png");
        files.closePreview();
        files.shutdownOwnedService();
        films.setEnabled(false);
        QJsonObject report{{"pass", false}, {"run_status", "failed_postcondition"},
            {"failed_postcondition", checkName}, {"checks", checks},
            {"service_sha256", QString::fromLatin1(sha256(service))},
            {"probe_sha256", QString::fromLatin1(sha256(probe))},
            {"tokenizer_sha256", QString::fromLatin1(sha256(QFileInfo(service).dir().filePath("semanticlens-film-tokenizer")))},
            {"text_model_sha256", QString::fromLatin1(sha256(models + "/embeddinggemma-2-Q8_0.gguf"))},
            {"image_model_sha256", QString::fromLatin1(sha256(models + "/mmproj-embeddinggemma-2-Q8_0.gguf"))},
            {"loopback_requests", canaryRequests}, {"loopback_hostile_requests", canaryHostileReads},
            {"qml_warnings", QJsonArray::fromStringList(warnings)},
            {"keyboard_focus_observations", QJsonArray::fromStringList(keyboardFocusObservations)},
            {"import_failures", QJsonArray::fromStringList(importFailures)},
            {"legacy_input_pins", legacyInputPins}, {"u02", u02Report},
            {"elapsed_seconds", double(runTimer.elapsed()) / 1000.0},
            {"source_root", "<project source tree>"}, {"socket", socket}, {"corpus", corpus},
            {"usage_measurement_scope", "Harness elapsed time only; model-token and package accounting unavailable."}};
        QFile receipt(output + "/results.json");
        if (receipt.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented) + '\n';
            receipt.write(bytes);
        }
    };
    auto require = [&](const QString &name, bool ok) {
        check(name, ok);
        if (!ok) writeEarlyFailure(name);
        return ok;
    };
    auto wait = [&](std::function<bool()> condition, int ms = 15000) {
        QElapsedTimer timer; timer.start();
        while (!condition() && timer.elapsed() < ms) QTest::qWait(25);
        return condition();
    };
    const QString relayRecordsPath = qEnvironmentVariable("FILM_UI_RELAY_RECORDS");
    QJsonArray relayRowsSeen;
    qint64 relayRecordsOffset = 0;
    auto findRelayEvent = [&](const QString &eventName, std::function<bool(const QJsonObject &)> predicate) {
        QFile relayFile(relayRecordsPath);
        if (relayFile.open(QIODevice::ReadOnly) && relayFile.seek(relayRecordsOffset)) {
            while (!relayFile.atEnd()) {
                const auto doc = QJsonDocument::fromJson(relayFile.readLine());
                if (doc.isObject()) relayRowsSeen.append(doc.object());
                relayRecordsOffset = relayFile.pos();
            }
        }
        for (const auto &entry : relayRowsSeen) {
            const auto row = entry.toObject();
            if (row.value("event").toString() == eventName && predicate(row)) return row;
        }
        return QJsonObject{};
    };
    auto waitRelayEvent = [&](const QString &eventName, std::function<bool(const QJsonObject &)> predicate,
                              int timeoutMs = 15000) {
        QJsonObject found;
        const bool observed = wait([&] {
            found = findRelayEvent(eventName, predicate);
            return !found.isEmpty();
        }, timeoutMs);
        return observed ? found : QJsonObject{};
    };
    auto item = [&](const char *name) { return findItem(window->contentItem(), QString::fromLatin1(name)); };
    auto click = [&](const char *name) {
        auto *control = item(name);
        if (!control || !control->isVisible() || !control->isEnabled()) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            control->mapToScene(QPointF(control->width()/2, control->height()/2)).toPoint());
        return true;
    };
    auto capture = [&](const QString &name) {
        QTest::qWait(250);
        return window->grabWindow().save(output + "/" + name + ".png");
    };
    if (geometryOnly) {
        window->resize(720, 520);
        auto item = [&](const char *name) { return findItem(window->contentItem(), QString::fromLatin1(name)); };
        auto waitGeometry = [&](std::function<bool()> condition, int ms = 12000) {
            QElapsedTimer timer; timer.start();
            while (!condition() && timer.elapsed() < ms) QTest::qWait(20);
            return condition();
        };
        QJsonArray checks;
        auto checkGeometry = [&](const QString &name, bool ok) {
            checks.append(QJsonObject{{"name", name}, {"pass", ok}});
            return ok;
        };
        const bool requiredItemsReady = waitGeometry([&] { return item("filmPage") && item("filmCollectionSelector")
            && item("filmPriorOptIn") && item("filmEvidencePanel"); });
        auto *page = item("filmPage");
        auto *selector = item("filmCollectionSelector");
        auto *prior = item("filmPriorOptIn");
        auto *panel = item("filmEvidencePanel");
        const bool panelLayoutReady = waitGeometry([&] { return panel && panel->isVisible()
            && panel->height() >= 200; }, 3000);
        const QRectF panelBounds = panel ? QRectF(panel->mapToScene(QPointF()), QSizeF(panel->width(), panel->height())) : QRectF();
        const QRectF windowBounds(0, 0, window->width(), window->height());
        const QString displayedText = selector ? selector->property("displayText").toString() : QString();
        auto *styleBackground = selector ? selector->property("background").value<QQuickItem *>() : nullptr;
        auto *hiddenTextField = selector ? selector->property("contentItem").value<QQuickItem *>() : nullptr;
        QList<QQuickItem *> visibleDisplayTexts;
        if (selector) collectVisibleTextItems(selector, "All film collections", &visibleDisplayTexts);
        const bool displaySinglePaint = selector && selector->isVisible()
            && displayedText == "All film collections"
            && styleBackground && styleBackground->isVisible()
            && styleBackground->property("text").toString() == displayedText
            && hiddenTextField && !hiddenTextField->isVisible()
            && visibleDisplayTexts.size() == 1 && visibleDisplayTexts.first() == styleBackground;
        checkGeometry("720x520-window-and-film-page", window->width() == 720 && window->height() == 520
            && page && page->isVisible());
        checkGeometry("prior-opt-in-row-visible-and-unchecked", prior && prior->isVisible()
            && !prior->property("checked").toBool());
        checkGeometry("evidence-panel-at-least-200px-high", panel && panel->isVisible()
            && panelLayoutReady && panel->height() >= 200 && panelBounds.top() >= windowBounds.top()-1
            && panelBounds.bottom() <= windowBounds.bottom()+1);
        checkGeometry("collection-selector-single-painted-native-label", displaySinglePaint);
        bool popupDelegateOk = false;
        bool popupClosed = true;
        bool popupOpened = false;
        bool popupMouseOpened = false;
        bool popupOpenMethodInvoked = false;
        QStringList popupTextValues;
        QString popupContentType;
        QString popupClass;
        QString popupDelegateText;
        int popupDelegateTextFormat = -1;
        QString popupCapturePath;
        if (selector) {
            auto *popup = selector->property("popup").value<QObject *>();
            popupClass = popup ? QString::fromLatin1(popup->metaObject()->className()) : QString("null");
            window->requestActivate();
            selector->forceActiveFocus(Qt::MouseFocusReason);
            waitGeometry([&] { return window->isActive(); }, 500);
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                selector->mapToScene(QPointF(selector->width()/2, selector->height()/2)).toPoint());
            popupMouseOpened = popup && waitGeometry([&] { return popup->property("visible").toBool(); }, 2000);
            if (!popupMouseOpened && popup)
                popupOpenMethodInvoked = QMetaObject::invokeMethod(popup, "open", Qt::DirectConnection);
            popupOpened = popup && waitGeometry([&] { return popup->property("visible").toBool(); }, 2000);
            if (popupOpened) {
                auto *popupContent = popup->property("contentItem").value<QQuickItem *>();
                auto *literalRow = findTextItem(popupContent, "All film collections");
                popupContentType = popupContent ? QString::fromLatin1(popupContent->metaObject()->className()) : QString("null");
                collectVisualTexts(popupContent, &popupTextValues);
                popupDelegateText = literalRow ? literalRow->property("text").toString() : QString();
                popupDelegateTextFormat = literalRow ? literalRow->property("textFormat").toInt() : -1;
                popupDelegateOk = literalRow && literalRow->isVisible()
                    && popupDelegateText == "All film collections"
                    && popupDelegateTextFormat == int(Qt::PlainText);
                if (capture("collection-popup-open")) popupCapturePath = output + "/collection-popup-open.png";
                if (popup) {
                    QMetaObject::invokeMethod(popup, "close", Qt::DirectConnection);
                    popupClosed = waitGeometry([&] { return !popup->property("visible").toBool(); }, 2000);
                }
            }
        }
        checkGeometry("collection-popup-has-visible-plain-text-delegate", popupDelegateOk);
        checkGeometry("collection-popup-closed", popupClosed);
        const bool captured = capture("narrow-720x520");
        checkGeometry("narrow-window-capture-written", captured);
        const bool modelPathsMissing = !QFileInfo::exists(models + "/embeddinggemma-2-Q8_0.gguf")
            && !QFileInfo::exists(models + "/mmproj-embeddinggemma-2-Q8_0.gguf");
        checkGeometry("private-invalid-service-and-model-free-backends", modelPathsMissing
            && !files.connected() && !films.available() && !files.downloading());
        const QStringList ownedHeavyProcessesAfter = ownedHeavyProcessSnapshot({socket, data, root});
        QStringList processMatches;
        const auto heavyProcessesAfter = heavyProcessSnapshot();
        for (const auto &entry : heavyProcessesAfter)
            if (!heavyProcessesBefore.contains(entry)) processMatches << entry;
        checkGeometry("no-owned-heavy-process-observed", ownedHeavyProcessesAfter.isEmpty());
        checkGeometry("required-qml-controls-ready", requiredItemsReady);
        checkGeometry("geometry-run-bounded-under-15-seconds", runTimer.elapsed() < 15000);
        QJsonArray warningRows; for (const auto &warning : warnings) warningRows.append(warning);
        QJsonObject report{{"pass", std::all_of(checks.begin(), checks.end(), [](const QJsonValue &v) { return v.toObject()["pass"].toBool(); })},
            {"run_status", "construction_only_native_qml_geometry"}, {"checks", checks},
            {"window_width", window->width()}, {"window_height", window->height()},
            {"film_page_width", page ? page->width() : 0}, {"film_page_height", page ? page->height() : 0},
            {"evidence_panel_width", panel ? panel->width() : 0}, {"evidence_panel_height", panel ? panel->height() : 0},
            {"evidence_panel_scene_x", panelBounds.x()}, {"evidence_panel_scene_y", panelBounds.y()},
            {"evidence_panel_scene_bottom", panelBounds.bottom()},
            {"evidence_panel_visible", panel && panel->isVisible()},
            {"evidence_panel_bounds_within_window", panel && panelBounds.top() >= windowBounds.top()-1
                && panelBounds.bottom() <= windowBounds.bottom()+1},
            {"evidence_panel_height_check", panel && panel->height() >= 200},
            {"evidence_panel_top_check", panelBounds.top() >= windowBounds.top()-1},
            {"evidence_panel_bottom_check", panelBounds.bottom() <= windowBounds.bottom()+1},
            {"evidence_panel_layout_ready", panelLayoutReady},
            {"collection_display_text", displayedText}, {"visible_matching_collection_label_items", visibleDisplayTexts.size()},
            {"style_item_text", styleBackground ? styleBackground->property("text").toString() : QString()},
            {"default_text_field_visible", hiddenTextField && hiddenTextField->isVisible()},
            {"collection_popup_opened", popupOpened}, {"collection_popup_content_type", popupContentType},
            {"collection_popup_class", popupClass}, {"collection_popup_mouse_opened", popupMouseOpened},
            {"collection_popup_open_method_invoked", popupOpenMethodInvoked},
            {"collection_popup_text_values", QJsonArray::fromStringList(popupTextValues)},
            {"collection_popup_delegate_text", popupDelegateText}, {"collection_popup_delegate_text_format", popupDelegateTextFormat},
            {"collection_popup_capture", popupCapturePath}, {"collection_popup_plain_text", popupDelegateOk}, {"qml_warnings", warningRows},
            {"service_path", service}, {"probe_path", probe}, {"models_path", models},
            {"data_path", data}, {"socket_path", socket}, {"model_paths_missing", modelPathsMissing},
            {"backend_connected", files.connected()}, {"film_backend_available", films.available()},
            {"model_download_active", files.downloading()},
            {"heavy_processes_before", QJsonArray::fromStringList(heavyProcessesBefore)},
            {"heavy_processes_after", QJsonArray::fromStringList(heavyProcessesAfter)},
            {"new_heavy_process_matches", QJsonArray::fromStringList(processMatches)},
            {"owned_heavy_processes_after", QJsonArray::fromStringList(ownedHeavyProcessesAfter)},
            {"screenshot", output + "/narrow-720x520.png"}, {"elapsed_seconds", double(runTimer.elapsed()) / 1000.0},
            {"usage_measurement_scope", "Model-free layout construction and render only; no inference or indexing."}};
        QFile receipt(output + "/geometry-results.json");
        if (!receipt.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 2;
        receipt.write(QJsonDocument(report).toJson(QJsonDocument::Indented) + '\n');
        films.setEnabled(false);
        files.closePreview();
        return report["pass"].toBool() ? 0 : 1;
    }
    auto typeText = [&](const QString &text) {
        auto *field = item("filmSearchField");
        if (field) field->forceActiveFocus();
        for (const QChar ch : text) {
            if (ch.isLetter()) {
                const auto key = Qt::Key(int(Qt::Key_A) + (ch.toUpper().unicode() - ushort('A')));
                QTest::keyClick(window, key, ch.isUpper() ? Qt::ShiftModifier : Qt::NoModifier);
            } else if (ch.isDigit()) {
                QTest::keyClick(window, Qt::Key(int(Qt::Key_0) + (ch.unicode() - ushort('0'))));
            } else if (ch == QLatin1Char('-')) QTest::keyClick(window, Qt::Key_Minus);
            else if (ch == QLatin1Char('.')) QTest::keyClick(window, Qt::Key_Period);
            else if (ch == QLatin1Char(' ')) QTest::keyClick(window, Qt::Key_Space);
            else if (ch == QLatin1Char('\n')) QTest::keyClick(window, Qt::Key_Return);
            else return false;
        }
        return true;
    };
    auto setSearch = [&](const QString &text) {
        auto *field = item("filmSearchField");
        if (!field || !field->isVisible() || !field->isEnabled()) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            field->mapToScene(QPointF(field->width()/2, field->height()/2)).toPoint());
        field->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        if (!typeText(text)) return false;
        QTest::qWait(300);
        return true;
    };
    auto searchFor = [&](const QString &query) {
        return setSearch(query) && wait([&] { return !films.searching() && !films.results().isEmpty(); });
    };
    auto settleQuery = [&](const QString &query) {
        return setSearch(query) && wait([&] { return !films.searching(); });
    };
    auto chooseFirstWithKeyboard = [&] {
        auto *field = item("filmSearchField");
        auto *page = item("filmPage");
        auto *results = item("filmResults");
        if (!field || !page || !results) return false;
        field->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Down);
        QTest::qWait(100);
        results->forceActiveFocus(Qt::OtherFocusReason);
        QTest::qWait(20);
        auto *focus = window->activeFocusItem();
        keyboardFocusObservations << QString("selected=%1 listFocus=%2 activeFocusName=%3 activeFocusType=%4")
            .arg(page->property("selectedIndex").toInt())
            .arg(results->hasActiveFocus())
            .arg(focus ? focus->objectName() : QString("<null>"))
            .arg(focus ? QString::fromLatin1(focus->metaObject()->className()) : QString("<null>"));
        // Selection is caused by the real Down key from the search field. The
        // following Space plus the backend evidence RPC is the focus-chain proof.
        return page->property("selectedIndex").toInt() == 0 && results->hasActiveFocus();
    };
    auto downUpSelectionAndFocusAgree = [&] {
        auto *page = item("filmPage");
        auto *list = item("filmResults");
        if (!page || !list || films.results().size() < 2 || !chooseFirstWithKeyboard()) return false;
        auto aligned = [&](int wanted, const QString &phase) {
            auto *current = list->property("currentItem").value<QQuickItem *>();
            auto *focus = window->activeFocusItem();
            const bool ok = page->property("selectedIndex").toInt() == wanted
                && list->property("currentIndex").toInt() == wanted
                && current && focus == current;
            keyboardFocusObservations << QString("%1 selected=%2 current=%3 currentItem=%4 focused=%5 aligned=%6")
                .arg(phase).arg(page->property("selectedIndex").toInt())
                .arg(list->property("currentIndex").toInt())
                .arg(current ? QString::fromLatin1(current->metaObject()->className()) : QString("<null>"))
                .arg(focus ? QString::fromLatin1(focus->metaObject()->className()) : QString("<null>"))
                .arg(ok);
            return ok;
        };
        const bool first = aligned(0, "initial-first");
        QTest::keyClick(window, Qt::Key_Down);
        QTest::qWait(50);
        const bool second = aligned(1, "down-second");
        QTest::keyClick(window, Qt::Key_Up);
        QTest::qWait(50);
        const bool back = aligned(0, "up-first");
        return first && second && back;
    };
    auto selectModeThroughPopup = [&](int index, const QString &mode, const QString &label) {
        auto *combo = item("filmModeSelector");
        if (!combo || index < 0 || index > 1) return false;
        auto *popup = combo->property("popup").value<QObject *>();
        if (!popup) return false;
        combo->forceActiveFocus();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            combo->mapToScene(QPointF(combo->width()/2, combo->height()/2)).toPoint());
        if (!wait([&] { return popup->property("visible").toBool(); }, 2000)) return false;
        auto *popupContent = popup->property("contentItem").value<QQuickItem *>();
        auto *choice = findTextItem(popupContent, label);
        if (!choice || !choice->isVisible() || !choice->window()) return false;
        QTest::mouseClick(choice->window(), Qt::LeftButton, Qt::NoModifier,
            choice->mapToScene(QPointF(choice->width()/2, choice->height()/2)).toPoint());
        return wait([&] {
            return !popup->property("visible").toBool()
                && combo->property("currentIndex").toInt() == index
                && films.mode() == mode;
        }, 3000);
    };

    check("real-service-and-model-ready", wait([&] { return files.connected() && files.modelReady() && films.available(); }, 120000));
    check("loopback-fetch-detector-positive", canaryPositiveReads == 1 && canaryRequests == 1);
    check("film-controls-rendered", item("filmSearchField") && item("filmModeSelector") && item("filmResults") && item("filmPage"));
    check("desktop-window-render-capture", capture("desktop"));

    films.addCollection(QUrl::fromLocalFile(corpus));
    check("film-collection-added", wait([&] { return !films.collectionId().isEmpty() && !films.collections().isEmpty(); }));
    const QString collection = films.collectionId();
    auto importSidecar = [&](const QString &relative, bool analysis) {
        QSet<QString> previousJobs;
        for (const auto &row : films.jobs()) previousJobs.insert(row.toMap()["job_id"].toString());
        films.importFile(QUrl::fromLocalFile(corpus + "/" + relative), analysis);
        return wait([&] {
            for (const auto &row : films.jobs()) {
                const auto job = row.toMap();
                const QString wanted = analysis ? "analysis" : "manifest";
                const QString jobId = job["job_id"].toString();
                if (job["kind"].toString() == wanted && !previousJobs.contains(jobId)) {
                    const QString state = job["state"].toString();
                    if (state == "failed") importFailures << QString("%1 | job=%2 | %3")
                        .arg(relative, jobId, job["error"].toString());
                    return state == "complete";
                }
            }
            return false;
        });
    };
    bool importsOk = !collection.isEmpty();
    for (const auto &film : {"harbor", "harbor-cut", "orchard", "crossing", "archive", "parcel-pdf"})
        importsOk = importSidecar(QString::fromLatin1(film) + "/manifest.json", false) && importsOk;
    for (const auto &film : {"harbor", "orchard", "crossing"})
        importsOk = importSidecar(QString::fromLatin1(film) + "/analysis.json", true) && importsOk;
    importsOk = importSidecar("harbor/prior-ui.json", true) && importsOk;
    check("authored-sidecars-import-through-film-backend", importsOk);

    check("literal-source-query", searchFor("CLOCK-7320"));
    auto sourceRow = mapAt(films.results(), 0);
    const auto initialEvidence = sourceRow["evidence"].toList();
    check("literal-source-identity-and-default-hidden", sourceRow["film_id"] == "film:orchard"
        && sourceRow["source_exact"].toBool()
        && !initialEvidence.isEmpty()
        && std::all_of(initialEvidence.cbegin(), initialEvidence.cend(), [](const QVariant &v) {
            const auto e = v.toMap(); return e["source_excerpt"].isNull() && e["analysis_text"].isNull();
        }));
    if (!require("source-mode-control-popup-closed-and-mode-observed",
        selectModeThroughPopup(1, "source", "Source only")
            && item("filmPriorOptIn") && !item("filmPriorOptIn")->isVisible()
            && !films.includeUnverifiedPrior()
            && wait([&] { return !films.searching() && !films.results().isEmpty(); }))) return 1;
    if (!require("keyboard-down-second-up-first-index-and-focused-delegate-agree",
        downUpSelectionAndFocusAgree())) return 1;
    if (!require("keyboard-select-and-space-reveal", chooseFirstWithKeyboard())) return 1;
    QTest::keyClick(window, Qt::Key_Space);
    if (!require("source-evidence-revealed", wait([&] {
        const auto e = films.evidence();
        const auto ref = e.value("reference").toMap();
        return ref.value("film_id").toString() == "film:orchard"
            && e.value("relative_path").toString() == "orchard/orchard.srt"
            && e.value("source_excerpt").toString().contains("CLOCK-7320")
            && e.value("cue_start_ms").toInt() == 3000;
    }))) return 1;
    auto sourceEvidence = films.evidence();
    const QString cueUnit = sourceEvidence.value("unit_id").toString();
    check("subtitle-cue-unit-locator", sourceEvidence.value("cue_start_ms").toInt() == 3000
        && !cueUnit.isEmpty() && item("filmEvidenceUnit") && item("filmEvidenceUnit")->isVisible()
        && item("filmEvidenceUnit")->property("text").toString()
            == QString("Subtitle offset 3.000 s · %1 (playback alignment unverified)").arg(cueUnit));
    check("source-path-remains-separate", item("filmEvidenceLocator") && item("filmEvidenceLocator")->isVisible()
        && item("filmEvidenceLocator")->property("text").toString() == sourceEvidence.value("relative_path").toString());
    check("source-state-capture", capture("source-revealed"));
    if (!require("unicode-subtitle-search-and-keyboard-selection",
        searchFor("I kept a place for you.") && chooseFirstWithKeyboard()
            && mapAt(films.results(), 0).value("film_id").toString() == "film:crossing")) return 1;
    QTest::keyClick(window, Qt::Key_Space);
    const QString unicodeCue = QString::fromUtf8("Café 👩‍🚒: I kept a place for you.");
    if (!require("unicode-cue-reveal-preserves-authored-source-text", wait([&] {
        const auto e = films.evidence();
        return e.value("reference").toMap().value("film_id").toString() == "film:crossing"
            && e.value("cue_start_ms").toInt() == 4500
            && e.value("source_excerpt").toString() == "I kept a place for you."
            && e.value("unit_text").toString() == unicodeCue;
    }) && item("filmEvidenceUnit") && item("filmEvidenceUnit")->property("text").toString()
        == QString("Subtitle offset 4.500 s · %1 (playback alignment unverified)")
            .arg(films.evidence().value("unit_id").toString())
       && item("filmEvidenceLocator")
       && item("filmEvidenceLocator")->property("text").toString() == "crossing/crossing.vtt"
       && item("filmSourceExcerpt")
       && item("filmSourceExcerpt")->property("text").toString() == "I kept a place for you.")) return 1;
    check("unicode-cue-source-capture", capture("unicode-source-revealed"));
    if (!require("pdf-page-locator", searchFor("PARC-20942") && chooseFirstWithKeyboard())) return 1;
    QTest::keyClick(window, Qt::Key_Space);
    if (!require("pdf-page-two-unit-locator", wait([&] { return films.evidence().value("page").toInt() == 2; })
        && item("filmEvidenceUnit") && item("filmEvidenceUnit")->isVisible()
        && !films.evidence().value("unit_id").toString().isEmpty()
        && item("filmEvidenceUnit")->property("text").toString()
            == "PDF page 2 · " + films.evidence().value("unit_id").toString())) return 1;
    check("pdf-path-remains-separate", item("filmEvidenceLocator") && item("filmEvidenceLocator")->isVisible()
        && item("filmEvidenceLocator")->property("text").toString() == films.evidence().value("relative_path").toString());

    if (!require("hybrid-mode-control-popup-closed-and-mode-observed",
        selectModeThroughPopup(0, "hybrid", "Hybrid") && searchFor("ANALYSIS-ONLY-K7"))) return 1;
    check("source-mode-resets-prior-opt-in", !films.includeUnverifiedPrior());
    auto analysisRow = mapAt(films.results(), 0);
    const auto analysisLanes = analysisRow["lanes"].toList();
    auto *analysisMatchLabel = item("filmMatchLabel");
    check("hybrid-label-and-analysis-lane", analysisRow["film_id"] == "film:harbor"
        && std::any_of(analysisLanes.cbegin(), analysisLanes.cend(), [](const QVariant &v) {
            return v.toMap()["lane"].toString() == "analysis_lexical";
        }) && analysisMatchLabel && analysisMatchLabel->property("text").toString() == "Interpretation text match"
        && analysisMatchLabel->property("textFormat").toInt() == int(Qt::PlainText));
    check("hybrid-source-and-analysis-fields", chooseFirstWithKeyboard());
    QTest::keyClick(window, Qt::Key_Space);
    if (!require("analysis-reveal-shows-source-coverage-and-plain-text", wait([&] {
        const auto e = films.evidence();
        return !e.value("analysis_text").toString().isEmpty()
            && !e.value("source_excerpt").toString().isEmpty()
            && !e.value("coverage").toList().isEmpty();
    }) && item("filmAnalysisText") && item("filmSourceExcerpt")
       && item("filmEvidenceCoverage") && item("filmEvidenceCoverage")->isVisible()
       && item("filmEvidenceCoverage")->property("text").toString().contains("source:harbor:script")
       && item("filmEvidenceCoverage")->property("text").toString().contains("line-6"))) return 1;
    const auto authoredAnalysis = readObject(corpus + "/harbor/analysis.json");
    const auto authoredClaims = authoredAnalysis.value("claims").toArray();
    const auto authoredClaim = authoredClaims.isEmpty() ? QJsonObject{} : authoredClaims.first().toObject();
    const auto evidence = films.evidence();
    const auto evidenceReference = evidence.value("reference").toMap();
    const auto declaredGeneration = authoredAnalysis.value("generation").toObject();
    const auto displayedGeneration = evidence.value("generation").toMap();
    QString expectedMetadata = "Declared basis: " + authoredClaim.value("basis").toString().replace('_', ' ')
        + " · " + authoredClaim.value("facet").toString().replace('_', ' ')
        + " · " + authoredClaim.value("scope").toString();
    const QString declaredModel = declaredGeneration.value("model_id").toString();
    const QString declaredRevision = declaredGeneration.value("model_revision").toString();
    if (!declaredModel.isEmpty()) expectedMetadata += "\nDeclared model: " + declaredModel
        + (declaredRevision.isEmpty() ? QString() : " @ " + declaredRevision);
    const QString expectedUnobservable = "Not established from these sources: "
        + [&] { QStringList values; for (const auto &v : authoredAnalysis.value("not_observable").toArray()) values << v.toString(); return values.join(", "); }();
    auto *analysisIdentity = item("filmAnalysisIdentity");
    auto *generationMetadata = item("filmGenerationMetadata");
    auto *unobservable = item("filmUnobservable");
    auto *originLabel = findTextItem(item("filmPage"), "Declared origin: authored fixture · interpretation unreviewed");
    check("rendered-analysis-and-claim-identities-match-sidecar", !authoredClaim.isEmpty()
        && evidenceReference.value("analysis_id").toString() == authoredAnalysis.value("analysis_id").toString()
        && evidenceReference.value("claim_id").toString() == authoredClaim.value("claim_id").toString()
        && analysisIdentity && analysisIdentity->isVisible()
        && analysisIdentity->property("text").toString()
            == "Analysis: " + authoredAnalysis.value("analysis_id").toString()
                + "\nClaim: " + authoredClaim.value("claim_id").toString());
    check("rendered-basis-facet-scope-and-declared-model-match-sidecar", generationMetadata
        && generationMetadata->isVisible()
        && evidence.value("basis").toString() == authoredClaim.value("basis").toString()
        && evidence.value("facet").toString() == authoredClaim.value("facet").toString()
        && evidence.value("scope").toString() == authoredClaim.value("scope").toString()
        && evidence.value("origin").toString() == declaredGeneration.value("origin").toString()
        && displayedGeneration.value("origin").toString() == declaredGeneration.value("origin").toString()
        && displayedGeneration.value("model_id").toString() == declaredModel
        && generationMetadata->property("text").toString() == expectedMetadata);
    check("rendered-origin-and-not-observable-match-sidecar", originLabel && originLabel->isVisible()
        && unobservable && unobservable->isVisible()
        && unobservable->property("text").toString() == expectedUnobservable);
    check("analysis-state-capture", capture("analysis-revealed"));

    auto hasAnalysisOnly = [](const QVariantList &rows) {
        return std::any_of(rows.cbegin(), rows.cend(), [](const QVariant &row) {
            const auto evidence = row.toMap()["evidence"].toList();
            return std::any_of(evidence.cbegin(), evidence.cend(), [](const QVariant &item) {
                return item.toMap()["reference"].toMap()["reference_kind"].toString() == "analysis_only";
            });
        });
    };
    check("default-search-excludes-unverified-prior", searchFor("PRIOR-LANTERN-616")
        && !hasAnalysisOnly(films.results()));
    if (!require("exploratory-prior-checkbox-opt-in", item("filmPriorOptIn")
        && item("filmPriorOptIn")->isVisible() && !films.includeUnverifiedPrior()
        && click("filmPriorOptIn") && item("filmPriorOptIn")->property("checked").toBool()
        && films.includeUnverifiedPrior()
        && wait([&] { return !films.searching() && hasAnalysisOnly(films.results()); }))) return 1;
    const auto priorResults = films.results();
    int priorIndex = -1;
    QVariantMap priorRow;
    for (int i=0; i<priorResults.size(); ++i) {
        const auto candidate = priorResults[i].toMap();
        const auto evidenceRows = candidate["evidence"].toList();
        if (std::any_of(evidenceRows.cbegin(), evidenceRows.cend(), [](const QVariant &item) {
            return item.toMap()["reference"].toMap()["reference_kind"].toString() == "analysis_only";
        })) { priorIndex=i; priorRow=candidate; break; }
    }
    check("anchorless-prior-reference-has-no-source", priorIndex >= 0
        && priorRow["film_id"] == "film:harbor"
        && !priorRow["evidence"].toList().isEmpty()
        && priorRow["evidence"].toList().first().toMap()["reference"].toMap()["reference_kind"] == "analysis_only");
    auto chooseIndexWithKeyboard = [&](int index) {
        if (index < 0) return false;
        auto *field = item("filmSearchField"); auto *page = item("filmPage"); auto *results = item("filmResults");
        if (!field || !page || !results) return false;
        field->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Down);
        QElapsedTimer timer; timer.start();
        while (page->property("selectedIndex").toInt() < index && timer.elapsed() < 2000) QTest::keyClick(window, Qt::Key_Down);
        return page->property("selectedIndex").toInt() == index && results->hasActiveFocus();
    };
    check("anchorless-prior-reveal-is-labeled-without-navigation", chooseIndexWithKeyboard(priorIndex));
    QTest::keyClick(window, Qt::Key_Space);
    if (!require("prior-plain-text-unverified-and-no-source", wait([&] {
        const auto e = films.evidence();
        return e["basis"].toString() == "unverified_prior"
            && e["label"].toString() == "Unverified prior; no source anchor"
            && e["source_excerpt"].isNull() && !e["analysis_text"].toString().isEmpty();
    }) && item("filmAnalysisText") && item("filmAnalysisText")->property("textFormat").toInt() == int(Qt::PlainText)
       && item("filmAnalysisText")->property("text").toString().contains("<img src=\"http://127.0.0.1:")
       && item("filmOpenSource") && !item("filmOpenSource")->isEnabled()
       && item("filmRevealSource") && !item("filmRevealSource")->isEnabled())) return 1;
    if (!require("anchorless-prior-has-no-unit-locator-or-undefined-warning", item("filmEvidenceUnit")
        && item("filmEvidenceUnit")->property("text").toString().isEmpty()
        && item("filmEvidenceLocator")
        && item("filmEvidenceLocator")->property("text").toString() == "Unverified prior; no source anchor"
        && warnings.isEmpty())) return 1;
    QTest::qWait(300);
    check("hostile-prior-text-is-literal-without-fetch", canaryHostileReads == 0);
    check("prior-state-capture", capture("prior-revealed"));
    if (!require("exploratory-prior-opt-out-clears-selection", click("filmPriorOptIn")
        && !item("filmPriorOptIn")->property("checked").toBool()
        && !films.includeUnverifiedPrior()
        && wait([&] { return films.results().isEmpty() && films.evidence().isEmpty(); }))) return 1;
    check("prior-is-excluded-again-after-opt-out", searchFor("PRIOR-LANTERN-616")
        && !hasAnalysisOnly(films.results()));
    bool staleImmediatelyCleared = false;
    auto *priorSearch = item("filmSearchField");
    if (priorSearch) {
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            priorSearch->mapToScene(QPointF(priorSearch->width()/2, priorSearch->height()/2)).toPoint());
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_N);
        staleImmediatelyCleared = films.results().isEmpty() && films.evidence().isEmpty();
    }
    check("stale-selection-clears-on-first-query-edit", staleImmediatelyCleared);
    const bool noAnswerSettled = settleQuery("lunar flight telemetry QUASAR-9817");
    const int noAnswerSuggestions = films.results().size();
    check("no-answer-query-and-suggestions-recorded", noAnswerSettled);
    check("narrow-window-render-capture", [&] {
        window->resize(720, 520);
        return wait([&] {
            auto *page = item("filmPage"); auto *panel = item("filmEvidencePanel");
            if (!page || !panel || panel->height() < 200) return false;
            const auto pageAt = page->mapToScene(QPointF());
            const auto panelAt = panel->mapToScene(QPointF());
            return page->width() <= 720 && pageAt.x() >= -1 && pageAt.x()+page->width() <= window->width()+1
                && panelAt.x() >= pageAt.x()-1 && panelAt.x()+panel->width() <= pageAt.x()+page->width()+1;
        }, 5000)
            && capture("narrow");
    }());

    const bool detailsClicked = click("filmStatusDetails");
    auto *statusDialog = window->findChild<QObject *>("filmStatusDialog");
    check("native-status-and-imports-dialog-open", detailsClicked
        && wait([&] { return statusDialog && statusDialog->property("visible").toBool(); }));
    QStringList dialogTexts;
    if (statusDialog) collectTextProperties(statusDialog, &dialogTexts);
    collectVisualTexts(window->contentItem(), &dialogTexts);
    const QString fullStatusMessage = films.message();
    check("status-dialog-renders-current-full-message", !fullStatusMessage.isEmpty()
        && dialogTexts.contains(fullStatusMessage));
    bool allJobsRendered = !films.jobs().isEmpty();
    for (const auto &row : films.jobs()) {
        const auto job = row.toMap();
        QString expected = job.value("kind").toString() + " · " + job.value("state").toString()
            + " · " + job.value("stage").toString();
        if (!job.value("error").toString().isEmpty()) expected += " — " + job.value("error").toString();
        allJobsRendered = allJobsRendered && dialogTexts.contains(expected);
    }
    check("status-dialog-renders-each-current-import-job", allJobsRendered);
    bool allWarningsRendered = true;
    for (const auto &warning : films.warnings()) {
        const auto map = warning.toMap();
        const QString errorText = map.value("error").toString();
        const QString warningValue = map.value("warning").toString();
        const QString warningText = warning.typeId() == QMetaType::QString ? warning.toString()
            : !errorText.isEmpty() ? errorText : !warningValue.isEmpty() ? warningValue : "Collection warning";
        allWarningsRendered = allWarningsRendered && dialogTexts.contains(warningText);
    }
    check("status-dialog-renders-current-warnings", allWarningsRendered);
    check("status-dialog-capture", capture("status-dialog"));
    const bool closeInvoked = statusDialog && QMetaObject::invokeMethod(statusDialog, "close", Qt::DirectConnection);
    check("native-status-and-imports-dialog-closed", closeInvoked
        && wait([&] { return statusDialog && !statusDialog->property("visible").toBool(); }));

    check("legacy-files-mode-restored", click("fileModeButton") && wait([&] { return !window->property("filmMode").toBool(); }));
    files.addRoot(QUrl::fromLocalFile(corpus));
    check("legacy-real-fixture-indexed", wait([&] { return files.modelReady() && !files.busy() && files.indexed() > 0; }, 120000));
    auto *legacySearch = item("searchField");
    bool legacyTyped = legacySearch && legacySearch->isVisible();
    if (legacyTyped) {
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            legacySearch->mapToScene(QPointF(legacySearch->width()/2, legacySearch->height()/2)).toPoint());
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        legacyTyped = typeText("The copper seal is broken.");
    }
    check("legacy-fixture-query-still-searches", wait([&] {
        const auto rows = files.results();
        return legacyTyped && !files.searching() && std::any_of(rows.cbegin(), rows.cend(), [](const QVariant &v) {
            return v.toMap()["path"].toString().endsWith("archive/archive.md");
        });
    }));

    check("films-mode-restored-for-lifecycle-controls", click("filmModeButton")
        && wait([&] { return window->property("filmMode").toBool() && films.available(); }));

    if (!geometryOnly) {
        const bool sourceModeForU02 = selectModeThroughPopup(1, "source", "Source only");
        check("u02-native-source-mode-before-background-control", sourceModeForU02 && films.mode() == "source");

        QSet<QString> priorJobIds;
        for (const auto &row : films.jobs()) priorJobIds.insert(row.toMap().value("job_id").toString());
        films.importFile(QUrl::fromLocalFile(backgroundManifestPath), false);
        QString backgroundJobId;
        const bool backgroundJobAccepted = wait([&] {
            for (const auto &row : films.jobs()) {
                const auto job = row.toMap();
                const auto id = job.value("job_id").toString();
                if (job.value("kind").toString() == "manifest" && !priorJobIds.contains(id)) {
                    backgroundJobId = id;
                    return true;
                }
            }
            return false;
        }, 30000);
        auto backgroundJobRow = [&]() {
            for (const auto &row : films.jobs()) {
                const auto job = row.toMap();
                if (job.value("job_id").toString() == backgroundJobId) return job;
            }
            return QVariantMap{};
        };
        const bool backgroundEmbedBefore = backgroundJobAccepted && wait([&] {
            const auto job = backgroundJobRow();
            return job.value("state").toString() == "running" && job.value("stage").toString() == "embed";
        }, 120000);
        const auto embedStateBefore = backgroundJobRow();
        const qint64 embedBeforeNs = monotonicNowNs();

        QTimer uiHeartbeat;
        uiHeartbeat.setInterval(40);
        int uiHeartbeatTicks = 0;
        qint64 lastHeartbeatNs = 0;
        qint64 maxHeartbeatGapNs = 0;
        QObject::connect(&uiHeartbeat, &QTimer::timeout, &app, [&] {
            const qint64 now = monotonicNowNs();
            if (lastHeartbeatNs) maxHeartbeatGapNs = qMax(maxHeartbeatGapNs, now - lastHeartbeatNs);
            lastHeartbeatNs = now;
            ++uiHeartbeatTicks;
        });

        const QString runnerHelper = root + "/build-runner-discovery-v1-helper/installed-runner-v2";
        const QString runnerPluginRoot = root + "/local/release-rc2-stage-v1/usr/lib/qt6/plugins";
        const QString runnerPlugin = runnerPluginRoot + "/kf6/krunner/kde-index-runner.so";
        const QString runnerConfig = output + "/runner.ini";
        const QString runnerPaymentPath = paymentPath;
        const bool runnerInputsPinned = sha256(runnerHelper)
                == "6331f961d570f0a607eaf600465970aea86c5285f3bb9c4ebe9d9c4c980ff0b5"
            && sha256(runnerPlugin)
                == "a587a20400e4892c0eded5af30c08e646ea09885a971eb3a6a6e6c61bda2e9ba"
            && QFileInfo::exists(runnerPaymentPath);
        QProcess runner;
        QProcessEnvironment runnerEnvironment = QProcessEnvironment::systemEnvironment();
        for (const auto &directory : {QString("runner-xdg-config"), QString("runner-xdg-cache"), QString("runner-xdg-data")})
            QDir().mkpath(output + "/" + directory);
        runnerEnvironment.insert("XDG_CONFIG_HOME", output + "/runner-xdg-config");
        runnerEnvironment.insert("XDG_CACHE_HOME", output + "/runner-xdg-cache");
        runnerEnvironment.insert("XDG_DATA_HOME", output + "/runner-xdg-data");
        runnerEnvironment.insert("KDE_INDEX_SOCKET", socket);
        runner.setProcessEnvironment(runnerEnvironment);
        runner.setProgram(runnerHelper);
        runner.setArguments({runnerPluginRoot, runnerConfig, socket, "find CARD-39312", runnerPaymentPath});
        runner.setProcessChannelMode(QProcess::SeparateChannels);
        qint64 runnerPid = 0;
        bool runnerStarted = false;
        QObject::connect(&runner, &QProcess::started, &app, [&] {
            runnerStarted = true;
            runnerPid = runner.processId();
        });
        const qint64 runnerStartNs = monotonicNowNs();
        if (backgroundEmbedBefore) uiHeartbeat.start();
        if (backgroundEmbedBefore && runnerInputsPinned) runner.start();
        const qint64 guiQueryStartNs = monotonicNowNs();
        const bool guiQueryTyped = backgroundEmbedBefore && setSearch("CLOCK-7320");
        bool guiQueryFinished = false;
        qint64 guiQueryEndNs = 0;
        bool runnerFinished = false;
        qint64 runnerEndNs = 0;
        QElapsedTimer u02Wait; u02Wait.start();
        while (u02Wait.elapsed() < 120000 && (!guiQueryFinished || !runnerFinished)) {
            QTest::qWait(20);
            const auto top = mapAt(films.results(), 0);
            if (!guiQueryFinished && guiQueryTyped && !films.searching() && !top.isEmpty()
                && top.value("film_id").toString() == "film:orchard" && top.value("source_exact").toBool()) {
                guiQueryFinished = true;
                guiQueryEndNs = monotonicNowNs();
            }
            if (!runnerFinished && runner.state() == QProcess::NotRunning) {
                runnerFinished = true;
                runnerEndNs = monotonicNowNs();
            }
        }
        if (runner.state() != QProcess::NotRunning) {
            runner.terminate();
            if (!runner.waitForFinished(1500)) { runner.kill(); runner.waitForFinished(3000); }
            runnerFinished = false;
            runnerEndNs = monotonicNowNs();
        }
        if (guiQueryEndNs == 0) guiQueryEndNs = monotonicNowNs();
        if (runnerEndNs == 0) runnerEndNs = monotonicNowNs();
        const auto runnerStdout = runner.readAllStandardOutput();
        const auto runnerStderr = runner.readAllStandardError();
        const auto runnerError = runner.errorString();
        QJsonParseError runnerParseError;
        const auto runnerDoc = QJsonDocument::fromJson(runnerStdout, &runnerParseError);
        const auto runnerResult = runnerDoc.object();
        const auto runnerMatches = runnerResult.value("matches").toArray();
        const auto runnerFirst = runnerMatches.isEmpty() ? QJsonObject{} : runnerMatches.first().toObject();
        const auto runnerData = runnerFirst.value("data").toObject();
        const bool runnerCorrect = runnerFinished && runner.exitCode() == 0
            && runnerParseError.error == QJsonParseError::NoError
            && runnerResult.value("pass").toBool()
            && runnerResult.value("off_main_thread").toBool()
            && runnerData.value("path").toString() == runnerPaymentPath
            && (runnerFirst.value("subtext").toString() + " " + runnerFirst.value("text").toString()).contains("CARD-39312");
        const auto embedStateAfter = backgroundJobRow();
        const bool backgroundEmbedAfter = embedStateAfter.value("state").toString() == "running"
            && embedStateAfter.value("stage").toString() == "embed";
        const qint64 embedAfterNs = monotonicNowNs();
        double heartbeatMaxGapMs = 0.0;

        u02Report = QJsonObject{{"scope", "native GUI keyboard search and installed KRunner query while UI-client manifest job reports stage=embed"},
            {"clock", "CLOCK_MONOTONIC via std::chrono::steady_clock"},
            {"ui_peer_pid", QCoreApplication::applicationPid()}, {"runner_peer_pid", double(runnerPid)},
            {"background_job_id", backgroundJobId}, {"background_job_state_before", embedStateBefore.value("state").toString()},
            {"background_job_stage_before", embedStateBefore.value("stage").toString()},
            {"background_job_state_after", embedStateAfter.value("state").toString()},
            {"background_job_stage_after", embedStateAfter.value("stage").toString()},
            {"background_embed_observed_before", backgroundEmbedBefore}, {"background_embed_observed_after", backgroundEmbedAfter},
            {"background_manifest_path", backgroundManifestPath}, {"background_source_sha256", backgroundSourceSha},
            {"background_source_line_count", backgroundSourceLines}, {"background_source_bytes", double(backgroundSourceBytes)},
            {"background_source_max_bytes", 2 * 1024 * 1024},
            {"background_embed_before_mono_ns", double(embedBeforeNs)}, {"background_embed_after_mono_ns", double(embedAfterNs)},
            {"gui_query_start_mono_ns", double(guiQueryStartNs)}, {"gui_query_end_mono_ns", double(guiQueryEndNs)},
            {"gui_query_input", "CLOCK-7320"}, {"gui_query_typed_through_qtest_keys", guiQueryTyped},
            {"gui_query_result_film_id", mapAt(films.results(), 0).value("film_id").toString()},
            {"gui_query_result_source_exact", mapAt(films.results(), 0).value("source_exact").toBool()},
            {"runner_helper_sha256", QString::fromLatin1(sha256(runnerHelper))},
            {"runner_plugin_sha256", QString::fromLatin1(sha256(runnerPlugin))},
            {"runner_helper_path", runnerHelper}, {"legacy_payment_path", runnerPaymentPath},
            {"runner_argv", QJsonArray{runnerHelper, runnerPluginRoot, runnerConfig, socket, "find CARD-39312", runnerPaymentPath}},
            {"runner_inputs_pinned", runnerInputsPinned}, {"runner_started", runnerStarted}, {"runner_error", runnerError},
            {"runner_exit_code", runner.exitCode()}, {"runner_result", runnerResult}, {"runner_stderr", QString::fromUtf8(runnerStderr)},
            {"runner_correct", runnerCorrect}, {"runner_start_mono_ns", double(runnerStartNs)},
            {"runner_end_mono_ns", double(runnerEndNs)},
            {"ui_heartbeat_ticks", uiHeartbeatTicks}, {"ui_heartbeat_max_gap_ms", heartbeatMaxGapMs},
            {"ui_heartbeat_observed", uiHeartbeatTicks >= 10 && heartbeatMaxGapMs < 500.0},
            {"relay_records_path", qEnvironmentVariable("FILM_UI_RELAY_RECORDS")},
            {"legacy_payment_sha256", QString::fromLatin1(sha256(runnerPaymentPath))}};
        check("u02-background-source-is-256-lines-under-2MiB", backgroundSourceLines == 256
            && backgroundSourceBytes > 0 && backgroundSourceBytes < 2 * 1024 * 1024);
        check("u02-import-job-reports-actual-embed-stage-before-controls", backgroundEmbedBefore);
        check("u02-gui-keyboard-query-finishes-with-authored-exact-result", guiQueryFinished);
        check("u02-actual-native-krunner-manager-query", runnerInputsPinned && runnerStarted && runnerCorrect);
        check("u02-gui-heartbeat-runs-during-controls", uiHeartbeatTicks >= 10 && maxHeartbeatGapNs < 500'000'000);
        check("u02-same-ui-job-still-in-embed-stage-after-measured-queries", backgroundEmbedAfter);
        check("u02-native-controls-capture", capture("u02-native-controls"));

        const QString lateQuery = "They erased my name from the rescue report.";
        const bool lateOldTyped = setSearch(lateQuery);
        const auto heldOld = lateOldTyped ? waitRelayEvent("successful_reply_held", [&](const QJsonObject &row) {
            const auto request = row.value("request").toObject();
            return row.value("peer").toObject().value("pid").toInt() == QCoreApplication::applicationPid()
                && request.value("op").toString() == "film_search"
                && request.value("text").toString().compare(lateQuery, Qt::CaseInsensitive) == 0;
        }, 30000) : QJsonObject{};
        const auto heldReply = heldOld.value("reply").toObject();
        bool heldHarborDraft = false;
        for (const auto &value : heldReply.value("results").toArray()) {
            const auto row = value.toObject();
            if (row.value("edition_id").toString() != "edition:harbor:draft-1") continue;
            for (const auto &ev : row.value("evidence").toArray())
                if (ev.toObject().value("relative_path").toString() == "harbor/harbor.txt") heldHarborDraft = true;
        }
        const bool oldStillPendingInUi = films.searching();
        const bool freshTyped = lateOldTyped && !heldOld.isEmpty() && heldReply.value("ok").toBool()
            && heldHarborDraft && oldStillPendingInUi && setSearch("CLOCK-7320");
        const QString oldRequestId = heldOld.value("request").toObject().value("id").toString();
        const auto cancelForwarded = waitRelayEvent("request_forwarded", [&](const QJsonObject &row) {
            const auto request = row.value("request").toObject();
            return row.value("peer").toObject().value("pid").toInt() == QCoreApplication::applicationPid()
                && request.value("op").toString() == "cancel"
                && request.value("target").toString() == oldRequestId
                && row.value("connection") == heldOld.value("connection");
        }, 5000);
        const QString cancelRequestId = cancelForwarded.value("request_id").toString();
        const auto cancelReply = cancelRequestId.isEmpty() ? QJsonObject{}
            : waitRelayEvent("reply_forwarded", [&](const QJsonObject &row) {
                return row.value("connection") == heldOld.value("connection")
                    && row.value("request").toObject().value("id").toString() == cancelRequestId;
            }, 5000);
        const bool freshSettled = freshTyped && wait([&] {
            const auto top = mapAt(films.results(), 0);
            return !films.searching() && top.value("film_id").toString() == "film:orchard"
                && top.value("source_exact").toBool();
        }, 30000);
        const auto oldDelivered = waitRelayEvent("held_successful_reply_delivered_after_fresh_reply",
            [&](const QJsonObject &row) { return row.value("request_id").toString() == oldRequestId; }, 15000);
        QTest::qWait(100);
        const auto staleTop = mapAt(films.results(), 0);
        const QString freshRequestId = oldDelivered.value("fresh_request_id").toString();
        const bool rawOldWasDeliveredAfterDistinctFreshResult = !oldDelivered.isEmpty()
            && oldDelivered.value("request_id").toString() == oldRequestId
            && oldDelivered.value("raw_reply").toString() == heldOld.value("raw_reply").toString()
            && oldDelivered.value("reply_forwarded_mono_ns").toDouble()
                > oldDelivered.value("fresh_reply_forwarded_mono_ns").toDouble()
            && freshRequestId != oldRequestId;
        const bool staleResponseIgnored = freshSettled && rawOldWasDeliveredAfterDistinctFreshResult
            && staleTop.value("film_id").toString() == "film:orchard"
            && staleTop.value("source_exact").toBool();
        lateResponseReport = QJsonObject{{"old_query", lateQuery}, {"fresh_query", "CLOCK-7320"},
            {"old_request_forwarded", !heldOld.isEmpty()}, {"old_request_id", oldRequestId},
            {"old_connection", heldOld.value("connection")}, {"old_peer", heldOld.value("peer")},
            {"old_upstream_peer", heldOld.value("upstream_peer")},
            {"old_reply_successful", heldReply.value("ok").toBool()},
            {"old_reply_raw", heldOld.value("raw_reply")}, {"old_reply_has_exact_harbor_source", heldHarborDraft},
            {"old_still_pending_in_ui_before_new_query", oldStillPendingInUi},
            {"cancel_request_forwarded", !cancelForwarded.isEmpty()},
            {"cancel_request", cancelForwarded.value("request")},
            {"cancel_request_connection", cancelForwarded.value("connection")},
            {"cancel_reply", cancelReply.value("reply")},
            {"cancel_reply_accepted", cancelReply.value("reply").toObject().value("accepted").toBool()},
            {"cancel_follows_actual_old_reply", cancelForwarded.value("request_forwarded_mono_ns").toDouble()
                > heldOld.value("reply_mono_ns").toDouble()},
            {"fresh_request_id", freshRequestId}, {"fresh_settled", freshSettled},
            {"fresh_film_id", staleTop.value("film_id").toString()},
            {"held_old_bytes_delivered_after_fresh_reply", rawOldWasDeliveredAfterDistinctFreshResult},
            {"stale_old_response_left_fresh_result_unchanged", staleResponseIgnored},
            {"old_delivery_receipt", oldDelivered}};
        check("late-search-actual-old-request-and-successful-raw-reply-held", lateOldTyped
            && !heldOld.isEmpty() && heldReply.value("ok").toBool() && heldHarborDraft && oldStillPendingInUi);
        check("late-search-fresh-query-completes-before-unchanged-old-bytes-delivery", freshSettled
            && !cancelForwarded.isEmpty() && !cancelReply.isEmpty()
            && cancelReply.value("reply").toObject().value("accepted").toBool() == false
            && rawOldWasDeliveredAfterDistinctFreshResult && staleResponseIgnored);
        check("late-response-native-capture", capture("late-response-fresh-result"));

        uiHeartbeat.stop();
        heartbeatMaxGapMs = double(maxHeartbeatGapNs) / 1'000'000.0;
        u02Report["ui_heartbeat_ticks"] = uiHeartbeatTicks;
        u02Report["ui_heartbeat_max_gap_ms"] = heartbeatMaxGapMs;
        u02Report["ui_heartbeat_observed"] = uiHeartbeatTicks >= 10 && heartbeatMaxGapMs < 500.0;
        check("u02-heartbeat-remains-responsive-through-late-reply-delivery",
            uiHeartbeatTicks >= 10 && heartbeatMaxGapMs < 500.0);

        bool backgroundCanceled = false;
        if (backgroundJobAccepted) {
            const auto current = backgroundJobRow();
            if (current.value("state").toString() == "pending" || current.value("state").toString() == "running") {
                films.cancelJob(backgroundJobId);
                backgroundCanceled = wait([&] { return backgroundJobRow().value("state").toString() == "canceled"; }, 30000);
            }
        }
        u02Report["background_job_cancelled_after_controls"] = backgroundCanceled;
        check("u02-background-job-canceled-after-controls", backgroundCanceled);
        const bool noBackgroundResultQuerySettled = settleQuery("Background embedding control row 041");
        const auto backgroundAfterCancelResults = films.results();
        const bool noPartialPromotion = noBackgroundResultQuerySettled
            && std::none_of(backgroundAfterCancelResults.cbegin(), backgroundAfterCancelResults.cend(), [](const QVariant &row) {
                return row.toMap().value("film_id").toString() == "film:u02-background-control";
            });
        check("u02-canceled-background-edition-has-no-partial-search-promotion", noPartialPromotion);
        check("u02-return-to-hybrid-after-source-and-runner-controls",
            selectModeThroughPopup(0, "hybrid", "Hybrid") && films.mode() == "hybrid");

        const QString collectionBeforeRestart = films.collectionId();
        const bool priorSearchReady = setSearch("PRIOR-LANTERN-616");
        const bool priorEnabledByNativeControl = item("filmPriorOptIn")
            && !item("filmPriorOptIn")->property("checked").toBool()
            && click("filmPriorOptIn") && wait([&] { return films.includeUnverifiedPrior() && !films.searching(); });
        int restartPriorIndex = -1;
        for (int i=0; i<films.results().size(); ++i) {
            const auto refs = films.results()[i].toMap().value("evidence").toList();
            if (std::any_of(refs.cbegin(), refs.cend(), [](const QVariant &value) {
                return value.toMap().value("reference").toMap().value("reference_kind").toString() == "analysis_only";
            })) { restartPriorIndex = i; break; }
        }
        const bool priorEvidenceSelected = priorSearchReady && priorEnabledByNativeControl
            && restartPriorIndex >= 0 && chooseIndexWithKeyboard(restartPriorIndex);
        if (priorEvidenceSelected) QTest::keyClick(window, Qt::Key_Space);
        const bool priorEvidenceVisible = priorEvidenceSelected && wait([&] {
            return films.evidence().value("basis").toString() == "unverified_prior"
                && !films.evidence().value("analysis_text").toString().isEmpty();
        });
        const QString requestPath = qEnvironmentVariable("FILM_UI_RESTART_REQUEST");
        const QString receiptPath = qEnvironmentVariable("FILM_UI_RESTART_RECEIPT");
        const QString restartNonce = QString::number(monotonicNowNs());
        const bool restartRequestWritten = priorEvidenceVisible && films.includeUnverifiedPrior()
            && !films.evidence().isEmpty() && !requestPath.isEmpty() && !receiptPath.isEmpty()
            && writeJsonFile(requestPath, QJsonObject{{"action", "controlled_upstream_restart"},
                {"nonce", restartNonce}, {"ui_pid", QCoreApplication::applicationPid()},
                {"collection_id", collectionBeforeRestart}, {"prior_opt_in", films.includeUnverifiedPrior()},
                {"evidence_present", !films.evidence().isEmpty()}, {"request_mono_ns", double(monotonicNowNs())}});
        const bool restartPreconditions = collectionBeforeRestart.size() > 0 && priorEvidenceVisible
            && films.includeUnverifiedPrior() && !films.evidence().isEmpty() && restartRequestWritten;
        check("wrapper-restart-precondition-has-native-opted-prior-evidence", restartPreconditions);
        const bool disconnectInvalidatedPriorState = restartPreconditions && wait([&] {
            return !films.available() && !films.includeUnverifiedPrior()
                && films.results().isEmpty() && films.evidence().isEmpty();
        }, 15000);
        const bool restartReceiptWritten = restartPreconditions && wait([&] {
            return QFileInfo::exists(receiptPath) && !readObject(receiptPath).isEmpty();
        }, 120000);
        const auto restartReceipt = restartReceiptWritten ? readObject(receiptPath) : QJsonObject{};
        const bool backendReconnected = restartReceiptWritten && wait([&] {
            return films.available() && !films.collections().isEmpty()
                && films.collectionId() == collectionBeforeRestart;
        }, 120000);
        const bool defaultPriorRestored = backendReconnected && item("filmPriorOptIn")
            && !item("filmPriorOptIn")->property("checked").toBool() && !films.includeUnverifiedPrior();
        const bool postRestartQuerySent = backendReconnected && setSearch("CLOCK-7320");
        const bool postRestartResult = postRestartQuerySent && wait([&] {
            const auto top = mapAt(films.results(), 0);
            return !films.searching() && top.value("film_id").toString() == "film:orchard"
                && top.value("source_exact").toBool();
        }, 30000);
        restartReport = QJsonObject{{"wrapper_restart_request_written", restartRequestWritten},
            {"disconnect_cleared_prior_opt_in_results_evidence", disconnectInvalidatedPriorState},
            {"restart_receipt_written", restartReceiptWritten}, {"restart_receipt", restartReceipt},
            {"collection_id_before", collectionBeforeRestart}, {"collection_id_after", films.collectionId()},
            {"backend_reconnected_ready", backendReconnected}, {"default_prior_policy_restored", defaultPriorRestored},
            {"post_restart_fresh_query_sent", postRestartQuerySent},
            {"post_restart_exact_source_result", postRestartResult},
            {"post_restart_result_film_id", mapAt(films.results(), 0).value("film_id").toString()},
            {"post_restart_evidence_empty_before_new_query", disconnectInvalidatedPriorState}};
        check("controlled-wrapper-restart-observed-clean-exit-socket-recreate-and-distinct-pid",
            restartReceiptWritten && restartReceipt.value("old_pid").toInt() != restartReceipt.value("new_pid").toInt()
                && restartReceipt.value("old_exit_code").toInt() == 0
                && restartReceipt.value("socket_released").toBool()
                && restartReceipt.value("socket_recreated").toBool()
                && restartReceipt.value("same_argv").toBool()
                && restartReceipt.value("same_data_path").toBool()
                && restartReceipt.value("same_upstream_socket").toBool());
        check("film-backend-reconnects-ready-with-collection-and-default-prior-policy",
            backendReconnected && defaultPriorRestored && films.collectionId() == collectionBeforeRestart);
        check("film-backend-fresh-source-search-after-wrapper-restart", postRestartResult);
        check("controlled-restart-native-capture", capture("after-controlled-service-restart"));
    }

    check("excluded-source-was-searchable", searchFor("They erased my name from the rescue report.")
        && [&] {
            for (const auto &entry : films.results()) {
                const auto row = entry.toMap();
                if (row.value("edition_id").toString() != "edition:harbor:draft-1") continue;
                for (const auto &evidence : row.value("evidence").toList())
                    if (evidence.toMap().value("relative_path").toString() == "harbor/harbor.txt") return true;
            }
            return false;
        }());
    films.setExclusions("harbor/harbor.txt");
    const bool exclusionSaved = wait([&] {
        const auto rows = films.collections();
        if (rows.isEmpty()) return false;
        const auto exclusions = rows.first().toMap()["exclusions"].toStringList();
        return exclusions.contains("harbor/harbor.txt");
    });
    const bool excludedLiteralSettled = settleQuery("They erased my name from the rescue report.");
    const auto excludedLiteralRows = films.results();
    const bool excludedDraftSourceRemains = std::any_of(excludedLiteralRows.cbegin(), excludedLiteralRows.cend(), [](const QVariant &entry) {
        const auto row = entry.toMap();
        if (row.value("edition_id").toString() == "edition:harbor:draft-1") return true;
        for (const auto &value : row.value("evidence").toList()) {
            const auto evidence = value.toMap();
            const auto reference = evidence.value("reference").toMap();
            if (evidence.value("relative_path").toString() == "harbor/harbor.txt"
                || reference.value("source_id").toString() == "source:harbor:script") return true;
        }
        return false;
    });
    const bool alternateCutPreserved = std::any_of(excludedLiteralRows.cbegin(), excludedLiteralRows.cend(), [](const QVariant &entry) {
        const auto row = entry.toMap();
        if (row.value("edition_id").toString() != "edition:harbor:alternate-cut") return false;
        const auto evidenceRows = row.value("evidence").toList();
        return std::any_of(evidenceRows.cbegin(), evidenceRows.cend(), [](const QVariant &value) {
            return value.toMap().value("relative_path").toString() == "harbor-cut/harbor.txt";
        });
    });
    check("collection-exclusion-removes-exact-draft-source-and-preserves-alternate-cut",
        exclusionSaved && excludedLiteralSettled && !excludedDraftSourceRemains && alternateCutPreserved);
    const bool excludedAnalysisSettled = settleQuery("ANALYSIS-ONLY-K7");
    const auto excludedAnalysisRows = films.results();
    const bool excludedAnalysisRemains = std::any_of(excludedAnalysisRows.cbegin(), excludedAnalysisRows.cend(), [](const QVariant &entry) {
        const auto row = entry.toMap();
        for (const auto &value : row.value("evidence").toList()) {
            const auto evidence = value.toMap();
            const auto reference = evidence.value("reference").toMap();
            if (reference.value("analysis_id").toString() == "analysis:harbor:1"
                || reference.value("source_id").toString() == "source:harbor:script"
                || evidence.value("relative_path").toString() == "harbor/harbor.txt") return true;
        }
        return false;
    });
    check("collection-exclusion-withdraws-dependent-analysis", excludedAnalysisSettled && !excludedAnalysisRemains);
    films.selectCollection(collection);
    films.removeCollection();
    check("collection-removal-clears-film-index", wait([&] {
        return films.collectionId().isEmpty() && films.collections().isEmpty();
    }));
    check("final-window-capture", capture("final"));
    files.closePreview();
    files.shutdownOwnedService();
    films.setEnabled(false);
    QTest::qWait(350);
    QJsonArray warningRows; for (const auto &warning : warnings) warningRows.append(warning);
    QJsonArray importFailureRows; for (const auto &failure : importFailures) importFailureRows.append(failure);
    check("qml-runtime-has-no-warnings", warnings.isEmpty());
    QJsonObject report{{"pass", passed}, {"checks", checks}, {"qml_warnings", warningRows},
        {"keyboard_focus_observations", QJsonArray::fromStringList(keyboardFocusObservations)},
        {"import_failures", importFailureRows},
        {"legacy_input_pins", legacyInputPins}, {"u02", u02Report},
        {"late_response", lateResponseReport}, {"controlled_restart", restartReport},
        {"loopback_requests", canaryRequests}, {"loopback_hostile_requests", canaryHostileReads},
        {"no_answer_suggestions", noAnswerSuggestions},
        {"service_sha256", QString::fromLatin1(sha256(service))},
        {"probe_sha256", QString::fromLatin1(sha256(probe))},
        {"tokenizer_sha256", QString::fromLatin1(sha256(QFileInfo(service).dir().filePath("semanticlens-film-tokenizer")))},
        {"text_model_sha256", QString::fromLatin1(sha256(models + "/embeddinggemma-2-Q8_0.gguf"))},
        {"image_model_sha256", QString::fromLatin1(sha256(models + "/mmproj-embeddinggemma-2-Q8_0.gguf"))},
        {"elapsed_seconds", double(runTimer.elapsed()) / 1000.0},
        {"usage_measurement_scope", "Harness elapsed time only; model-token and package accounting unavailable."},
        {"source_root", "<project source tree>"}, {"socket", socket}, {"corpus", corpus},
        {"window_width", window->width()}, {"window_height", window->height()}};
    const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented) + '\n';
    QFile receipt(output + "/results.json");
    if (!receipt.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 2;
    receipt.write(bytes); receipt.close();
    std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout);
    return passed ? 0 : 1;
}
