// Additive film observer. Reads real QObject/QQuick state; actions use synthesized Qt input.
// No expected text, result, evidence, geometry or visibility is assigned.
#include <QApplication>
#include <QTest>
#include <QJSValue>
#include <QWheelEvent>
#include <QWidget>
#include <QDesktopServices>
#include <QDateTime>
#include <QtGui/qguiapplication_platform.h>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QSGRendererInterface>
#include <dlfcn.h>
#include <unistd.h>
#include <set>
#include <atomic>
#include <memory>
struct FrameWatch {std::atomic<quint64> end{0},before{0},sync{0},animate{0};};
static QHash<QQuickWindow*,std::shared_ptr<FrameWatch>> watches;
static QHash<QString,QPointer<QObject>> nodes;
static QElapsedTimer clockTimer;
static QString id(QObject *o){return QString::number(quintptr(o),16);}
static QJsonObject rect(QRectF r){return {{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()}};}
static void watch(QQuickWindow*w){if(watches.contains(w))return;auto f=std::make_shared<FrameWatch>();watches[w]=f;
 QObject::connect(w,&QQuickWindow::afterFrameEnd,w,[f]{++f->end;},Qt::DirectConnection);
 QObject::connect(w,&QQuickWindow::beforeRendering,w,[f]{++f->before;},Qt::DirectConnection);
 QObject::connect(w,&QQuickWindow::afterSynchronizing,w,[f]{++f->sync;},Qt::DirectConnection);
 QObject::connect(w,&QQuickWindow::afterAnimating,w,[f]{++f->animate;},Qt::DirectConnection);
}
static QJsonValue jsonValue(QVariant v){if(v.metaType()==QMetaType::fromType<QJSValue>())v=v.value<QJSValue>().toVariant();return QJsonValue::fromVariant(v);}
static QJsonObject describe(QObject*o){nodes[id(o)]=o;QJsonObject r{{"node",id(o)},{"class",o->metaObject()->className()},{"name",o->objectName()},{"object_parent",id(o->parent())}};
 for(const char*p:{"enabled","selectedIndex","selectedResult","highlighted","activeFocus","modelData","evidence","results","client","text","textFormat","displayText","title","query","queryString","singleRunner","querying","pendingFirstResults","count","currentIndex","contentHeight","contentWidth","contentX","contentY","contentItem","implicitWidth","implicitHeight","implicitContentWidth","implicitContentHeight","availableHeight","availableWidth","leftPadding","rightPadding","topPadding","bottomPadding","width","height","visible","clip","opacity","minimumHeight","minimumWidth","maximumHeight","maximumWidth","preferredHeight","preferredWidth","fillHeight","fillWidth","index","isCurrent"}){auto v=o->property(p);if(v.isValid()){if(auto ptr=qvariant_cast<QObject*>(v))r[p]=id(ptr);else r[p]=jsonValue(v);}}
 if(auto w=qobject_cast<QWindow*>(o)){r["window_exposed"]=w->isExposed();r["window_visible"]=w->isVisible();r["window_geometry"]=rect(w->geometry());}
 if(auto w=qobject_cast<QQuickWindow*>(o)){watch(w);auto f=watches[w];r["scene_graph_initialized"]=w->isSceneGraphInitialized();r["frames"] = QJsonObject{{"afterFrameEnd",qint64(f->end.load())},{"beforeRendering",qint64(f->before.load())},{"afterSynchronizing",qint64(f->sync.load())},{"afterAnimating",qint64(f->animate.load())}};r["graphics_api"]=int(w->rendererInterface()->graphicsApi());}
 if(auto item=qobject_cast<QQuickItem*>(o)){
  auto w=item->window();QRectF bounds=item->mapRectToScene(QRectF(0,0,item->width(),item->height()));QRectF intersection=w?bounds.intersected(QRectF(0,0,w->width(),w->height())):QRectF();QJsonArray chain;bool visible=item->isVisible();double opacity=item->opacity();
  for(auto p=item->parentItem();p;p=p->parentItem()){visible&=p->isVisible();opacity*=p->opacity();if(p->clip()){auto clip=p->mapRectToScene(QRectF(0,0,p->width(),p->height()));chain.append(QJsonObject{{"node",id(p)},{"class",p->metaObject()->className()},{"clip_rect",rect(clip)}});intersection=intersection.intersected(clip);}}
  r["visual_parent"]=id(item->parentItem());r["scene_bounds"]=rect(bounds);r["window_clipped_bounds"]=rect(intersection);r["clip_ancestors"]=chain;r["effective_visible"]=visible;r["effective_opacity"]=opacity;r["window"]=id(w);r["window_exposed"]=w&&w->isExposed();r["window_visible"]=w&&w->isVisible();r["geometry_intersects_viewport"]=!intersection.isEmpty()&&visible&&opacity>0&&w&&w->isVisible();
  if(auto context=qmlContext(item)){auto v=context->contextProperty("backend");auto b=v.value<QObject*>();if(b){QJsonObject backend;for(const char*p:{"query","connected","modelReady","results","message"})backend[p]=jsonValue(b->property(p));r["actual_backend_state"]=backend;}auto film=context->contextProperty("filmBackend").value<QObject*>();if(film){QJsonObject state;for(const char*p:{"query","mode","includeUnverifiedPrior","available","searching","collectionId","results","evidence","message"})state[p]=jsonValue(film->property(p));r["actual_film_state"]=state;}}
 }
 return r;}
