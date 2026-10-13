#include "schema.h"
#include <stdexcept>
#include <cmath>
static void initializeFilmResources(){static const bool initialized=[](){Q_INIT_RESOURCE(film_schemas);return true;}();Q_UNUSED(initialized);}
namespace {
void bad(QString path,QString message){throw std::invalid_argument((path+": "+message).toStdString());}
int length(QString s){int n=0;for(qsizetype i=0;i<s.size();i++,n++){if(s[i].isHighSurrogate()){if(i+1>=s.size()||!s[i+1].isLowSurrogate())bad("text","unpaired surrogate");i++;}else if(s[i].isLowSurrogate())bad("text","unpaired surrogate");}return n;}
void check(QJsonValue v,QJsonObject s,const QJsonObject &root,QString path){
 if(s.contains("$ref")){QString ref=s["$ref"].toString();if(!ref.startsWith("#/$defs/"))bad(path,"unsupported schema reference");check(v,root["$defs"].toObject()[ref.mid(8)].toObject(),root,path);return;}
 if(s.contains("anyOf")){bool good=false;for(auto branch:s["anyOf"].toArray())try{check(v,branch.toObject(),root,path);good=true;break;}catch(const std::invalid_argument&){}if(!good)bad(path,"no schema alternative matched");}
 bool isInteger=v.isDouble()&&std::isfinite(v.toDouble())&&std::floor(v.toDouble())==v.toDouble();
 auto matches=[&](QString t){return (t=="object"&&v.isObject())||(t=="array"&&v.isArray())||(t=="string"&&v.isString())||(t=="integer"&&isInteger)||(t=="number"&&v.isDouble()&&std::isfinite(v.toDouble()))||(t=="null"&&v.isNull())||(t=="boolean"&&v.isBool());};
 if(s.contains("type")){auto type=s["type"];bool matched=false;if(type.isString())matched=matches(type.toString());else if(type.isArray()){for(auto t:type.toArray()){if(!t.isString())bad(path,"invalid embedded union type");matched|=matches(t.toString());}}else bad(path,"invalid embedded type");if(!matched)bad(path,"wrong type");}
 if(s.contains("const")&&v!=s["const"])bad(path,"wrong schema constant");
 if(s.contains("enum")&&!s["enum"].toArray().contains(v))bad(path,"invalid enum");
 if(v.isObject()){
  auto o=v.toObject(),props=s["properties"].toObject();for(auto key:s["required"].toArray())if(!o.contains(key.toString()))bad(path,"missing "+key.toString());
  for(auto i=o.begin();i!=o.end();++i){if(!props.contains(i.key())){if(s["additionalProperties"]==false)bad(path,"undeclared "+i.key());}else check(i.value(),props[i.key()].toObject(),root,path+"."+i.key());}
 }
 if(v.isString()){QString text=v.toString();int n=length(text);if(text.contains(QChar(char16_t(0))))bad(path,"NUL rejected");if(s.contains("minLength")&&n<s["minLength"].toInt())bad(path,"string too short");if(s.contains("maxLength")&&n>s["maxLength"].toInt())bad(path,"string too long");if(s.contains("pattern")&&!QRegularExpression(s["pattern"].toString()).match(text).hasMatch())bad(path,"invalid string pattern");if(s["format"]=="date-time"&&(!QRegularExpression("(Z|[+-][0-9]{2}:[0-9]{2})$").match(text).hasMatch()||!QDateTime::fromString(text,Qt::ISODateWithMs).isValid()))bad(path,"invalid date-time");}
 if(v.isDouble()){double n=v.toDouble();if(s.contains("minimum")&&n<s["minimum"].toDouble())bad(path,"number below minimum");if(s.contains("maximum")&&n>s["maximum"].toDouble())bad(path,"number above maximum");}
 if(v.isArray()){auto a=v.toArray();if(s.contains("minItems")&&a.size()<s["minItems"].toInt())bad(path,"too few items");if(s.contains("maxItems")&&a.size()>s["maxItems"].toInt())bad(path,"too many items");QSet<QByteArray> seen;for(int i=0;i<a.size();i++){if(s["uniqueItems"].toBool()){QByteArray bytes=QJsonDocument(QJsonArray{a[i]}).toJson(QJsonDocument::Compact);if(seen.contains(bytes))bad(path,"duplicate item");seen.insert(bytes);}if(s.contains("items"))check(a[i],s["items"].toObject(),root,path+"["+QString::number(i)+"]");}}
 if(s.contains("allOf"))for(auto b:s["allOf"].toArray())check(v,b.toObject(),root,path);
 if(s.contains("contains")&&v.isArray()){int n=0;for(auto item:v.toArray())try{check(item,s["contains"].toObject(),root,path);n++;}catch(const std::invalid_argument&){}if(n<s.value("minContains").toInt(1))bad(path,"array lacks required item");}
 if(s.contains("if")){bool matched=true;try{check(v,s["if"].toObject(),root,path);}catch(const std::invalid_argument&){matched=false;}if(matched&&s.contains("then"))check(v,s["then"].toObject(),root,path);if(!matched&&s.contains("else"))check(v,s["else"].toObject(),root,path);}
}
}
void FilmSchema::validate(const QJsonObject &value,const QString &name){
 initializeFilmResources();QFile f(":/film/"+name+".schema.json");if(!f.open(QIODevice::ReadOnly))bad(name,"embedded schema unavailable");QJsonParseError e;auto d=QJsonDocument::fromJson(f.readAll(),&e);if(e.error!=QJsonParseError::NoError||!d.isObject())bad(name,"embedded schema invalid");check(value,d.object(),d.object(),name);
}
