#include "PreviewController.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QImageWriter>
#include <QElapsedTimer>
#include <QThread>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>
template<class F> void until(F done){QElapsedTimer t;t.start();while(!done()&&t.elapsed()<120000){QCoreApplication::processEvents();QThread::msleep(1);}if(!done())throw std::runtime_error("deadline");}
QByteArray hash(const QImage& image){return QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()),QCryptographicHash::Sha256).toHex();}
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    try{
        if(argc!=2)throw std::runtime_error("resources required");QTemporaryDir owned;
        QImage fixture(2400,1600,QImage::Format_RGB888);fixture.fill(QColor(145,104,72));const auto photo=owned.filePath("owned.png");
        if(!QImageWriter(photo,"PNG").write(fixture))throw std::runtime_error("fixture write failed");
        QElapsedTimer tick;tick.start();qint64 last=0,maxGap=0;int beats=0;QTimer heartbeat;heartbeat.setInterval(5);
        QObject::connect(&heartbeat,&QTimer::timeout,&app,[&]{const auto now=tick.elapsed();maxGap=std::max(maxGap,now-last);last=now;++beats;});heartbeat.start();
        PreviewController c(std::filesystem::path(QString::fromLocal8Bit(argv[1]).toStdWString()),nullptr,{},owned.filePath("edits"));until([&]{return c.ready()||c.failed();});
        c.setAdjustment("grainActive",false);c.setAdjustment("halationActive",false);c.setAdjustment("glareActive",false);
        QJsonObject report;QElapsedTimer clock;clock.start();c.openRaw(QUrl::fromLocalFile(photo));until([&]{return !c.busy()&&c.hasFrame();});report["open_ms"]=clock.elapsed();
        auto measure=[&](const char* name,auto action){QCoreApplication::processEvents();maxGap=0;last=tick.elapsed();const int before=beats;clock.restart();action();const auto call=clock.elapsed();until([&]{return !c.busy()&&!c.dirty();});QCoreApplication::processEvents();report[QString(name)+"_call_ms"]=call;report[QString(name)+"_settle_ms"]=clock.elapsed();report[QString(name)+"_heartbeat_max_gap_ms"]=maxGap;report[QString(name)+"_heartbeats"]=beats-before;};
        measure("post",[&]{c.setPostAdjustment("exposure",.4);});report["post_hash"]=QString::fromLatin1(hash(c.previewImage()));
        measure("geometry",[&]{c.setStraightenDegrees(2);});report["geometry_hash"]=QString::fromLatin1(hash(c.previewImage()));
        measure("original",[&]{report["original_hash"]=QString::fromLatin1(hash(c.originalImage()));});
        measure("uncropped",[&]{report["uncropped_hash"]=QString::fromLatin1(hash(c.uncroppedImage()));});
        clock.restart();for(int i=0;i<100;++i)c.histogram();report["histogram_100_ms"]=clock.elapsed();
        clock.restart();for(int i=0;i<100;++i)c.postAdjustments();report["post_getter_100_ms"]=clock.elapsed();
        std::cout<<QJsonDocument(report).toJson(QJsonDocument::Compact).constData()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
