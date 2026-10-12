// Independent native consumer challenge. Frozen actual RunnerManager fields,
// installed Plasma ToolTip, no classifier used to decide network observations.
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <iostream>
int main(int argc,char **argv) {
 QGuiApplication app(argc,argv); if(argc!=2)return 2;
 QFile file(argv[1]);if(!file.open(QIODevice::ReadOnly))return 3;
 QJsonObject fixture;
 for(auto line:file.readAll().split('\n')) {
  auto r=QJsonDocument::fromJson(line).object();
  if(r["case"]=="passage-image")fixture=r;
 }
 if(fixture.isEmpty())return 4;
 QElapsedTimer clock;clock.start();QTcpServer server;QJsonArray gets,phases;
 if(!server.listen(QHostAddress::LocalHost,43573))return 5;
 QObject::connect(&server,&QTcpServer::newConnection,[&]{
  while(server.hasPendingConnections()) {
   auto *s=server.nextPendingConnection();
   QObject::connect(s,&QTcpSocket::readyRead,[&,s]{
    QByteArray bytes=s->readAll();
    if(!bytes.startsWith("GET "))return;
    gets.append(QJsonObject{{"request",QString::fromLatin1(bytes.split('\n')[0]).trimmed()},{"elapsed_ns",double(clock.nsecsElapsed())}});
    auto png=QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==");
    s->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nCache-Control: no-store\r\nContent-Length: "+QByteArray::number(png.size())+"\r\nConnection: close\r\n\r\n"+png);s->disconnectFromHost();
   });
   QObject::connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
  }
 });
 QString assigned=fixture["matches"].toArray()[0].toObject()["subtext"].toObject()["text"].toString();
 QString raw=fixture["response"].toObject()["results"].toArray()[0].toObject()["text"].toString();
 // Candidate first: avoids a prior successful fetch/cache masking the negative.
 for(auto pair:{qMakePair(QString("candidate-match"),assigned),qMakePair(QString("raw-source-control"),raw)}) {
  QQmlEngine engine;QQmlComponent component(&engine);
  component.setData("import QtQuick\nimport org.kde.plasma.components as PC\nItem { width: 300; height: 100; PC.ToolTip { objectName: \"actualToolTip\"; visible: false } }",QUrl("file:///tmp/consumer-review.qml"));
  auto *root=component.create();if(!root){std::cerr<<component.errorString().toStdString();return 6;}
  auto *tip=root->findChild<QObject*>("actualToolTip");if(!tip)return 7;
  auto start=clock.nsecsElapsed();tip->setProperty("text",pair.second);
  QEventLoop loop;QTimer::singleShot(650,&loop,&QEventLoop::quit);loop.exec();
  QJsonArray labels;
  for(auto *obj:root->findChildren<QObject*>()) {
   if(obj->property("text").isValid()&&obj->property("textFormat").isValid())
    labels.append(QJsonObject{{"class",obj->metaObject()->className()},{"text",obj->property("text").toString()},{"textFormat",obj->property("textFormat").toInt()}});
  }
  phases.append(QJsonObject{{"name",pair.first},{"input",pair.second},{"start_ns",double(start)},{"end_ns",double(clock.nsecsElapsed())},{"tip_visible",tip->property("visible").toBool()},{"labels",labels},{"gets_cumulative",gets.size()}});
  delete root;
 }
 server.close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
 QJsonObject out{{"qt_version",qVersion()},{"phases",phases},{"gets",gets},{"server_closed",!server.isListening()},{"socket_children_after_deferred_delete",server.findChildren<QTcpSocket*>().size()}};
 std::cout<<QJsonDocument(out).toJson(QJsonDocument::Indented).toStdString();
 return phases[0].toObject()["gets_cumulative"].toInt()==0&&gets.size()==1&&phases[1].toObject()["gets_cumulative"].toInt()==1 ? 0:8;
}
