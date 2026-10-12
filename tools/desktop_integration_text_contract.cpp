// Installed Qt AutoText classifier experiment, not a production repair.
#include <QCoreApplication>
#include <QTextDocument>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdio>
int main(int argc,char**argv){QCoreApplication app(argc,argv);QStringList samples={"<b>TITLE-77331.txt","DESKTOP-77331 <img src=\"http://127.0.0.1:1/passage\" />","<!DOCTYPE html><img src=x>","<?xml version='1.0'?><img src=x>","&lt;img src=x&gt;","ordinary A&B report"};QJsonArray a;for(auto raw:samples){auto display=raw;display.replace('<',QChar(0xff1c));display.replace('>',QChar(0xff1e));display.replace('&',QChar(0xff06));auto tooltip=display;tooltip.remove(QRegularExpression("&(?=\\S)"));a.append(QJsonObject{{"raw",raw},{"raw_qt_mightBeRichText",Qt::mightBeRichText(raw)},{"proposed_display",display},{"proposed_qt_mightBeRichText",Qt::mightBeRichText(display)},{"stock_tooltip_text",tooltip},{"stock_tooltip_qt_mightBeRichText",Qt::mightBeRichText(tooltip)}});}auto j=QJsonDocument(a).toJson();fwrite(j.data(),1,j.size(),stdout);}
