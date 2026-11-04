// User-visible close path: no private photographs or app-data records.
#include "PreviewController.hpp"
#include "EditStore.hpp"
#include <QCoreApplication>
#include <QColorSpace>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include <iostream>
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void idle(PreviewController& c){QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);poll.start();deadline.start(30000);loop.exec();check(!c.busy()&&!c.dirty()&&!c.failed(),c.status().toUtf8().constData());}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Read owned record");return f.readAll();}
void write(const QString& path,const QByteArray& bytes){QFile f(path);check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"Write owned record");check(f.write(bytes)==bytes.size(),"Complete owned record write");}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
check(argc==2,"Expected resources");QTemporaryDir root;check(root.isValid(),"Private fixtures");const auto image=root.filePath("owned.png"),edits=root.filePath("edits");QImage input(64,64,QImage::Format_RGB32);input.setColorSpace(QColorSpace::SRgb);input.fill(qRgb(140,100,70));check(input.save(image),"Owned PNG");const auto url=QUrl::fromLocalFile(image);const std::filesystem::path resources(app.arguments()[1].toStdWString());
{
 PreviewController c(resources,nullptr,{},edits);idle(c);c.openRaw(url);idle(c);c.setExposureEv(.3);idle(c);check(c.prepareToClose(),"Initial close accepted");
}
spk::desktop::editing::Store store(edits);const auto record=store.filePath(url);auto doc=QJsonDocument::fromJson(read(record)).object();auto source=doc["source"].toObject();source["mtimeMs"]=QString::number(source["mtimeMs"].toString().toLongLong()-1000);doc["source"]=source;auto state=doc["state"].toObject(),versions=state["materialVersions"].toObject();versions["engineResourcesSha256"]=QString(64,'a');state["materialVersions"]=versions;doc["state"]=state;const auto protectedBytes=QJsonDocument(doc).toJson();write(record,protectedBytes);
{
 PreviewController c(resources,nullptr,{},edits);idle(c);c.openRaw(url);idle(c);check(!c.persistenceWarning().isEmpty(),"Reproduced source/resource conflict");c.setExposureEv(.7);idle(c);check(c.prepareToClose(),"Combined conflict never blocks close");check(c.persistenceWarning().isEmpty(),"Current context saved automatically");check(read(record)==protectedBytes,"Original history preserved byte for byte");
}
{
 PreviewController c(resources,nullptr,{},edits);idle(c);c.openRaw(url);idle(c);check(std::abs(c.exposureEv()-.7)<1e-10,"Current session restored on reopen");check(c.persistenceWarning().isEmpty(),"Reopen has no conflict loop");c.setExposureEv(.8);input.fill(qRgb(30,90,150));check(input.save(image),"Owned source replacement");check(c.prepareToClose(),"Source replacement still allows close");const QDir recovery(edits+"/Recovery");const auto files=recovery.entryList({"*.json"},QDir::Files);check(!files.isEmpty(),"Unverified session checkpoint retained");const auto checkpoint=QJsonDocument::fromJson(read(recovery.filePath(files.first()))).object();check(checkpoint["autoApply"]==false,"Untrusted checkpoint cannot silently attach to new source");
}
std::cout<<"Close/reopen recovery: "<<checks<<" checks passed; private fixtures only.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
