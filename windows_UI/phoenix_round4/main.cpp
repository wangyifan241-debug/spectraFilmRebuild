// Independent research executable. No material edits and no runtime fallback.
#include "pipeline/pipeline.hpp"
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
 void capture(const QString& name,const Image& i){auto v=values(i);QByteArray bytes(reinterpret_cast<const char*>(v.data()),qsizetype(v.size()*4));write(output+"/"+name+".f32",bytes);auto q=stats(v);q["sha256"]=hash(bytes);q["width"]=int(i.w);q["height"]=int(i.h);q["encoding"]="little-endian float32 packed RGB; semantics by stage";report[name]=q;}
 Params params(const std::string& stock,const QJsonObject& delta={}){Params p;need(init_params(resources.toStdString(),stock,"kodak_portra_endura",json(QJsonDocument::fromJson(read(resources+"/neutral_print_filters.json")).object()),p,err));apply_delta(p,json(QJsonObject{{"input_color_space","ProPhoto RGB"},{"input_cctf_decoding",false},{"output_color_space","ProPhoto RGB"},{"auto_exposure",false},{"grain_active",false},{"halation_active",false},{"glare_active",false}}));apply_delta(p,json(QJsonObject{{"exposure_compensation_ev",exposureEv}}));apply_delta(p,json(delta));return p;}
 Image film(Pipeline& pipe,const Image& input,const QString& prefix,bool trace){pipe.set_source_long_edge(std::max(input.w,input.h),std::max(input.w,input.h));Image start;need(ResearchPipelineAccess::film(pipe,input,start,err));Pipeline::Chain chain{start,{}};if(trace)capture(prefix+"-film-prefix",start);for(size_t i=0;i<Pipeline::kFilmStageCount;i++){Pipeline::Chain next;need((pipe.*Pipeline::kFilmStages[i].run)(chain,next,err));need(gpu->flush(err));chain=next;if(trace)capture(prefix+"-"+Pipeline::kFilmStages[i].name,chain.cur);}return chain.cur;}
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
struct Scanner {
 Lab& lab;gpu::BufferRef chd,base,ixs,ep,x,inv,y,matrix;Vec axis,target;QJsonObject recipe;size_t k=129;
 Scanner(Lab& l,const Params& p,Pipeline& pipe):lab(l){
  Vec dyes=p.film.data.channel_density,zero(81,0),weights(243),illum;need(l.colour.illuminant_values("D50",illum,err));
  size_t missing=0;for(size_t w=0;w<81;w++){bool valid=std::isfinite(p.film.data.base_density[w]);for(int c=0;c<3;c++)valid&=std::isfinite(dyes[3*w+c]);if(!valid){missing++;for(int c=0;c<3;c++)dyes[3*w+c]=0;}}
  const double centres[3]={610,540,450},sigma[3]={30,30,25};
  for(int c=0;c<3;c++){double norm=0;for(size_t w=0;w<81;w++){double z=(380.+5*w-centres[c])/sigma[c];bool valid=std::isfinite(p.film.data.base_density[w]);for(int j=0;j<3;j++)valid&=std::isfinite(p.film.data.channel_density[3*w+j]);weights[3*w+c]=valid?illum[w]*std::exp(-.5*z*z):0;norm+=weights[3*w+c];}if(!(norm>0))throw std::runtime_error("empty detector");for(size_t w=0;w<81;w++)weights[3*w+c]/=norm;}
  chd=l.gpu->upload_persistent_f32(dyes.data(),dyes.size(),err);base=l.gpu->upload_persistent_f32(zero.data(),zero.size(),err);ixs=l.gpu->upload_persistent_f32(weights.data(),weights.size(),err);const float e[6]={1,1,1,0,0,0};ep=l.gpu->upload(e,sizeof e,err);
  std::vector<float> neutral(k*3);for(size_t i=0;i<k;i++)for(int c=0;c<3;c++)neutral[3*i+c]=float(.184*std::exp2(-6.+12.*i/(k-1)));
  auto negative=l.film(pipe,l.upload(neutral,unsigned(k),1),"",false);auto measured=l.values(spectral(negative));axis.resize(k*3);target.resize(k*3);Vec steps((k-1)*3);
  for(size_t i=0;i<k;i++)for(int c=0;c<3;c++){axis[3*i+c]=measured[3*(k-1-i)+c];target[3*i+c]=std::log10(neutral[3*(k-1-i)+c]);}
  for(size_t i=0;i+1<k;i++)for(int c=0;c<3;c++){double d=axis[3*(i+1)+c]-axis[3*i+c];if(!(d>1e-8))throw std::runtime_error("Noninvertible neutral scanner response; no sorting/repair allowed");steps[3*i+c]=1/d;}
  x=l.gpu->upload_persistent_f32(axis.data(),axis.size(),err);inv=l.gpu->upload_persistent_f32(steps.data(),steps.size(),err);y=l.gpu->upload_persistent_f32(target.data(),target.size(),err);
  // Synthetic scene Jacobian around neutral, NOT a real-film parameter fit.
  std::vector<float> probes(4*3,.184f);for(int c=0;c<3;c++)probes[3*(c+1)+c]+=.01f;
  auto probesNeg=l.film(pipe,l.upload(probes,4,1),"",false);auto logs=l.values(invert(spectral(probesNeg)));Mat3 jac;
  for(int row=0;row<3;row++)for(int col=0;col<3;col++)jac.m[row][col]=(std::pow(10.,logs[3*(col+1)+row])-std::pow(10.,logs[row]))/.01;
  auto m=jac.inverse();double jnorm=0,inorm=0;for(int row=0;row<3;row++){double a=0,b=0;for(int c=0;c<3;c++){a+=std::abs(jac.m[row][c]);b+=std::abs(m.m[row][c]);}jnorm=std::max(jnorm,a);inorm=std::max(inorm,b);}if(!std::isfinite(jnorm*inorm)||jnorm*inorm>10000)throw std::runtime_error("Ill-conditioned synthetic scanner Jacobian");for(int row=0;row<3;row++){double sum=m.m[row][0]+m.m[row][1]+m.m[row][2];if(!std::isfinite(sum)||std::abs(sum)<1e-6)throw std::runtime_error("singular scanner matrix");for(int col=0;col<3;col++)m.m[row][col]/=sum;}
  double raw[9];m.to_row_major(raw);matrix=l.gpu->upload_persistent_f32(raw,9,err);
  Mat3 prophotoXYZ;need(l.colour.matrix_RGB_to_XYZ("ProPhoto RGB",nullptr,"CAT02",prophotoXYZ,err));double xyzMatrix[9];prophotoXYZ.to_row_major(xyzMatrix);
  recipe={{"linear_prophoto_to_XYZ_D50",array(Vec(xyzMatrix,xyzMatrix+9))},{"output_recipe","luminance shoulder above 0.184; ProPhoto encode/16-bit; shared engine OutputTransform CAT02/CAM16 to sRGB/16-bit; hard clip at quantization; bundled ICC"},{"status","IDEALIZED_PROXY_NOT_VERIFIED"},{"illuminant","D50 baked engine samples"},{"wavelength_nm",array(l.colour.wavelengths())},{"normalized_detector_weights",array(weights)},{"base_operation","zero base per wavelength before integral; idealized mathematical removal"},{"excluded_missing_wavelengths",double(missing)},{"neutral_axis_logT",array(axis)},{"neutral_axis_logPositive",array(target)},{"scanner_to_linear_prophoto",array(Vec(raw,raw+9))},{"jacobian_condition_inf",jnorm*inorm},{"matrix_source","inverse synthetic scene Jacobian at 0.184, delta 0.01; row normalized to retain neutral"},{"range_ev",QJsonArray{-6,6}},{"outside_range","endpoint clamp; counts reported separately"}};
 }
 Image spectral(const Image& n){Image o=lab.alloc(n);uint32_t meta[3]={uint32_t(n.pixels()),81,0};need(lab.gpu->dispatch("spk_spectral_epilogue",{gpu::Arg::buf(n.buf),gpu::Arg::buf(chd),gpu::Arg::buf(base),gpu::Arg::buf(ixs),gpu::Arg::buf(ep),gpu::Arg::inline_bytes(meta,3),gpu::Arg::buf(o.buf)},n.pixels(),err));need(lab.gpu->flush(err));return o;}
 Image invert(const Image& n){Image o=lab.alloc(n);uint32_t meta[2]={uint32_t(k),uint32_t(n.pixels())};need(lab.gpu->dispatch("spk_curves",{gpu::Arg::buf(n.buf),gpu::Arg::buf(x),gpu::Arg::buf(inv),gpu::Arg::buf(y),gpu::Arg::inline_bytes(meta,2),gpu::Arg::buf(o.buf)},n.pixels(),err));need(lab.gpu->flush(err));return o;}
 Image positive(const Image& logs){Image o=lab.alloc(logs);uint32_t meta[1]={uint32_t(logs.pixels())};need(lab.gpu->dispatch("spk_round4_scan",{gpu::Arg::buf(logs.buf),gpu::Arg::buf(matrix),gpu::Arg::inline_bytes(meta,1),gpu::Arg::buf(o.buf)},logs.pixels(),err));need(lab.gpu->flush(err));return o;}
};
int main(int argc,char** argv){QGuiApplication app(argc,argv);try{
 auto args=app.arguments();if(qEnvironmentVariableIsSet("PHOENIX_ROUND4_EXPOSURE_EV")){bool ok=false;exposureEv=qEnvironmentVariable("PHOENIX_ROUND4_EXPOSURE_EV").toDouble(&ok);if(!ok||!std::isfinite(exposureEv)||exposureEv< -8||exposureEv>8)throw std::runtime_error("Invalid explicit exposure EV");}
 if(args.size()==3&&args[1]=="--view"){
  QStringList names,urls;QDir dir(args[2]);for(const auto& f:dir.entryList({"*.png"},QDir::Files,QDir::Name)){names<<f;urls<<QUrl::fromLocalFile(dir.absoluteFilePath(f)).toString();}
  if(names.empty())throw std::runtime_error("No experiment PNG images");QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("resultNames",names);engine.rootContext()->setContextProperty("resultUrls",urls);engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath()+"/Viewer.qml"));if(engine.rootObjects().empty())throw std::runtime_error("Viewer failed");return app.exec();
 }
 if(args.size()!=5||(args[1]!="--render"&&args[1]!="--render-full"))throw std::runtime_error("Use --render resources RAW-or-synthetic new-output-dir, or --view output-dir");
 QString resources=QDir(args[2]).absolutePath(),output=QDir(args[4]).absolutePath();if(QFile::exists(output))throw std::runtime_error("Output must not exist");need(QDir().mkpath(output));Lab lab(resources,output);
 QJsonObject provenance{{"status","SOFTWARE_COMPARISON_NOT_REAL_FILM_VALIDATION"},{"resources_absolute",resources},{"executable_absolute",QCoreApplication::applicationFilePath()},{"executable_sha256",hash(read(QCoreApplication::applicationFilePath()))},{"gpu",QString::fromStdString(lab.gpu->device_name())},{"real_film_samples",0},{"exposure_ev",exposureEv}};
 const auto buildReceipt=QCoreApplication::applicationDirPath()+"/round4-build.json";if(QFile::exists(buildReceipt)){const auto bytes=read(buildReceipt);auto receipt=QJsonDocument::fromJson(bytes).object();if(receipt["executable_sha256"].toString()!=provenance["executable_sha256"].toString())throw std::runtime_error("Build receipt mismatch");provenance["build_receipt_sha256"]=hash(bytes);provenance["build_source_inventory_sha256"]=receipt["source_inventory_sha256"];}else provenance["build_receipt_status"]="NOT_RECORDED";
 QJsonObject resourceHashes;QDirIterator it(resources,QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){auto p=it.next();resourceHashes[QDir(resources).relativeFilePath(p)]=hash(read(p));}provenance["resource_sha256"]=resourceHashes;
 std::vector<float> rgb;unsigned w,h;
 if(args[3]=="synthetic"){w=256;h=96;rgb.resize(size_t(w)*h*3);for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){double ev=-6.+12.*x/(w-1);double base=.184*std::exp2(ev);for(int c=0;c<3;c++)rgb[(size_t(y)*w+x)*3+c]=float(base*(y<24?1:(int(y/24)-1==c?1:.25)));}provenance["input"]="synthetic neutral / channel-biased exposure ramps; NOT a measured chart";}
 else{io::DecodedRaw decoded;need(io::decode_raw_compatible(std::filesystem::path(args[3].toStdWString()),decoded,err));provenance["input_absolute"]=QFileInfo(args[3]).absoluteFilePath();provenance["input_sha256"]=hash(read(args[3]));provenance["decoder"]=QString::fromStdString(decoded.metadata.libraw_version);provenance["decode_policy"]="compatible16 as-shot WB linear ProPhoto clipped integer AHD";unsigned step=args[1]=="--render-full"?1:std::max(1u,(std::max(decoded.width,decoded.height)+1023)/1024);w=(decoded.width+step-1)/step;h=(decoded.height+step-1)/step;rgb.resize(size_t(w)*h*3);for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(int c=0;c<3;c++)rgb[(size_t(y)*w+x)*3+c]=decoded.rgb[(size_t(y*step)*decoded.width+x*step)*3+c];provenance["preview_stride"]=int(step);provenance["source_width"]=int(decoded.width);provenance["source_height"]=int(decoded.height);}
 auto input=lab.upload(rgb,w,h);lab.capture("input-linear-prophoto",input);
 for(const std::string stock:{"harman_phoenix_200_exp_v1","harman_phoenix_ii_200_exp_v1"}){
  auto p=lab.params(stock);Pipeline pipe(lab.gpu.get(),&lab.colour,&lab.blob,&lab.cache);need(pipe.build(p,err));auto negative=lab.film(pipe,input,QString::fromStdString(stock),true);
  auto calibrationParams=p;calibrationParams.camera.exposure_compensation_ev=0;Pipeline calibration(lab.gpu.get(),&lab.colour,&lab.blob,&lab.cache);need(calibration.build(calibrationParams,err));Scanner scanner(lab,calibrationParams,calibration); // scanner recipe is independent of image exposure edits
  pipe.set_source_long_edge(std::max(w,h),std::max(w,h));
  auto logT=scanner.spectral(negative);auto reversed=scanner.invert(logT);auto positive=scanner.positive(reversed);QString prefix=QString::fromStdString(stock);
  lab.capture(prefix+"-scan-logT",logT);lab.capture(prefix+"-scan-logPositive",reversed);lab.capture(prefix+"-scan-linear-prophoto",positive);lab.exportImage(prefix+"-round4-scan",positive,true);
  write(output+"/"+prefix+"-scan-recipe.json",QJsonDocument(scanner.recipe).toJson());
  auto optical=lab.print(pipe,negative,prefix,true);
  // Witness that node taps preserve the ordinary engine's Optical Print output.
  const auto wire=read_params(p).dump();spk_image supplied{rgb.data(),w,h,3};std::unique_ptr<spk_session,decltype(&spk_session_release)> control(spk_open(lab.engine.get(),&supplied,wire.c_str(),nullptr),spk_session_release);if(!control)throw std::runtime_error(spk_last_error(lab.engine.get()));spk_result ordinary{};if(spk_render(control.get(),"full",&ordinary)!=SPK_OK)throw std::runtime_error(spk_last_error(lab.engine.get()));auto floats=lab.values(optical);unsigned maximum=0;for(unsigned row=0;row<h;row++)for(unsigned col=0;col<w;col++)for(unsigned c=0;c<3;c++){unsigned q=unsigned(std::clamp(floats[(size_t(row)*w+col)*3+c],0.f,1.f)*65535.f+.5f);unsigned ref=ordinary.rgba16[(size_t(row)*ordinary.row_stride_px+col)*4+c];maximum=std::max(maximum,unsigned(std::abs(int(q)-int(ref))));}spk_result_free(&ordinary);if(maximum>1)throw std::runtime_error("Trace changed Optical Print output: "+std::to_string(maximum));lab.report[prefix+"-uninstrumented-control"]=QJsonObject{{"max_RGBA16_error",int(maximum)},{"status","PASS"}};
  lab.exportImage(prefix+"-v1-optical-print",optical,false);write(output+"/"+prefix+"-resolved.json",QJsonDocument(resolved(p)).toJson());
  for(const auto& mode:QStringList{"no-unsharp","no-scanner-correction","no-gamut","direct-negative","no-auto-print-compensation"}){
   auto alternative=lab.params(stock);
   if(mode=="no-unsharp")alternative.scanner.unsharp_mask[0]=alternative.scanner.unsharp_mask[1]=0;
   if(mode=="no-scanner-correction")alternative.scanner.black_correction=alternative.scanner.white_correction=false;
   if(mode=="no-gamut")alternative.io.output_gamut_compress.algorithm="off";
   if(mode=="direct-negative")alternative.io.scan_film=true;
   if(mode=="no-auto-print-compensation")alternative.enlarger.print_exposure_compensation=false;
   Pipeline other(lab.gpu.get(),&lab.colour,&lab.blob,&lab.cache);need(other.build(alternative,err));auto neg=lab.film(other,input,"",false);auto result=lab.print(other,neg,prefix+"-"+mode,true);lab.exportImage(prefix+"-"+mode,result,false);write(output+"/"+prefix+"-"+mode+"-resolved.json",QJsonDocument(resolved(alternative)).toJson());
  }
 }
 QJsonObject outputs;QDirIterator files(output,QDir::Files);while(files.hasNext()){auto f=files.next();outputs[QFileInfo(f).fileName()]=hash(read(f));}provenance["output_sha256"]=outputs;
 lab.report["provenance"]=provenance;write(output+"/report.json",QJsonDocument(lab.report).toJson());std::cout<<"Experiment complete: "<<output.toStdString()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
