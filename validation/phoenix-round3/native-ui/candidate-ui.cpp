#include "PreviewController.hpp"
#include "MaterialLibrary.hpp"
#include "core/params.hpp"
#include "core/curves.hpp"
#include <QCoreApplication>
#include <QColorSpace>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
namespace {
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void idle(PreviewController& c){QEventLoop loop;QTimer timer,deadline;timer.setInterval(10);deadline.setSingleShot(true);QObject::connect(&timer,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);timer.start();deadline.start(60000);loop.exec();check(!c.busy()&&!c.dirty()&&!c.failed(),c.status().toUtf8().constData());}
QJsonObject json(const QString& file){QFile f(file);check(f.open(QIODevice::ReadOnly),"Phoenix profile absent");return QJsonDocument::fromJson(f.readAll()).object();}
int index(const QVariantList& list,const QString& id){for(int i=0;i<list.size();i++)if(list[i].toMap()["id"]==id)return i;throw std::runtime_error("Phoenix absent from selectable Qt films");}
double difference(const QImage& a,const QImage& b){check(a.size()==b.size(),"Image dimensions differ");double sum=0;for(int y=0;y<a.height();y++)for(int x=0;x<a.width();x++){const auto p=a.pixel(x,y),q=b.pixel(x,y);sum+=std::abs(qRed(p)-qRed(q))+std::abs(qGreen(p)-qGreen(q))+std::abs(qBlue(p)-qBlue(q));}return sum/(a.width()*a.height()*3);}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
const bool iccOnly=argc==4;check(argc==2||argc==3||argc==4,"Expected resources and optional evidence folder");const auto resources=app.arguments()[1];const auto evidence=argc==3?app.arguments()[2]:QString();if(!evidence.isEmpty())check(QDir().mkpath(evidence),"Cannot create evidence folder");const QStringList ids{"harman_phoenix_200_exp_v1","harman_phoenix_ii_200_exp_v1","phx_200_base_neutral_r3","phx_200_base_amp010_r3","phx_200_base_center560_r3","phx_200_base_sigma075_r3","phx_200_dye_portra_proxy_r3","phx_200_dye_nonnegative_r3","phx_ii_200_base_neutral_r3","phx_ii_200_base_amp010_r3","phx_ii_200_base_center560_r3","phx_ii_200_base_sigma075_r3","phx_ii_200_dye_portra_proxy_r3","phx_ii_200_dye_nonnegative_r3"};
for(const auto& id:ids){
 const auto p=json(resources+"/profiles/"+id+".json");MaterialLibrary::validateProfile(p,true);const auto meta=p["metadata"].toObject();
 check((meta["version"]=="0.1.0"||meta["version"]=="0.3.0")&&!meta["photographic_fidelity_validated"].toBool(true),"Experimental evidence claim missing");check(meta["evidence"].isObject()&&meta["assumptions"].isObject(),"Upgrade information missing");
 const auto rows=p["data"].toObject()["density_curves"].toArray();for(int c=0;c<3;c++){double last=-1;for(const auto row:rows){const double v=row.toArray()[c].toDouble();check(std::isfinite(v)&&v>=last&&v>=0&&v<10,"Invalid density response");last=v;}check(last>.5,"Flat response");}
 auto bad=p;auto info=bad["info"].toObject(),model=info["render_model"].toObject();model["grain_particle_area_um2"]=-1;info["render_model"]=model;bad["info"]=info;bool refused=false;try{MaterialLibrary::validateProfile(bad,true);}catch(...){refused=true;}check(refused,"Invalid effects accepted");
 spk::Profile native;std::string error;check(spk::load_profile(resources.toStdString(),id.toStdString(),native,error),error.c_str());spk::Params params;params.film=native;check(spk::digest(params,spk::Json::object(),error),error.c_str());check(params.film_render.grain.particle_area_um2==native.info.grain_particle_area_um2,"Grain estimate ignored");check(params.film_render.halation.halation_strength[0]==native.info.halation_strength_rgb[0],"Halation estimate ignored");
 double coupling[9];spk::dir_couplers_matrix(params.film_render.dir_couplers,coupling);
 for(int c=0;c<3;c++)for(size_t i=1;i<native.data.n_exposure;i++){
  const auto x=[&](size_t k){double v=native.data.log_exposure[k];for(int channel=0;channel<3;channel++)v-=native.normalized_curves[k*3+channel]*coupling[channel*3+c];return v;};
  check(x(i)>x(i-1),"Transformed DIR-coupler exposure axis folds");
 }
}
QTemporaryDir owned;check(owned.isValid(),"Cannot create isolated fixture");QImage target(384,256,QImage::Format_RGB32);target.setColorSpace(QColorSpace::SRgb);
const QRgb patches[]={qRgb(180,45,30),qRgb(30,160,50),qRgb(35,55,190),qRgb(180,150,30),qRgb(160,45,160),qRgb(30,160,170),qRgb(150,100,75),qRgb(210,165,130)};
for(int y=0;y<256;y++)for(int x=0;x<384;x++){
 if(y<85){const double linear=.18*std::exp2(-6+10.*x/383),encoded=linear<=.0031308?12.92*linear:1.055*std::pow(linear,1/2.4)-.055;const int v=std::clamp(int(255*encoded),0,255);target.setPixel(x,y,qRgb(v,v,v));}
 else if(y<180){const auto p=patches[(x/48)%8];const double gain=y<117?.4:y<149?.7:1;target.setPixel(x,y,qRgb(int(qRed(p)*gain),int(qGreen(p)*gain),int(qBlue(p)*gain)));}
 else target.setPixel(x,y,x>180&&x<204&&y>205&&y<229?qRgb(255,255,255):qRgb(3,3,3));
}
const auto input=owned.filePath("gray-colors-highlight.png"),edits=owned.filePath("edits");check(target.save(input),"Fixture save failed");const std::filesystem::path nativeResources(resources.toStdWString());PreviewController c(nativeResources,nullptr,{},edits);idle(c);c.openRaw(QUrl::fromLocalFile(input));idle(c);c.setAdjustment("grainActive",false);c.setAdjustment("halationActive",false);c.setAdjustment("glareActive",false);idle(c);const auto old=c.previewImage();const int oldIndex=c.filmIndex();QImage first;
if(iccOnly){QImage wrong=target;wrong.setColorSpace(QColorSpace::ProPhotoRgb);const auto badICC=owned.filePath("intentionally-wrong-prophoto.png");check(wrong.save(badICC),"ICC negative fixture failed");QImageReader verify(badICC);check(verify.read().colorSpace()==QColorSpace(QColorSpace::ProPhotoRgb),"Wrong embedded ICC was not preserved");c.openRaw(QUrl::fromLocalFile(badICC));idle(c);check(difference(old,c.previewImage())>1,"Incorrect ICC did not change interpretation");c.openRaw(QUrl::fromLocalFile(input));idle(c);check(c.previewImage()==old,"Correct ICC did not restore baseline");std::cout<<"Embedded ICC positive/wrong-tag negative control passed; "<<checks<<" checks\n";return 0;}

if(!evidence.isEmpty()){check(target.save(evidence+"/input.png"),"Input evidence failed");check(old.save(evidence+"/portra.png"),"Baseline evidence failed");}
for(const auto& id:ids){
 c.setFilmIndex(index(c.films(),id));idle(c);check(c.frame()->settings.film_stock==id.toStdString()&&!c.frame()->scan_film&&!c.filmIsMonochrome(),"Selection did not reach spectral negative pipeline");const auto clean=c.previewImage();check(!clean.isNull()&&difference(clean,old)>.05,"Phoenix has no independent response");if(first.isNull())first=clean;else check(difference(clean,first)>.05,"Generations identical");
 if(!evidence.isEmpty())check(clean.save(evidence+"/"+id+".png"),"Phoenix evidence save failed");
 int low=1000,high=-1;for(int x=5;x<379;x++){const auto p=clean.pixel(x,40);const int l=qRed(p)+qGreen(p)+qBlue(p);low=std::min(low,l);high=std::max(high,l);}check(high-low>60,"Gray scale collapsed");std::set<int> colors;for(int y:{100,130,160})for(int x=24;x<384;x+=48)colors.insert(int(clean.pixel(x,y)&0xffffff));check(colors.size()>12,"Color patches collapsed");
 for(int x=8;x<376;x+=8){const auto a=clean.pixel(x-4,40),b=clean.pixel(x,40);check(qRed(b)+qGreen(b)+qBlue(b)+3>=qRed(a)+qGreen(a)+qBlue(a),"Gray ordering inverted");}
 c.setAdjustment("filmExposureEv",-2);idle(c);const auto under=c.previewImage();c.setAdjustment("filmExposureEv",2);idle(c);check(difference(under,c.previewImage())>.05,"Exposure has no effect");for(double ev:{-6.,6.}){c.setAdjustment("filmExposureEv",ev);idle(c);check(!c.previewImage().isNull(),"Extreme exposure failed");}c.setAdjustment("filmExposureEv",0);idle(c);check(c.previewImage()==clean,"Exposure reset not deterministic");
 c.setAdjustment("grainActive",true);c.setAdjustment("grainAmount",1);idle(c);check(difference(clean,c.previewImage())>.001,"Grain has no effect");c.setAdjustment("grainAmount",0);idle(c);check(difference(clean,c.previewImage())<.01,"Zero grain does not bypass");c.setAdjustment("grainActive",false);idle(c);c.setAdjustment("halationActive",true);c.setAdjustment("halationAmount",2);idle(c);check(difference(clean,c.previewImage())>.0001,"Halation has no effect");c.setAdjustment("halationActive",false);idle(c);check(c.previewImage()==clean,"Effects bypass changed pixels");
 const auto png=owned.filePath(id+".png");c.exportImage(QUrl::fromLocalFile(png),1,95,0);idle(c);QImageReader reader(png);const auto exported=reader.read();check(!exported.isNull()&&difference(clean,exported)<.6,"Preview/export diverge");for(const int format:{0,2,3}){const auto file=owned.filePath(id+QString::number(format)+(format==0?".tif":format==3?".jpg":"-16.png"));c.exportImage(QUrl::fromLocalFile(file),format,95,0);idle(c);check(QFileInfo(file).size()>100,"Export produced no file");}
 c.flushEdits();check(c.persistenceWarning().isEmpty(),"Recipe could not save");PreviewController reopened(nativeResources,nullptr,{},edits);idle(reopened);reopened.openRaw(QUrl::fromLocalFile(input));idle(reopened);
 std::cout<<id.toStdString()<<" reopened="<<reopened.frame()->settings.film_stock<<" preview difference="<<difference(reopened.previewImage(),clean)<<" saved settings equal="<<(reopened.frame()->settings==c.frame()->settings)<<'\n';
 check(reopened.frame()->settings.film_stock==id.toStdString()&&reopened.previewImage()==clean,"Versioned recipe did not reopen identically");c.setFilmIndex(oldIndex);idle(c);check(c.previewImage()==old,"Existing Portra changed");
}
std::cout<<"Phoenix: "<<checks<<" checks; SOFTWARE ONLY. Photographic fidelity NOT validated.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
