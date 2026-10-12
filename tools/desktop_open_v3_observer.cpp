// Konsole-only native receiver readback. Never receives or writes the expected source body.
#include <QApplication>
#include <QWidget>
#include <QWindow>
#include <QAccessible>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <dlfcn.h>
#include <unistd.h>
#include <typeinfo>
static QString oid(QObject*o){return QString::number(quintptr(o),16);}
static QElapsedTimer elapsed;
static QJsonObject collect(QString dir,QString name,bool capture){QJsonArray terminals,windows;
 for(auto w:QApplication::topLevelWidgets()){
  QJsonObject v{{"class",w->metaObject()->className()},{"title",w->windowTitle()},{"visible",w->isVisible()},{"width",w->width()},{"height",w->height()},{"window_exposed",w->windowHandle()&&w->windowHandle()->isExposed()}};
  if(capture){QString file=name+"-window-"+oid(w)+".png";auto image=w->grab().toImage();v["capture_file"]=file;v["capture_saved"]=image.save(dir+"/"+file);v["capture_width"]=image.width();v["capture_height"]=image.height();}windows.append(v);
 }
 for(auto widget:QApplication::allWidgets())if(QString(widget->metaObject()->className())=="Konsole::TerminalDisplay"){
  auto iface=QAccessible::queryAccessibleInterface(widget);auto text=iface?iface->textInterface():nullptr;QJsonObject r{{"object",oid(widget)},{"class",widget->metaObject()->className()},{"visible",widget->isVisible()},{"width",widget->width()},{"height",widget->height()},{"screen_rect_x",widget->mapToGlobal(QPoint()).x()},{"screen_rect_y",widget->mapToGlobal(QPoint()).y()},{"accessible_valid",iface&&iface->isValid()},{"text_interface_present",text!=nullptr}};
  if(iface){r["accessible_cpp_type"]=typeid(*iface).name();r["accessible_role"]=int(iface->role());r["accessible_name"]=iface->text(QAccessible::Name);r["accessible_value"]=iface->text(QAccessible::Value);}
  if(text){int count=text->characterCount();r["character_count"]=count;r["cursor_position"]=text->cursorPosition();r["actual_native_text"]=text->text(0,count);}
  terminals.append(r);
 }
 return {{"pid",int(getpid())},{"executable",QCoreApplication::applicationFilePath()},{"argv",QJsonArray::fromStringList(QCoreApplication::arguments())},{"elapsed_ns",elapsed.nsecsElapsed()},{"terminals",terminals},{"windows",windows}};
}
extern "C" int _ZN16QCoreApplication4execEv(){using Exec=int(*)();auto real=reinterpret_cast<Exec>(dlsym(RTLD_NEXT,"_ZN16QCoreApplication4execEv"));
 if(!qobject_cast<QApplication*>(QCoreApplication::instance())||QFileInfo(QCoreApplication::applicationFilePath()).fileName()!=QStringLiteral("konsole"))return real();
 elapsed.start();QString root=qEnvironmentVariable("DESKTOP_OPEN_V3_OBSERVER_ROOT");QString dir=root+"/"+QString::number(getpid());QDir().mkpath(dir);QSaveFile start(dir+"/identity.json");if(start.open(QIODevice::WriteOnly)){start.write(QJsonDocument(collect(dir,"initial",false)).toJson());start.commit();}
 auto timer=new QTimer(QCoreApplication::instance());timer->setInterval(30);QObject::connect(timer,&QTimer::timeout,QCoreApplication::instance(),[dir]{QFile f(dir+"/command.json");if(!f.open(QIODevice::ReadOnly))return;auto cmd=QJsonDocument::fromJson(f.readAll()).object();f.close();f.remove();QString name=cmd["id"].toString();auto result=collect(dir,name,cmd["op"].toString()=="capture");result["command"]=cmd;QSaveFile target(dir+"/"+name+".json");if(target.open(QIODevice::WriteOnly)){target.write(QJsonDocument(result).toJson());target.commit();}});timer->start();return real();}
