// Separate pixel inspection: consumes raw geometry, screenshot and actual Poppler raster.
#include <QGuiApplication>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <cstdio>
int main(int argc,char **argv) {
 QGuiApplication app(argc,argv);if(argc!=4)return 2;
 QFile file(argv[1]);if(!file.open(QIODevice::ReadOnly))return 2;
 auto report=QJsonDocument::fromJson(file.readAll()).object();QJsonObject geometry;
 for(auto item:report["render_observations"].toArray())if(item.toObject()["event"]=="capture-after-ready-scene-frame")geometry=item.toObject();
 QImage reference(argv[2]),capture(argv[3]);if(geometry.isEmpty()||reference.isNull()||capture.isNull())return 2;
 const double sx=double(capture.width())/geometry["window_width"].toDouble(),sy=double(capture.height())/geometry["window_height"].toDouble();
 QRect region(qRound(geometry["painted_scene_x"].toDouble()*sx),qRound(geometry["painted_scene_y"].toDouble()*sy),qRound(geometry["painted_width"].toDouble()*sx),qRound(geometry["painted_height"].toDouble()*sy));
 QImage actual=capture.copy(region).convertToFormat(QImage::Format_RGB32),expected=reference.scaled(region.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
 int minX=expected.width(),minY=expected.height(),maxX=-1,maxY=-1,ink=0;double error=0;
 for(int y=0;y<expected.height();++y)for(int x=0;x<expected.width();++x){int e=qGray(expected.pixel(x,y)),a=qGray(actual.pixel(x,y));error+=std::abs(e-a);if(e<210){++ink;minX=std::min(minX,x);maxX=std::max(maxX,x);minY=std::min(minY,y);maxY=std::max(maxY,y);}}
 QRect witness=QRect(QPoint(minX,minY),QPoint(maxX,maxY)).adjusted(-3,-3,3,3).intersected(expected.rect());double sumE=0,sumA=0,sumEE=0,sumAA=0,sumEA=0;int n=0;
 for(int y=witness.top();y<=witness.bottom();++y)for(int x=witness.left();x<=witness.right();++x){double e=qGray(expected.pixel(x,y)),a=qGray(actual.pixel(x,y));sumE+=e;sumA+=a;sumEE+=e*e;sumAA+=a*a;sumEA+=e*a;++n;}
 double ve=n?sumEE-sumE*sumE/n:0,va=n?sumAA-sumA*sumA/n:0,corr=ve>0&&va>0?(sumEA-sumE*sumA/n)/std::sqrt(ve*va):0,mae=error/(expected.width()*expected.height());
 bool pass=region.width()>0&&region.height()>0&&capture.rect().contains(region)&&ink>=20&&mae<=10&&corr>=.65;
 QJsonObject result{{"capture",argv[3]},{"reference_raster",argv[2]},{"pass",pass},{"painted_rect",QJsonArray{region.x(),region.y(),region.width(),region.height()}},{"ink_reference_pixels",ink},{"mean_absolute_gray_error",mae},{"ink_witness_correlation",corr},{"thresholds",QJsonObject{{"maximum_gray_mae",10},{"minimum_ink_pixels",20},{"minimum_ink_correlation",.65}}}};
 auto out=QJsonDocument(result).toJson();fwrite(out.constData(),1,out.size(),stdout);return pass?0:1;
}