static void walk(QObject*o,QJsonArray&a,std::set<QObject*>&seen){if(!o||seen.count(o))return;seen.insert(o);a.append(describe(o));for(auto c:o->children())walk(c,a,seen);if(auto i=qobject_cast<QQuickItem*>(o))for(auto c:i->childItems())walk(c,a,seen);}
static QJsonObject envFields(){QJsonObject r;for(const char*k:{"WAYLAND_DISPLAY","DISPLAY","DBUS_SESSION_BUS_ADDRESS","XDG_RUNTIME_DIR","HOME","XDG_CONFIG_HOME","XDG_CACHE_HOME","XDG_DATA_HOME","XDG_SESSION_TYPE","XDG_CURRENT_DESKTOP"})r[k]=qEnvironmentVariableIsSet(k)?QJsonValue(qEnvironmentVariable(k)):QJsonValue();return r;}
static QJsonObject snapshot(){QJsonArray a;std::set<QObject*>seen;for(auto w:QGuiApplication::allWindows())walk(w,a,seen);for(auto w:QApplication::topLevelWidgets())walk(w,a,seen);return {{"environment",envFields()},{"platform_name",QGuiApplication::platformName()},{"pid",int(getpid())},{"executable",QCoreApplication::applicationFilePath()},{"elapsed_ns",clockTimer.nsecsElapsed()},{"nodes",a}};}
static void save(QString out,QString name,QJsonObject o){QSaveFile f(out+"/"+name+".json");if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(o).toJson());f.commit();}}
static QJsonArray readback(QString out,QString name){QJsonArray images;for(auto w:QGuiApplication::allWindows())if(auto q=qobject_cast<QQuickWindow*>(w)){QImage image=q->grabWindow();QString file=name+"-"+id(q)+".png";bool ok=image.save(out+"/"+file);images.append(QJsonObject{{"window",id(q)},{"file",file},{"saved",ok},{"width",image.width()},{"height",image.height()}});}return images;}
extern "C" int _ZN16QCoreApplication4execEv(){using Exec=int(*)();if(QFileInfo(QCoreApplication::applicationFilePath()).fileName()!=QStringLiteral("kde-index"))return reinterpret_cast<Exec>(dlsym(RTLD_NEXT,"_ZN16QCoreApplication4execEv"))();clockTimer.start();auto app=QCoreApplication::instance();QString out=qEnvironmentVariable("DESKTOP_LAYOUT_OUT");auto timer=new QTimer(app);timer->setInterval(30);
 QObject::connect(timer,&QTimer::timeout,app,[out]{QFile file(out+"/command.json");if(!file.open(QIODevice::ReadOnly))return;auto cmd=QJsonDocument::fromJson(file.readAll()).object();file.close();file.remove();QString name=cmd["id"].toString(),op=cmd["op"].toString();
  QJsonObject result{{"command",cmd},{"snapshot",snapshot()}};
  if(op=="click"||op=="input"||op=="wheel"){
   auto item=qobject_cast<QQuickItem*>(nodes.value(cmd["node"].toString()));
   auto state=item?describe(item):QJsonObject{};result["target_before_input"]=state;
   bool allowed=item&&item->isEnabled()&&state["geometry_intersects_viewport"].toBool()&&item->window()&&item->window()->isExposed();
   result["input_dispatched"]=allowed;
   if(allowed){auto b=state["window_clipped_bounds"].toObject();QPoint at(qRound(b["x"].toDouble()+b["width"].toDouble()/2),qRound(b["y"].toDouble()+b["height"].toDouble()/2));auto w=item->window();
    if(op=="wheel"){QWheelEvent event(at,w->mapToGlobal(at),QPoint(),QPoint(0,cmd["delta"].toInt(-480)),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(w,&event);}
    else {QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,at);if(op=="input"){QTest::keyClick(w,Qt::Key_A,Qt::ControlModifier);for(auto c:cmd["text"].toString().toLatin1())QTest::keyClick(w,c);}}
   }
   result["after_input"]=snapshot();
  }
  if(op=="capture"){result["readbacks"]=readback(out,name);result["post_readback"]=snapshot();}
  if(op=="quit")QCoreApplication::quit();save(out,name,result);
 });timer->start();using Exec=int(*)();return reinterpret_cast<Exec>(dlsym(RTLD_NEXT,"_ZN16QCoreApplication4execEv"))();}
extern "C" bool _ZN16QDesktopServices7openUrlERK4QUrl(const QUrl&url){using Open=bool(*)(const QUrl&);auto real=reinterpret_cast<Open>(dlsym(RTLD_NEXT,"_ZN16QDesktopServices7openUrlERK4QUrl"));QJsonObject e{{"pid",int(getpid())},{"actual_url",url.toString(QUrl::FullyEncoded)},{"local_file",url.toLocalFile()},{"before_call_elapsed_ns",clockTimer.nsecsElapsed()},{"focus_window",QGuiApplication::focusWindow()?QGuiApplication::focusWindow()->title():QString()}};
#if QT_CONFIG(wayland)
 if(auto w=qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())e["last_input_serial"]=qint64(w->lastInputSerial());
#endif
 e["return_accepted"]=real(url);e["after_call_elapsed_ns"]=clockTimer.nsecsElapsed();QFile f(qEnvironmentVariable("DESKTOP_LAYOUT_OUT")+"/open-events.jsonl");if(f.open(QIODevice::WriteOnly|QIODevice::Append)){f.write(QJsonDocument(e).toJson(QJsonDocument::Compact));f.write("\n");}return e["return_accepted"].toBool();}
