// Offline calibration of the IMPLEMENTED model's print white balance.
// This is not a measurement or calibration against physical HARMAN film.
#include "spektrafilm/spk_engine.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
 if(argc!=3)throw std::runtime_error("Expected resources and output report");
 const auto resources=app.arguments()[1];
 QFile filtersFile(resources+"/neutral_print_filters.json");if(!filtersFile.open(QIODevice::ReadOnly))throw std::runtime_error("Missing print filter database");
 const auto filters=QJsonDocument::fromJson(filtersFile.readAll()).object();
 std::unique_ptr<spk_engine,decltype(&spk_engine_destroy)> engine(spk_engine_create(resources.toUtf8().constData(),nullptr),spk_engine_destroy);
 const auto require=[&](bool ok){if(!ok)throw std::runtime_error(spk_last_error(engine.get()));};require(bool(engine));
 std::vector<float> patch(16*16*3,.184f);spk_image input{patch.data(),16,16,3};
 QJsonObject entries;
 for(const auto stock:{"harman_phoenix_200_exp_v1","harman_phoenix_ii_200_exp_v1"}){
  const auto pack=filters["kodak_portra_endura"].toObject()["TH-KG3"].toObject()[stock].toArray();
  if(pack.size()!=3)throw std::runtime_error("Missing explicit model print starting pack");
  const double initialM=pack[1].toDouble(),initialY=pack[2].toDouble();
  QJsonObject open{{"film_stock",stock},{"print_stock","kodak_portra_endura"},{"input_color_space","ProPhoto RGB"},{"input_cctf_decoding",false},{"output_color_space","sRGB"},{"grain_active",false},{"halation_active",false},{"glare_active",false},{"auto_exposure",false},{"filter_shift_scale",60}};
  // auto_exposure is a boolean wire field; no hidden product-default assumptions.
  const auto bytes=QJsonDocument(open).toJson(QJsonDocument::Compact);
  std::unique_ptr<spk_session,decltype(&spk_session_release)> session(spk_open(engine.get(),&input,bytes.constData(),nullptr),spk_session_release);require(bool(session));
  const auto evaluate=[&](std::array<double,2> shift){
   QJsonObject delta{{"m_filter_shift",shift[0]},{"y_filter_shift",shift[1]}};
   const auto json=QJsonDocument(delta).toJson(QJsonDocument::Compact);require(spk_set_params(session.get(),json.constData(),nullptr)==SPK_OK);
   spk_result result{};require(spk_render(session.get(),"full",&result)==SPK_OK);
   const double r=std::max(1.,double(result.rgba16[0])),g=std::max(1.,double(result.rgba16[1])),b=std::max(1.,double(result.rgba16[2]));spk_result_free(&result);
   return std::array<double,2>{std::log(r/g),std::log(b/g)};
  };
  std::array<double,2> shift{0,0};auto current=evaluate(shift);
  const auto loss=[](const auto& a){return a[0]*a[0]+a[1]*a[1];};
  for(int it=0;it<20&&loss(current)>1e-8;it++){
   const double h=.01;auto m=shift,y=shift;m[0]+=h;y[1]+=h;
   const auto fm=evaluate(m),fy=evaluate(y);const double a=(fm[0]-current[0])/h,b=(fy[0]-current[0])/h,c=(fm[1]-current[1])/h,d=(fy[1]-current[1])/h,det=a*d-b*c;
   if(std::abs(det)<1e-8)break;
   const std::array<double,2> step{(-d*current[0]+b*current[1])/det,(c*current[0]-a*current[1])/det};
   bool moved=false;for(double rate:{1.,.5,.25,.1}){std::array<double,2> next{std::clamp(shift[0]+rate*step[0],std::max(-1.,-initialM/60),std::min(1.,(200-initialM)/60)),std::clamp(shift[1]+rate*step[1],std::max(-1.,-initialY/60),std::min(1.,(200-initialY)/60))};const auto f=evaluate(next);if(loss(f)<loss(current)){shift=next;current=f;moved=true;break;}}if(!moved)break;
  }
  QFile source(resources+"/profiles/"+stock+".json");if(!source.open(QIODevice::ReadOnly))throw std::runtime_error("Missing source profile");
  const auto hash=QString::fromLatin1(QCryptographicHash::hash(source.readAll(),QCryptographicHash::Sha256).toHex());
  entries[stock]=QJsonObject{{"CMY",QJsonArray{pack[0].toDouble(),initialM+60*shift[0],initialY+60*shift[1]}},{"log_rgb_ratio_residual",QJsonArray{current[0],current[1]}},{"source_profile_sha256",hash},{"converged",loss(current)<1e-5}};
 }
 const QJsonObject report{{"format","SpektraLab.PhoenixPrintCalibration"},{"version",1},{"paper","kodak_portra_endura"},{"illuminant","TH-KG3"},{"evidence","C: neutral model-grey through actual Vulkan renderer; NOT physical film measurements"},{"entries",entries}};
 QFile file(app.arguments()[2]);if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(report).toJson())<0)throw std::runtime_error("Cannot write calibration report");
 std::cout<<QJsonDocument(report).toJson().constData();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
