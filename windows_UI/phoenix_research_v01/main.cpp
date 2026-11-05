#include "Lab.hpp"
#include <limits>
#include "Research.hpp"

QJsonObject shaderProbe(Lab& l){
 // Non-symmetric matrix distinguishes M*x from transpose(M)*x.
 std::vector<float> logs{0,-1,-2};auto input=l.upload(logs,1,1),out=l.alloc(input);
 const float m[9]={1,2,3,4,5,6,7,8,10};auto matrix=l.gpu->upload(m,sizeof m,err);uint32_t meta=1;
 need(l.gpu->dispatch("spk_round4_scan",{gpu::Arg::buf(input.buf),gpu::Arg::buf(matrix),gpu::Arg::inline_bytes(&meta,1),gpu::Arg::buf(out.buf)},1,err));auto values=l.values(out);double error=0;for(int c=0;c<3;c++)error=std::max(error,std::abs(values[c]-(m[3*c]+.1*m[3*c+1]+.01*m[3*c+2])));
 // Constant density .3, unit-normalized integral => log10 transmission=-.3.
 Vec dyes(243,0),base(81,.3),weights(243,1./81);float epi[6]={1,1,1,0,0,0};auto d=l.gpu->upload_persistent_f32(dyes.data(),243,err),b=l.gpu->upload_persistent_f32(base.data(),81,err),w=l.gpu->upload_persistent_f32(weights.data(),243,err),e=l.gpu->upload(epi,sizeof epi,err);uint32_t sm[3]={1,81,0};
 need(l.gpu->dispatch("spk_spectral_epilogue",{gpu::Arg::buf(input.buf),gpu::Arg::buf(d),gpu::Arg::buf(b),gpu::Arg::buf(w),gpu::Arg::buf(e),gpu::Arg::inline_bytes(sm,3),gpu::Arg::buf(out.buf)},1,err));auto spectral=l.values(out);for(float v:spectral)error=std::max(error,std::abs(double(v)+.3));
 // Curves: (-2,0)->(-3,1); input [-3,-1,1] gives saturated/interpolated [-3,-1,-?].
 Vec x{-2,-2,-2,0,0,0},y{-3,-3,-3,1,1,1},steps{.5,.5,.5};auto xb=l.gpu->upload_persistent_f32(x.data(),6,err),yb=l.gpu->upload_persistent_f32(y.data(),6,err),ib=l.gpu->upload_persistent_f32(steps.data(),3,err);auto ci=l.upload({-3,-1,1},1,1);uint32_t cm[2]={2,1};
 need(l.gpu->dispatch("spk_curves",{gpu::Arg::buf(ci.buf),gpu::Arg::buf(xb),gpu::Arg::buf(ib),gpu::Arg::buf(yb),gpu::Arg::inline_bytes(cm,2),gpu::Arg::buf(out.buf)},1,err));auto curves=l.values(out);float expected[3]={-3,-1,1};for(int c=0;c<3;c++)error=std::max(error,std::abs(double(curves[c]-expected[c])));
 return {{"maximum_absolute_error",error},{"pass",error<2e-5},{"matrix_result",array(Vec(values.begin(),values.end()))},{"spectral_result",array(Vec(spectral.begin(),spectral.end()))},{"curves_result",array(Vec(curves.begin(),curves.end()))}};
}
int main(int argc,char** argv){QGuiApplication app(argc,argv);try{
 auto args=app.arguments();if(args.size()==3&&args[1]=="--view"){
  QStringList names,urls;QDir dir(args[2]);for(const auto& f:dir.entryList({"*.png"},QDir::Files,QDir::Name)){names<<f;urls<<QUrl::fromLocalFile(dir.absoluteFilePath(f)).toString();}
  QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("resultNames",names);engine.rootContext()->setContextProperty("resultUrls",urls);engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath()+"/Viewer.qml"));if(engine.rootObjects().empty())throw std::runtime_error("Viewer failed");return app.exec();
 }
 if(args.size()!=5||args[1]!="--render")throw std::runtime_error("--render resources RAW-or-synthetic new-directory (preview only)");
 QString resources=QDir(args[2]).absolutePath(),output=QDir(args[4]).absolutePath();if(QFile::exists(output))throw std::runtime_error("Output must be new");need(QDir().mkpath(output));Lab lab(resources,output);
 QJsonObject prov{{"gpu",QString::fromStdString(lab.gpu->device_name())},{"executable_sha256",hash(read(QCoreApplication::applicationFilePath()))},{"exposure_ev",0},{"grain",false},{"halation",false},{"glare",false},{"paper","SCAN ONLY; no paper pipeline"},{"policy","DecoupledDeviceToneV01"}};
 QJsonObject hashes;QDirIterator it(resources,QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){auto f=it.next();hashes[QDir(resources).relativeFilePath(f)]=hash(read(f));}prov["resources_sha256"]=hashes;
 std::vector<float> rgb;unsigned w,h;
 if(args[3]=="synthetic"){
  w=97;h=8;rgb.resize(w*h*3);const double bias[8][3]={{1,1,1},{1,.25,.25},{.25,1,.25},{.25,.25,1},{1,.65,.45},{.8,.5,.35},{.6,.35,.22},{.4,.22,.13}};
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(int c=0;c<3;c++)rgb[(y*w+x)*3+c]=float(.184*std::exp2(-6.+x*.125)*bias[y][c]);prov["input"]="neutral, R/G/B, four skin-like RGB exposure sweeps -6..6 step .125; not measured skin";
 }else{io::DecodedRaw decoded;need(io::decode_raw_compatible(std::filesystem::path(args[3].toStdWString()),decoded,err));prov["input_absolute"]=QFileInfo(args[3]).absoluteFilePath();prov["input_sha256"]=hash(read(args[3]));prov["decode"]="compatible16 as-shot WB linear ProPhoto clipped integer AHD";unsigned step=std::max(1u,(std::max(decoded.width,decoded.height)+1023)/1024);w=(decoded.width+step-1)/step;h=(decoded.height+step-1)/step;rgb.resize(size_t(w)*h*3);for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(int c=0;c<3;c++)rgb[(size_t(y)*w+x)*3+c]=decoded.rgb[(size_t(y*step)*decoded.width+x*step)*3+c];prov["preview_stride"]=int(step);}
 auto input=lab.upload(rgb,w,h);lab.capture("input-linear-prophoto",input);lab.exportImage("digital_baseline",input,true);lab.report["shader_domain_probe"]=shaderProbe(lab);
 auto pi=lab.params("harman_phoenix_200_round5"),pii=lab.params("harman_phoenix_ii_200_round5");Device device(lab,pi,pii);lab.report["device"]=device.receipt;
 for(int stock=0;stock<2;stock++){
  const QString label=stock?"phoenix_ii":"phoenix_i";auto p=stock?pii:pi;Pipeline pipe(lab.gpu.get(),&lab.colour,&lab.blob,&lab.cache);need(pipe.build(p,err));auto neg=lab.film(pipe,input);Scanner legacy(lab,p,pipe);
  auto lt=legacy.spectral(neg),inv=legacy.invert(lt),pos=legacy.positive(inv);lab.capture("round5_"+label,pos);lab.capture("legacy_logT_"+label,lt);lab.capture("legacy_logPositive_"+label,inv);lab.exportImage("round5_"+label,pos,true);write(output+"/legacy_recipe_"+label+".json",QJsonDocument(qjson(legacy.recipe)).toJson());write(output+"/resolved_"+label+".json",QJsonDocument(resolved(p)).toJson());
  auto repeat=legacy.positive(legacy.invert(legacy.spectral(neg)));lab.report["legacy_repeat_"+label]=QJsonObject{{"exact",lab.values(pos)==lab.values(repeat)}};
  // Only this local scanner object's detector buffer is replaced. Legacy output already captured.
  legacy.ixs=lab.gpu->upload_persistent_f32(device.weights.data(),device.weights.size(),err);
  std::vector<float> neutral(97*3);for(int i=0;i<97;i++)for(int c=0;c<3;c++)neutral[i*3+c]=float(.184*std::exp2(-6.+i*.125));
  auto nneg=lab.film(pipe,lab.upload(neutral,97,1));auto ntrans=device.optical(lab.values(legacy.spectral(nneg)));Tone tone(ntrans);lab.capture("neutral_device_transmission_"+label,lab.upload(ntrans,97,1));
  auto sharedLog=legacy.spectral(neg);lab.capture("research_logT_"+label,sharedLog);auto transmission=device.optical(lab.values(sharedLog));lab.capture("device_transmission_"+label,lab.upload(transmission,w,h));auto candidate=tone.positive(transmission);auto image=lab.upload(candidate,w,h);lab.capture("research_"+label,image);lab.exportImage("research_"+label,image,true);lab.report["tone_"+label]=QJsonObject{{"neutral_Y_strictly_decreasing",tone.monotonic},{"policy","scalar Y neutral inversion, reciprocal RGB transmission ratio; CPU float32 buffers; not optical paper"}};
 }
 lab.report["provenance"]=prov;write(output+"/report.json",QJsonDocument(lab.report).toJson());std::cout<<"Completed real Vulkan experiment: "<<output.toStdString()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
