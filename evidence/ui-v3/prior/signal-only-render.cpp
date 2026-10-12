// Controlled async-render phase falsifier plus real Poppler/header accessibility.
#include "backend.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QAccessible>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QThread>
#include <QFileInfo>
#include <QJsonDocument>
#include <QImage>
#include <atomic>
#include <functional>
#include <cstdio>
class DelayedResponse : public QQuickImageResponse {
 QImage m_image;
 public: explicit DelayedResponse(QImage image):m_image(std::move(image)){QTimer::singleShot(500,this,[this]{emit finished();});}
 QQuickTextureFactory *textureFactory() const override {return QQuickTextureFactory::textureFactoryForImage(m_image);}
};
class DelayedProvider : public QQuickAsyncImageProvider {
 QImage m_image;
 public: explicit DelayedProvider(QImage image):m_image(std::move(image)){}
 QQuickImageResponse *requestImageResponse(const QString &,const QSize &) override {return new DelayedResponse(m_image);}
};
bool wait(std::function<bool()> p,int bound=10000){QElapsedTimer t;t.start();while(t.elapsed()<bound){QCoreApplication::processEvents();if(p())return true;QThread::msleep(5);}return p();}
int main(int argc,char**argv){
 QApplication app(argc,argv);if(argc!=4)return 2;QString qml=QFileInfo(argv[1]).absoluteFilePath(),pdf=QFileInfo(argv[2]).absoluteFilePath(),output=QFileInfo(argv[3]).absoluteFilePath();
 Backend backend({output+".sock","/nonexistent", "/nonexistent",output+"-no-models",output+"-unused-data",""});
 QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("backend",&backend);engine.load(QUrl::fromLocalFile(qml));auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));if(!window)return 2;
 auto dialog=window->findChild<QObject*>("resultPreview");auto image=window->findChild<QQuickItem*>("previewImage");auto header=window->findChild<QQuickItem*>("previewTitle");if(!dialog||!image||!header)return 2;
 QJsonArray observations,checks;QElapsedTimer elapsed;elapsed.start();std::atomic<quint64> frames{0};auto frameConnection=QObject::connect(window,&QQuickWindow::afterFrameEnd,&app,[&]{frames.fetch_add(1);},Qt::DirectConnection);
 auto record=[&](QString event){double pw=image->property("paintedWidth").toDouble(),ph=image->property("paintedHeight").toDouble();auto scene=image->mapToScene(QPointF((image->width()-pw)/2,(image->height()-ph)/2));QJsonObject o{{"event",event},{"milliseconds",elapsed.elapsed()},{"scene_frames",qint64(frames.load())},{"image_status",image->property("status").toInt()},{"image_source",image->property("source").toUrl().toString()},{"visible",image->isVisible()},{"width",image->width()},{"height",image->height()},{"painted_width",pw},{"painted_height",ph},{"painted_scene_x",scene.x()},{"painted_scene_y",scene.y()},{"window_width",window->width()},{"window_height",window->height()}};observations.append(o);return o;};
 bool pass=true;auto check=[&](QString name,bool value,QJsonObject details={}){details["name"]=name;details["pass"]=value;checks.append(details);pass&=value;};
 QTcpServer http;http.listen(QHostAddress::LocalHost,0);int fetches=0;QJsonArray requests;
 QObject::connect(&http,&QTcpServer::newConnection,&app,[&]{auto socket=http.nextPendingConnection();QObject::connect(socket,&QTcpSocket::readyRead,&app,[&,socket]{++fetches;requests.append(QString::fromLatin1(socket->readAll()));socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");socket->disconnectFromHost();});});
 QString title=QString("<b>PDF & ID-7481</b><img src='http://127.0.0.1:%1/title'>").arg(http.serverPort());dialog->setProperty("selectedResult",QVariantMap{{"kind","pdf"},{"page",2},{"path",pdf},{"title",title},{"text","PDF-10299"}});QMetaObject::invokeMethod(dialog,"open");check("real-poppler-raster-produced",wait([&]{return !backend.previewUrl().isEmpty();}));QString raster=backend.previewUrl().toLocalFile();QImage original(raster);check("actual-raster-positive",!original.isNull());QFile::copy(raster,output+"-raster.png");
 // The source pixels already exist, while the actual QML Image producer is loading.
 engine.addImageProvider("phase",new DelayedProvider(original));image->setProperty("source",QUrl("image://phase/page"));record("controlled-loading-with-existing-valid-raster");check("loading-state-is-not-render-ready",image->property("status").toInt()==2);window->grabWindow().save(output+"-loading.png");
 check("actual-image-ready-positive-geometry",wait([&]{return image->property("status").toInt()==1&&image->isVisible()&&image->property("paintedWidth").toDouble()>0&&image->property("paintedHeight").toDouble()>0;}));auto armed=frames.load();record("image-ready-frame-armed");window->update();window->grabWindow();check("subsequent-actual-scene-frame",wait([&]{return frames.load()>armed;}));record("capture-after-ready-scene-frame");check("page-pixels-captured",window->grabWindow().save(output+"-page.png"));
 QAccessible::setActive(true);auto accessible=QAccessible::queryAccessibleInterface(header);check("header-accessibility-applies-to-item",accessible&&accessible->role()==QAccessible::StaticText&&accessible->text(QAccessible::Name)==title,{{"expected_name",title},{"actual_name",accessible?accessible->text(QAccessible::Name):QString()},{"actual_role",accessible?int(accessible->role()):-1},{"text_format",header->property("textFormat").toInt()}});
 wait([]{return false;},300);check("arbitrary-accessible-header-no-http-fetch",fetches==0,{{"requests",requests}});
 QTimer closed;closed.setSingleShot(true);closed.setInterval(10000);QObject::connect(dialog,SIGNAL(closed()),&closed,SLOT(stop()));closed.start();QMetaObject::invokeMethod(dialog,"close");check("actual-closed-cleanup",wait([&]{return !closed.isActive()&&!dialog->property("visible").toBool()&&backend.previewUrl().isEmpty()&&!QFileInfo::exists(raster);}),{{"raster",raster}});
 QObject::disconnect(frameConnection);http.close();auto json=QJsonDocument(QJsonObject{{"pass",pass},{"checks",checks},{"render_observations",observations},{"milliseconds",elapsed.elapsed()}}).toJson();fwrite(json.constData(),1,json.size(),stdout);return pass?0:1;
}
