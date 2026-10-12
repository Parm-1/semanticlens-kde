// Real-service interaction/render check. Use ONLY an owned synthetic corpus:
// the check writes test fixtures. No personal folder is appropriate.
#include "backend.h"
#include <QApplication>
#include <KIconTheme>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTest>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <functional>
#include <cstdio>

int main(int argc, char **argv) {
    KIconTheme::initTheme(); QQuickStyle::setStyle("org.kde.desktop");
    QApplication app(argc, argv);
    if(argc!=9) {fprintf(stderr,"ui-redesign SOURCE SERVICE PROBE MODELS DATA SOCKET CORPUS OUTPUT\n");return 2;}
    const auto root=QString::fromLocal8Bit(argv[1]), corpus=QString::fromLocal8Bit(argv[7]), output=QString::fromLocal8Bit(argv[8]);
    QDir().mkpath(output);
    Backend backend({QString::fromLocal8Bit(argv[6]),QString::fromLocal8Bit(argv[2]),QString::fromLocal8Bit(argv[3]),QString::fromLocal8Bit(argv[4]),QString::fromLocal8Bit(argv[5]),""});
    QQmlApplicationEngine engine; engine.rootContext()->setContextProperty("backend",&backend);
    QJsonArray checks; QStringList warnings; bool pass=true;
    QObject::connect(&engine,&QQmlApplicationEngine::warnings,[&](const QList<QQmlError>& errors){for(auto e:errors)warnings<<e.toString();});
    engine.load(QUrl::fromLocalFile(root+"/src/ui-v4/Main.qml"));
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().value(0)); if(!window)return 2;
    auto check=[&](QString name,bool ok){checks.append(QJsonObject{{"name",name},{"pass",ok}});pass&=ok;return ok;};
    auto wait=[&](std::function<bool()> condition,int milliseconds=30000){QElapsedTimer t;t.start();while(!condition()&&t.elapsed()<milliseconds)QTest::qWait(25);return condition();};
    std::function<QQuickItem*(QQuickItem*, const QString&)> visualItem = [&](QQuickItem* parent, const QString& name) -> QQuickItem* { if(parent->objectName()==name)return parent;for(auto child:parent->childItems())if(auto found=visualItem(child,name))return found;return nullptr; };
    auto item=[&](const char*name){return visualItem(window->contentItem(),QString::fromLatin1(name));};
    auto click=[&](const char*name){auto i=item(name);if(!i||!i->isVisible()||!i->isEnabled())return false;QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,i->mapToScene(QPointF(i->width()/2,i->height()/2)).toPoint());return true;};
    auto capture=[&](QString name){QTest::qWait(300);return window->grabWindow().save(output+"/"+name+".png");};
    auto choose=[&](int index){return QMetaObject::invokeMethod(window,"chooseResult",Q_ARG(QVariant,index));};
    auto imageReady=[&]{auto i=item("previewImage");return i&&i->isVisible()&&i->property("status").toInt()==1&&i->property("paintedWidth").toDouble()>0;};
    auto clear=[&]{QMetaObject::invokeMethod(window,"clearSelection");};
    QTcpServer canary;int fetches=0;canary.listen(QHostAddress::LocalHost,0);
    QObject::connect(&canary,&QTcpServer::newConnection,[&]{++fetches;auto s=canary.nextPendingConnection();s->close();s->deleteLater();});
    QFile hostile(corpus+"/Literal markup.txt");if(!hostile.open(QIODevice::WriteOnly))return 2;
    hostile.write(QString("LITERAL-82371. This is literal source text: <b>not styled</b><img src='http://127.0.0.1:%1/source'>.\n").arg(canary.serverPort()).toUtf8());hostile.close();
    check("model-and-service-ready",wait([&]{return backend.modelReady();},45000));
    backend.addRoot(QUrl::fromLocalFile(corpus));
    check("selected-synthetic-root-indexed",wait([&]{return backend.indexed()>=5&&!backend.busy();}));
    backend.setQuery("how to clean the cooling system");
    check("real-results",wait([&]{return !backend.searching()&&backend.results().size()>=3;}));
    int pdf=-1;for(int i=0;i<backend.results().size();++i){auto r=backend.results()[i].toMap();if(r["kind"]=="pdf"&&r["page"].toInt()==2){pdf=i;break;}}
    check("matching-pdf-page-in-results",pdf>=0);
    if(pdf>=0)choose(pdf);
    check("inline-pdf-preview-loaded",wait(imageReady));
    QString previewFile=backend.previewUrl().toLocalFile();
    check("pdf-thumbnail-loaded",wait([&]{return pdf>=0&&!backend.thumbnailUrl(backend.results()[pdf].toMap()).isEmpty();}));
    check("all-media-thumbnails-complete",wait([&]{for(auto row:backend.results()){auto r=row.toMap();if((r["kind"]=="image"||r["kind"]=="pdf")&&backend.thumbnailUrl(r).isEmpty())return false;}return true;}));
    check("grid-pdf-render-capture",capture("grid-pdf"));
    check("list-toggle",click("viewModeToggle")&&wait([&]{return item("resultList")!=nullptr;}));
    check("list-render-capture",capture("list"));
    check("grid-toggle",click("viewModeToggle")&&wait([&]{return item("resultGrid")!=nullptr;}));
    check("images-filter-click",click("filterImages"));
    check("images-filter-result",wait([&]{if(backend.searching()||backend.results().isEmpty())return false;for(auto r:backend.results())if(r.toMap()["kind"]!="image")return false;return true;}));
    auto imageResult=backend.results().value(0).toMap();
    QTest::keyClick(window,Qt::Key_L,Qt::ControlModifier);QTest::qWait(100);
    check("search-shortcut-focus",window->activeFocusItem()&&window->activeFocusItem()->objectName()=="searchField");
    QTest::keyClick(window,Qt::Key_Down);QTest::qWait(100);
    check("keyboard-result-selection",window->property("selectedIndex").toInt()==0);
    QTest::keyClick(window,Qt::Key_Space);
    check("image-thumbnail-and-preview-loaded",imageResult["kind"]=="image"&&wait([&]{return imageReady()&&!backend.thumbnailUrl(imageResult).isEmpty();}));
    check("old-pdf-preview-removed",!QFile::exists(previewFile));
    check("images-render-capture",capture("images"));
    check("preview-hide-cleans-url",click("previewToggle")&&backend.previewUrl().isEmpty());
    check("preview-show-renders-again",click("previewToggle")&&wait(imageReady));
    window->resize(720,520);QTest::qWait(300);choose(0);
    check("narrow-preview-visible",wait([&]{auto panel=item("previewPanel");if(!panel)return false;auto origin=panel->mapToScene(QPointF());return panel->isVisible()&&panel->width()>600&&panel->width()<=window->width()+1&&origin.x()>=-1&&origin.x()+panel->width()<=window->width()+1&&imageReady();}));
    check("narrow-render-capture",capture("narrow"));
    window->resize(1180,780);QTest::qWait(300);
    check("settings-button",click("settingsButton"));
    auto settings=window->findChild<QObject*>("settingsDialog");
    check("settings-opens",wait([&]{return settings&&settings->property("visible").toBool();}));
    check("settings-render-capture",capture("settings"));
    if(settings)QMetaObject::invokeMethod(settings,"close");QTest::qWait(200);
    check("all-filter-click",click("filterAll"));backend.setQuery("LITERAL-82371");
    check("literal-source-result",wait([&]{return !backend.results().isEmpty()&&backend.results()[0].toMap()["text"].toString().contains("<img");}));choose(0);
    auto passage=window->findChild<QObject*>("passageText");
    check("literal-source-rendering",passage&&passage->property("textFormat").toInt()==0&&passage->property("text").toString().contains("<b>"));
    QTest::qWait(600);check("source-markup-no-network",fetches==0);
    check("document-render-capture",capture("document"));
    backend.setQuery("a changed search");check("stale-selection-and-preview-cleared",backend.results().isEmpty()&&backend.previewUrl().isEmpty()&&window->property("selectedIndex").toInt()==-1);
    QFile broken(corpus+"/Broken.pdf");if(!broken.open(QIODevice::WriteOnly))return 2;broken.write("This synthetic file is deliberately not a valid PDF.");broken.close();
    check("actual-extraction-failure-recorded",wait([&]{return backend.errors()>0&&!backend.busy()&&!backend.failures().isEmpty();}));
    auto issues=item("indexingIssues");
    check("main-indexing-issue-visible",issues&&issues->isVisible()&&issues->property("text").toString().contains(QString::number(backend.errors())));
    check("main-indexing-details-click",click("indexingDetailsButton"));
    auto failuresDialog=window->findChild<QObject*>("failuresDialog");
    check("indexing-details-open",wait([&]{return failuresDialog&&failuresDialog->property("visible").toBool();}));
    check("indexing-error-render-capture",capture("indexing-error"));
    if(failuresDialog)QMetaObject::invokeMethod(failuresDialog,"close");

    clear();backend.closePreview();backend.shutdownOwnedService();QTest::qWait(600);
    check("qml-runtime-clean",warnings.isEmpty());
    QJsonArray warningRows;for(auto s:warnings)warningRows.append(s);
    const auto report=QJsonDocument(QJsonObject{{"pass",pass},{"checks",checks},{"qml_warnings",warningRows},{"network_fetches",fetches},{"window_width",window->width()},{"window_height",window->height()}}).toJson();
    QFile receipt(output+"/results.json");if(!receipt.open(QIODevice::WriteOnly))return 2;receipt.write(report);
    fwrite(report.constData(),1,size_t(report.size()),stdout);return pass?0:1;
}
