// In-process diagnostic readback for stock Qt apps; never substitutes a frontend.
#include <QApplication>
#include <QWidget>
#include <QDesktopServices>
#include <QDateTime>
#include <QtGui/qguiapplication_platform.h>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaMethod>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QAccessible>
#include <KFileItem>
#include <dlfcn.h>
#include <unistd.h>
#include <set>
static QHash<QString,QObject*> nodes;
static QString identity(QObject *o){return QString::number(quintptr(o),16);}
static QJsonObject describe(QObject *o){
 nodes[identity(o)]=o;QJsonObject r{{"node",identity(o)},{"class",o->metaObject()->className()},{"name",o->objectName()},{"parent_node",identity(o->parent())}};
 for(const char *p:{"text","textFormat","displayText","title","source","query","singleRunner","currentIndex","count","isCurrent","truncated","visible","width","height","opacity","index","model","actions","activeAction"}){auto v=o->property(p);if(v.isValid())r[p]=QJsonValue::fromVariant(v);}
 if(auto item=qobject_cast<QQuickItem*>(o)){auto pt=item->mapToScene(QPointF(item->width()/2,item->height()/2));r["scene_x"]=pt.x();r["scene_y"]=pt.y();r["effective_visible"]=item->isVisible();r["window"]=identity(item->window());r["window_visible"]=item->window()&&item->window()->isVisible();}
 if(qEnvironmentVariable("DESKTOP_OBSERVER_ROLE")=="dolphin"&&QString(o->metaObject()->className())=="DolphinView"){
  using Url=QUrl(*)(const void*);using Selected=KFileItemList(*)(const void*);
  auto url=reinterpret_cast<Url>(dlsym(RTLD_DEFAULT,"_ZNK11DolphinView3urlEv"));auto selected=reinterpret_cast<Selected>(dlsym(RTLD_DEFAULT,"_ZNK11DolphinView13selectedItemsEv"));
  if(url)r["actual_view_url"]=url(o).toString();if(selected){QJsonArray a;for(auto item:selected(o))a.append(item.url().toString());r["actual_selected_urls"]=a;}
 }
 return r;
}
static void walk(QObject *o,QJsonArray &a,std::set<QObject*> &seen){if(!o||seen.count(o))return;seen.insert(o);a.append(describe(o));for(auto c:o->children())walk(c,a,seen);if(auto i=qobject_cast<QQuickItem*>(o))for(auto c:i->childItems())walk(c,a,seen);}
static QJsonObject snapshot(){QJsonArray a;std::set<QObject*> seen;for(auto w:QGuiApplication::allWindows())walk(w,a,seen);for(auto w:QApplication::topLevelWidgets())walk(w,a,seen);return {{"pid",int(getpid())},{"executable",QCoreApplication::applicationFilePath()},{"nodes",a}};}
extern "C" int _ZN16QCoreApplication4execEv(){
 auto app=QCoreApplication::instance();QString out=qEnvironmentVariable("DESKTOP_OBSERVER_OUT");if(!out.isEmpty()){
  auto timer=new QTimer(app);timer->setInterval(50);QObject::connect(timer,&QTimer::timeout,app,[out]{QFile f(out+"/command.json");if(!f.open(QIODevice::ReadOnly))return;auto cmd=QJsonDocument::fromJson(f.readAll()).object();f.close();f.remove();QJsonObject result{{"command",cmd}};auto operation=cmd["op"].toString();QObject *o=nodes.value(cmd["node"].toString());
   if(operation=="set"&&o)result["success"]=o->setProperty(cmd["property"].toString().toUtf8().constData(),cmd["value"].toVariant());
   if(operation=="invoke"&&o){auto method=cmd["method"].toString().toUtf8();bool ok;if(cmd.contains("argument")){QVariant arg=cmd["argument"].toVariant();ok=QMetaObject::invokeMethod(o,method.constData(),Qt::DirectConnection,Q_ARG(QVariant,arg));}else ok=QMetaObject::invokeMethod(o,method.constData(),Qt::DirectConnection);result["success"]=ok;}
   if(operation=="hover"&&o){auto i=qobject_cast<QQuickItem*>(o);if(i&&i->window()){auto w=i->window();auto pt=i->mapToScene(QPointF(i->width()/2,i->height()/2));QMouseEvent e(QEvent::MouseMove,pt,w->mapToGlobal(pt),Qt::NoButton,Qt::NoButton,Qt::NoModifier);result["success"]=QCoreApplication::sendEvent(w,&e);result["hover_scene_x"]=pt.x();result["hover_scene_y"]=pt.y();}}
   if(operation=="key"&&o){QKeyEvent press(QEvent::KeyPress,cmd["key"].toInt(),Qt::NoModifier);QKeyEvent release(QEvent::KeyRelease,cmd["key"].toInt(),Qt::NoModifier);result["success"]=QCoreApplication::sendEvent(o,&press);QCoreApplication::sendEvent(o,&release);}
   if(operation=="capture"){for(auto w:QGuiApplication::allWindows())if(auto q=qobject_cast<QQuickWindow*>(w))if(q->isVisible())result["capture_saved"]=q->grabWindow().save(out+"/"+cmd["id"].toString()+".png");}
   result["snapshot"]=snapshot();QSaveFile target(out+"/"+cmd["id"].toString()+".json");if(target.open(QIODevice::WriteOnly)){target.write(QJsonDocument(result).toJson());target.commit();}if(operation=="quit")QCoreApplication::quit();
  });timer->start();
 }
 using Exec=int(*)();return reinterpret_cast<Exec>(dlsym(RTLD_NEXT,"_ZN16QCoreApplication4execEv"))();
}

extern "C" bool _ZN16QDesktopServices7openUrlERK4QUrl(const QUrl &url){
 using Open=bool(*)(const QUrl&);auto real=reinterpret_cast<Open>(dlsym(RTLD_NEXT,"_ZN16QDesktopServices7openUrlERK4QUrl"));
 auto window=QGuiApplication::focusWindow();QJsonObject event{{"phase","before-QDesktopServices-openUrl"},{"url",url.toString(QUrl::FullyEncoded)},{"pid",int(getpid())},{"local_file",url.toLocalFile()},{"platform",QGuiApplication::platformName()},{"focus_window",window?window->title():QString()},{"utc_ms",QDateTime::currentMSecsSinceEpoch()}};
 #if QT_CONFIG(wayland)
 auto wayland=qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();if(wayland)event["last_wayland_input_serial"]=qint64(wayland->lastInputSerial());
 #endif
 bool accepted=real(url);event["return_accepted"]=accepted;QString out=qEnvironmentVariable("DESKTOP_OBSERVER_OUT");QFile file(out+"/openUrl-events.jsonl");if(file.open(QIODevice::WriteOnly|QIODevice::Append)){file.write(QJsonDocument(event).toJson(QJsonDocument::Compact));file.write("\n");}return accepted;
}
