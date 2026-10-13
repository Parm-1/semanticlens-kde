// Typed native Okular observer. All ownership/exposure/paint fields precede ANY grab.
// No document/page/text/zoom setters. Capturing paints cannot become natural paints.
#include <QApplication>
#include <QWidget>
#include <QWindow>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QElapsedTimer>
#include <okular/core/document.h>
#include <okular/core/page.h>
#include <dlfcn.h>
#include <unistd.h>
#include <set>
static QString oid(QObject*o){return QString::number(quintptr(o),16);}
static QElapsedTimer elapsed;
struct Counts{quint64 natural=0,capture=0;QJsonObject firstCapture,lastNatural;};
static QHash<QObject*,Counts> paints;
static int captureDepth=0;
static void documents(QObject*,QList<Okular::Document*>&,std::set<QObject*>&);
static QWidget* ownerWidget(Okular::Document*);
static Okular::Document* ownerDocument(QWidget*,const QList<Okular::Document*>&);
static Counts& track(QObject*o){if(!paints.contains(o)){paints[o]={};QObject::connect(o,&QObject::destroyed,QCoreApplication::instance(),[o]{paints.remove(o);});}return paints[o];}
class PaintObserver:public QObject {public:using QObject::QObject;bool eventFilter(QObject*o,QEvent*e)override{
 if(e->type()==QEvent::Paint){auto widget=qobject_cast<QWidget*>(o);auto parent=widget?qobject_cast<QAbstractScrollArea*>(widget->parentWidget()):nullptr;
  if(parent&&QString(parent->metaObject()->className())=="PageView"&&parent->viewport()==widget){
   auto &count=track(o);if(captureDepth)++count.capture;else {++count.natural;
    QList<Okular::Document*> docs;std::set<QObject*>seen;documents(parent->window(),docs,seen);auto doc=ownerDocument(parent,docs);auto window=parent->window();
    count.lastNatural={{"document",oid(doc)},{"owner_window",oid(window)},{"local_file",doc?doc->currentDocument().toLocalFile():QString()},{"current_page_index_zero_based",doc?int(doc->currentPage()):-1},{"page_count",doc?int(doc->pages()):0},{"window_exposed",window&&window->windowHandle()&&window->windowHandle()->isExposed()},{"canvas_visible",widget->isVisible()},{"elapsed_ns",elapsed.nsecsElapsed()}};
   }
  }}return false;}};
