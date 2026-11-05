// Independent research executable. No material edits and no runtime fallback.
#include "pipeline/pipeline.hpp"
#include "desktop/phoenix_round5_scan.hpp"
#include "io/raw_decoder.hpp"
#include "io/image_writer.hpp"
#include "output.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QColorSpace>
#include <QUrl>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <memory>
#include <functional>
#include <cstring>
namespace spk { struct ResearchPipelineAccess {
 static bool film(Pipeline& p,const Image& i,Image& o,std::string& e){return p.film_prefix(i,{i.h,i.w},o,e);}
 static bool print(Pipeline& p,std::string& e){return p.print_prefix(e);}
 static QJsonObject constants(const Pipeline& p){QJsonArray gain,exposure;for(int c=0;c<3;c++){gain.append(p.print_gain_[c]);exposure.append(p.print_exposure_gain_[c]);}return {{"internal_gain",gain},{"print_exposure_gain",exposure}};}
 static bool trace(Pipeline& p,const std::string& stage,const Image& input,Image& out,const std::function<void(const char*,const Image&)>& tap,std::string& e){
  Image cur=input,next;
  auto node=[&](const char* label,auto f){if(!(p.*f)(cur,next,e)||!p.gpu_->flush(e))return false;cur=next;tap(label,cur);return true;};
  if(stage=="print_spectral"){
   if(!p.params_.io.scan_film){if(!node("paper-spectral",&Pipeline::node_enlarger_spectral)||!node("paper-exposure",&Pipeline::node_print_exposure)||!node("paper-density",&Pipeline::node_print_curves))return false;}
   if(!node("scanner-XYZ",&Pipeline::node_scan_spectral)||!node("scanner-bw-XYZ",&Pipeline::node_bw_correction))return false;
  }else if(stage=="print_linear"){
   if(!node("scanner-linear-RGB",&Pipeline::node_xyz_to_rgb)||!node("gamut-linear-RGB",&Pipeline::node_gamut_compress))return false;
  }else if(stage=="print_scan_finish"){
   if(!node("scanner-blur-RGB",&Pipeline::node_scanner_blur)||!node("unsharp-RGB",&Pipeline::node_unsharp))return false;
  }else return false;
  out=cur;return true;
 }

}; }
using namespace spk;
namespace {
std::string err;
double exposureEv=0;
bool exportOnly=false;
void need(bool ok){if(!ok)throw std::runtime_error(err.empty()?"operation failed":err);}
QString hash(const QByteArray& b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());}
QByteArray read(const QString& p){QFile f(p);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error(("Cannot read "+p).toStdString());return f.readAll();}
void write(const QString& p,const QByteArray& b){QFile f(p);if(!f.open(QIODevice::WriteOnly|QIODevice::NewOnly)||f.write(b)!=b.size())throw std::runtime_error(("Cannot create "+p).toStdString());}
QJsonArray array(const Vec& v){QJsonArray a;for(double x:v)a.append(std::isfinite(x)?QJsonValue(x):QJsonValue());return a;}
Json json(const QJsonObject& q){Json j;need(Json::parse(QJsonDocument(q).toJson(QJsonDocument::Compact).toStdString(),j,err));return j;}
QJsonObject qjson(const Json& j){return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).object();}
QJsonObject resolved(const Params& p){auto q=qjson(read_params(p));q["research_non_wire"]=QJsonObject{{"print_exposure_compensation",p.enlarger.print_exposure_compensation},{"scanner_unsharp",QJsonArray{p.scanner.unsharp_mask[0],p.scanner.unsharp_mask[1]}},{"output_gamut_algorithm",QString::fromStdString(p.io.output_gamut_compress.algorithm)},{"output_cctf_encoding",p.io.output_cctf_encoding}};return q;}
struct Lab {
 std::unique_ptr<spk_engine,decltype(&spk_engine_destroy)> engine{nullptr,spk_engine_destroy};spkhost::OutputTransform display;std::unique_ptr<gpu::Gpu> gpu; Blob blob;Colour colour;SetupCache cache;QString output,resources;QJsonObject report;
 Lab(QString r,QString o):output(o),resources(r){engine.reset(spk_engine_create(r.toUtf8().constData(),nullptr));if(!engine)throw std::runtime_error("Engine creation failed");need(display.fetch(engine.get(),"ProPhoto RGB","sRGB",err));gpu.reset(gpu::Gpu::create_vulkan((r+"/vulkan").toStdString(),err));need(bool(gpu));need(blob.open((r+"/spektrafilm_constants.bin").toStdString(),err));need(colour.init(blob,err));}
 Image upload(const std::vector<float>& data,unsigned w,unsigned h){Image i;i.w=w;i.h=h;i.buf=gpu->upload(data.data(),data.size()*sizeof(float),err);need(bool(i.buf));return i;}
 Image alloc(const Image& in){Image o;o.w=in.w;o.h=in.h;o.buf=gpu->alloc(in.bytes(),err);need(bool(o.buf));return o;}
 std::vector<float> values(const Image& i){need(gpu->flush(err));std::vector<float> v(i.elements());need(gpu->read(i.buf.get(),0,v.data(),i.bytes(),err));return v;}
 QJsonObject stats(const std::vector<float>& v){QJsonArray channels;for(int c=0;c<3;c++){Vec x;double sum=0;size_t bad=0,clip=0;for(size_t j=c;j<v.size();j+=3){double y=v[j];if(!std::isfinite(y)){bad++;continue;}x.push_back(y);sum+=y;clip+=y<0||y>1;}std::sort(x.begin(),x.end());QJsonObject q{{"nonfinite",double(bad)},{"outside_0_1",double(clip)}};if(!x.empty()){q["min"]=x.front();q["max"]=x.back();q["mean"]=sum/x.size();q["p01"]=x[x.size()/100];q["median"]=x[x.size()/2];q["p99"]=x[std::min(x.size()-1,x.size()*99/100)];}channels.append(q);}return {{"channels",channels}};}
 void capture(const QString& name,const Image& i){if(exportOnly)return;auto v=values(i);QByteArray bytes(reinterpret_cast<const char*>(v.data()),qsizetype(v.size()*4));write(output+"/"+name+".f32",bytes);auto q=stats(v);q["sha256"]=hash(bytes);q["width"]=int(i.w);q["height"]=int(i.h);q["encoding"]="little-endian float32 packed RGB; semantics by stage";report[name]=q;}
 Params params(const std::string& stock,const QJsonObject& delta={}){Params p;need(init_params(resources.toStdString(),stock,"kodak_portra_endura",json(QJsonDocument::fromJson(read(resources+"/neutral_print_filters.json")).object()),p,err));apply_delta(p,json(QJsonObject{{"input_color_space","ProPhoto RGB"},{"input_cctf_decoding",false},{"output_color_space","ProPhoto RGB"},{"auto_exposure",false},{"grain_active",false},{"halation_active",false},{"glare_active",false}}));apply_delta(p,json(QJsonObject{{"exposure_compensation_ev",exposureEv}}));apply_delta(p,json(delta));return p;}
 Image film(Pipeline& pipe,const Image& input,const QString& prefix,bool trace){pipe.set_source_long_edge(std::max(input.w,input.h),std::max(input.w,input.h));Image start;need(ResearchPipelineAccess::film(pipe,input,start,err));Pipeline::Chain chain{start,{}};if(trace)capture(prefix+"-film-prefix",start);for(size_t i=0;i<Pipeline::kFilmStageCount;i++){Pipeline::Chain next;need((pipe.*Pipeline::kFilmStages[i].run)(chain,next,err));need(gpu->flush(err));chain=next;if(trace)capture(prefix+"-"+Pipeline::kFilmStages[i].name,chain.cur);}return chain.cur;}
 Image film(Pipeline& pipe,const Image& input){return film(pipe,input,"",false);}
 Image print(Pipeline& pipe,const Image& negative,const QString& prefix,bool trace){need(ResearchPipelineAccess::print(pipe,err));if(trace)report[prefix+"-constants"]=ResearchPipelineAccess::constants(pipe);Pipeline::Chain chain{negative,{}};for(size_t i=0;i<Pipeline::kPrintStageCount;i++){Pipeline::Chain next;std::string stage=Pipeline::kPrintStages[i].name;
 if(trace&&(stage=="print_spectral"||stage=="print_linear"||stage=="print_scan_finish")){next=chain;need(ResearchPipelineAccess::trace(pipe,stage,chain.cur,next.cur,[&](const char* label,const Image& im){capture(prefix+"-"+label,im);},err));}
 else need((pipe.*Pipeline::kPrintStages[i].run)(chain,next,err));need(gpu->flush(err));chain=next;if(trace)capture(prefix+"-"+Pipeline::kPrintStages[i].name,chain.cur);}return chain.cur;}
 void exportImage(const QString& name,const Image& i,bool linearProPhoto){auto v=values(i);QImage image(i.w,i.h,QImage::Format_RGBA64);if(image.isNull())throw std::runtime_error("image allocation");for(unsigned y=0;y<i.h;y++){auto row=reinterpret_cast<QRgba64*>(image.scanLine(y));for(unsigned x=0;x<i.w;x++){double rgb[3];for(int c=0;c<3;c++)rgb[c]=v[(size_t(y)*i.w+x)*3+c];if(linearProPhoto){double Y=.2880402*rgb[0]+.7118741*rgb[1]+.0000857*rgb[2];double gain=Y>.184?(.184+.816*(1-std::exp(-(Y-.184)/.816)))/Y:1.;
for(int c=0;c<3;c++)rgb[c]=colour.cctf_encode(rgb[c]*gain,"ProPhoto RGB");}row[x]=QRgba64::fromRgba64(uint16_t(std::clamp(rgb[0],0.,1.)*65535+.5),uint16_t(std::clamp(rgb[1],0.,1.)*65535+.5),uint16_t(std::clamp(rgb[2],0.,1.)*65535+.5),65535);}}
 std::vector<float> displayLinear;std::vector<uint16_t> encoded;display.to_linear(reinterpret_cast<const uint16_t*>(image.constBits()),i.w,i.h,image.bytesPerLine()/8,displayLinear);display.encode_rgba16(displayLinear,encoded);for(unsigned row=0;row<i.h;row++)std::memcpy(image.scanLine(row),encoded.data()+size_t(row)*i.w*4,size_t(i.w)*8);
 image.setColorSpace(QColorSpace::SRgb);need(image.save(output+"/"+name+".png"));need(io::write_srgb_tiff(std::filesystem::path((output+"/"+name+".tif").toStdWString()),reinterpret_cast<const uint16_t*>(image.constBits()),i.w,i.h,image.bytesPerLine()/8,std::filesystem::path((resources+"/io/sRGB.icc").toStdWString()),err));report[name+"-display"]=QJsonObject{{"sha256_rgba16",hash(QByteArray(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()))},{"icc_sha256",hash(read(resources+"/io/sRGB.icc"))}};
 }
};
}
using Scanner=spk::phoenix5::Scanner<Lab>;
