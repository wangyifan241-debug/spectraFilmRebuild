#include "PreviewController.hpp"
#include <QGuiApplication>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <iostream>
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
void idle(PreviewController& c){QEventLoop loop;QTimer poll,deadline;poll.setInterval(20);deadline.setSingleShot(true);QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);poll.start();deadline.start(90000);loop.exec();check(!c.busy()&&!c.dirty()&&!c.failed(),c.status().toStdString());}
int index(PreviewController& c,const QString& id){auto films=c.films();for(int i=0;i<films.size();i++)if(films[i].toMap().value("id").toString()==id)return i;throw std::runtime_error("Missing stock "+id.toStdString());}
QString hash(const QImage& image){return QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()),QCryptographicHash::Sha256).toHex();}
int main(int argc,char** argv){QGuiApplication app(argc,argv);try{
 check(argc==4||argc==5,"resources ARW new-evidence-directory required");const auto args=app.arguments();const QString out=args[3];check(!QDir(out).exists()&&QDir().mkpath(out),"Evidence directory must be new");
 const std::filesystem::path resources(args[1].toStdWString());const auto input=QUrl::fromLocalFile(args[2]);QJsonObject report;QString previous;
 for(const QString id:{"harman_phoenix_200_round5","harman_phoenix_ii_200_round5"}){
  const auto edits=out+"/edits-"+id;QImage expected;spk::desktop::RenderSettings settings;
  {PreviewController c(resources,nullptr,{},edits);idle(c);index(c,"kodak_portra_400");index(c,"cinestill_800t");c.setFilmIndex(index(c,id));c.openRaw(input);idle(c);
   check(c.frame()->settings.film_stock==id.toStdString(),"Incorrect selected stock");check(std::max(c.frame()->width(),c.frame()->height())<=1024,"Preview size exceeded");expected=c.previewImage();settings=c.frame()->settings;
   check(!expected.isNull(),"Missing actual preview");const auto fingerprint=hash(expected);check(previous.isEmpty()||previous!=fingerprint,"Two generations produced identical pixels");previous=fingerprint;
   check(expected.save(out+"/"+id+".png"),"Cannot save evidence");c.flushEdits();check(c.persistenceWarning().isEmpty(),"Save failed");report[id]=QJsonObject{{"pixel_sha256",fingerprint},{"width",expected.width()},{"height",expected.height()},{"saved",true}};
  }
  {PreviewController reopened(resources,nullptr,{},edits);idle(reopened);reopened.openRaw(input);idle(reopened);check(reopened.frame()->settings==settings,"Recipe changed across controller recreation");check(reopened.previewImage()==expected,"Reopen changed preview pixels");
   reopened.setFilmIndex(index(reopened,"kodak_portra_400"));idle(reopened);check(!reopened.previewImage().isNull(),"Portra regression");
   reopened.setFilmIndex(index(reopened,"cinestill_800t"));idle(reopened);check(!reopened.previewImage().isNull(),"CineStill regression");
   reopened.setFilmIndex(index(reopened,id));idle(reopened);check(reopened.previewImage()==expected,"Switch back changed pixels");
   if(argc==5){reopened.openRaw(QUrl::fromLocalFile(args[4]));idle(reopened);reopened.selectImage(0);idle(reopened);check(reopened.frame()->settings.film_stock==id.toStdString()&&reopened.previewImage()==expected,"Photo switch lost material or pixels");}
  }
 }
 report["status"]="PASS real Sony RAW/controller/Vulkan; reopen exact; physical accuracy Not Verified";QFile file(out+"/report.json");check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot write report");file.write(QJsonDocument(report).toJson());std::cout<<QJsonDocument(report).toJson().toStdString();return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