static QJsonObject envFields(){QJsonObject r;for(const char*k:{"WAYLAND_DISPLAY","DISPLAY","DBUS_SESSION_BUS_ADDRESS","XDG_RUNTIME_DIR","HOME","XDG_CONFIG_HOME","XDG_CACHE_HOME","XDG_DATA_HOME","XDG_SESSION_TYPE","XDG_CURRENT_DESKTOP"})r[k]=qEnvironmentVariableIsSet(k)?QJsonValue(qEnvironmentVariable(k)):QJsonValue();return r;}
static void documents(QObject*o,QList<Okular::Document*>&docs,std::set<QObject*>&seen){if(!o||seen.count(o))return;seen.insert(o);if(auto d=qobject_cast<Okular::Document*>(o))docs<<d;for(auto c:o->children())documents(c,docs,seen);}
static QWidget* ownerWidget(Okular::Document*d){for(auto p=d->parent();p;p=p->parent())if(auto w=qobject_cast<QWidget*>(p))return w;return nullptr;}
static Okular::Document* ownerDocument(QWidget*view,const QList<Okular::Document*>&docs){for(auto p=view;p;p=p->parentWidget()){Okular::Document*found=nullptr;for(auto doc:docs)if(ownerWidget(doc)==p){if(found)return nullptr;found=doc;}if(found)return found;}return nullptr;}
static QJsonObject collect(QString dir,QString name,bool capture){
 QList<Okular::Document*> docs;std::set<QObject*>seen;for(auto w:QApplication::topLevelWidgets())documents(w,docs,seen);
 QJsonArray docFields,windows,views,captures,windowCaptures;QList<QAbstractScrollArea*> nativeViews;
 for(auto w:QApplication::topLevelWidgets())windows.append(QJsonObject{{"object",oid(w)},{"class",w->metaObject()->className()},{"title",w->windowTitle()},{"visible",w->isVisible()},{"window_exposed",w->windowHandle()&&w->windowHandle()->isExposed()},{"width",w->width()},{"height",w->height()}});
 for(auto doc:docs){auto owner=ownerWidget(doc);auto window=owner?owner->window():nullptr;QJsonArray visible;for(auto r:doc->visiblePageRects())visible.append(QJsonObject{{"page_index_zero_based",r->pageNumber},{"left",r->rect.left},{"top",r->rect.top},{"right",r->rect.right},{"bottom",r->rect.bottom}});auto page=doc->isOpened()?doc->page(doc->currentPage()):nullptr;
  docFields.append(QJsonObject{{"object",oid(doc)},{"owner_widget",oid(owner)},{"owner_window",oid(window)},{"class",doc->metaObject()->className()},{"is_opened",doc->isOpened()},{"current_url",doc->currentDocument().toString(QUrl::FullyEncoded)},{"local_file",doc->currentDocument().toLocalFile()},{"page_count",int(doc->pages())},{"current_page_index_zero_based",int(doc->currentPage())},{"visible_page_rects",visible},{"selected_page_text_cache",page?page->text():QString()}});
 }
 for(auto w:QApplication::allWidgets())if(QString(w->metaObject()->className())=="PageView")if(auto view=qobject_cast<QAbstractScrollArea*>(w)){auto canvas=view->viewport();auto doc=ownerDocument(view,docs);auto owner=doc?ownerWidget(doc):nullptr;auto window=view->window();const auto count=paints.value(canvas);
  QJsonObject fields{{"object",oid(view)},{"document",oid(doc)},{"owner_widget",oid(owner)},{"owner_window",oid(window)},{"class",view->metaObject()->className()},{"visible",view->isVisible()},{"canvas_visible",canvas->isVisible()},{"window_exposed",window&&window->windowHandle()&&window->windowHandle()->isExposed()},{"canvas_width",canvas->width()},{"canvas_height",canvas->height()},{"natural_paint_events",qint64(count.natural)},{"capture_paint_events",qint64(count.capture)},{"last_natural_paint_phase",count.lastNatural},{"first_capture_pre_phase",count.firstCapture.isEmpty()?QJsonValue():QJsonValue(count.firstCapture)}};
  views.append(fields);nativeViews<<view;
 }
 // Freeze the complete pre-capture phase before the first whole-window OR canvas grab.
 const QJsonObject phase{{"documents",docFields},{"windows",windows},{"pageviews",views},{"elapsed_ns",elapsed.nsecsElapsed()},{"capture_depth",captureDepth}};
 if(capture){for(int i=0;i<nativeViews.size();++i){auto canvas=nativeViews[i]->viewport();auto &count=track(canvas);if(count.firstCapture.isEmpty())count.firstCapture=views[i].toObject();}
  ++captureDepth;
  for(auto w:QApplication::topLevelWidgets()){QString file=name+"-window-"+oid(w)+".png";auto image=w->grab().toImage();windowCaptures.append(QJsonObject{{"window",oid(w)},{"file",file},{"saved",image.save(dir+"/"+file)}});}
  for(int i=0;i<nativeViews.size();++i){auto view=nativeViews[i];auto canvas=view->viewport();auto pre=views[i].toObject();QString file=name+"-pageview-"+oid(view)+".png";auto image=canvas->grab().toImage();const auto after=paints.value(canvas);
   captures.append(QJsonObject{{"view",oid(view)},{"document",pre["document"]},{"owner_window",pre["owner_window"]},{"capture_file",file},{"capture_saved",image.save(dir+"/"+file)},{"capture_width",image.width()},{"capture_height",image.height()},{"pre_capture_natural_paint_events",pre["natural_paint_events"]},{"capture_paint_events_delta",qint64(after.capture)-pre["capture_paint_events"].toInteger()},{"natural_paint_events_after",qint64(after.natural)}});
  }--captureDepth;
 }
 return {{"environment",envFields()},{"platform_name",QGuiApplication::platformName()},{"pid",int(getpid())},{"executable",QCoreApplication::applicationFilePath()},{"argv",QJsonArray::fromStringList(QCoreApplication::arguments())},{"pre_capture_phase",phase},{"captures",captures},{"window_captures",windowCaptures}};
}
extern "C" int _ZN16QCoreApplication4execEv(){using Exec=int(*)();auto real=reinterpret_cast<Exec>(dlsym(RTLD_NEXT,"_ZN16QCoreApplication4execEv"));if(!qobject_cast<QApplication*>(QCoreApplication::instance())||QFileInfo(QCoreApplication::applicationFilePath()).fileName()!=QStringLiteral("okular"))return real();elapsed.start();auto filter=new PaintObserver(QCoreApplication::instance());QCoreApplication::instance()->installEventFilter(filter);QString dir=qEnvironmentVariable("FILM_PDF_OBSERVER_ROOT")+"/"+QString::number(getpid());QDir().mkpath(dir);QSaveFile start(dir+"/identity.json");if(start.open(QIODevice::WriteOnly)){start.write(QJsonDocument(collect(dir,"initial",false)).toJson());start.commit();}auto timer=new QTimer(QCoreApplication::instance());timer->setInterval(30);QObject::connect(timer,&QTimer::timeout,QCoreApplication::instance(),[dir]{QFile f(dir+"/command.json");if(!f.open(QIODevice::ReadOnly))return;auto cmd=QJsonDocument::fromJson(f.readAll()).object();f.close();f.remove();QString name=cmd["id"].toString();auto result=collect(dir,name,cmd["op"].toString()=="capture");result["command"]=cmd;QSaveFile target(dir+"/"+name+".json");if(target.open(QIODevice::WriteOnly)){target.write(QJsonDocument(result).toJson());target.commit();}});timer->start();return real();}
