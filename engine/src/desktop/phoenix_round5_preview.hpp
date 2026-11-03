#pragma once
#include "phoenix_round5_scan.hpp"
#include "preview_host.hpp"
namespace spk::desktop {
inline bool phoenix_round5(const std::string& id){return id=="harman_phoenix_200_round5"||id=="harman_phoenix_ii_200_round5";}
namespace phoenix_preview {
struct Context {
 std::unique_ptr<gpu::Gpu> gpu;Blob blob;Colour colour;SetupCache cache;
 explicit Context(const std::string& resources){using namespace phoenix5;gpu.reset(gpu::Gpu::create_vulkan(resources+"/vulkan",err));need(bool(gpu));need(blob.open(resources+"/spektrafilm_constants.bin",err));need(colour.init(blob,err));}
 Image upload(const std::vector<float>& v,unsigned w,unsigned h){Image i;i.w=w;i.h=h;i.buf=gpu->upload(v.data(),v.size()*sizeof(float),phoenix5::err);phoenix5::need(bool(i.buf));return i;}
 Image alloc(const Image& in){Image o;o.w=in.w;o.h=in.h;o.buf=gpu->alloc(in.bytes(),phoenix5::err);phoenix5::need(bool(o.buf));return o;}
 std::vector<float> values(const Image& in){phoenix5::need(gpu->flush(phoenix5::err));std::vector<float> v(in.elements());phoenix5::need(gpu->read(in.buf.get(),0,v.data(),in.bytes(),phoenix5::err));return v;}
 Image film(Pipeline& pipe,const Image& in){Image out;pipe.set_source_long_edge(std::max(in.w,in.h),std::max(in.w,in.h));phoenix5::need(pipe.run_film(in,out,nullptr,phoenix5::err));phoenix5::need(gpu->flush(phoenix5::err));return out;}
};
}
// Deliberately preview only. The native export gate remains blocked.
inline void render_phoenix_preview(const std::string& resources,const io::DecodedRaw& decoded,const RenderSettings& settings,std::vector<uint16_t>& pixels,spk_result& result){
 using namespace phoenix5;err.clear();
 if(settings.edge.active||settings.date.active||settings.pair.active||settings.placement.active)throw std::runtime_error("Round 5 preview does not yet support film edges, dates, pairs or scene-placement mapping");
 phoenix_preview::Context context(resources);auto& l=context;
 const unsigned step=std::max(1u,(std::max(decoded.width,decoded.height)+1023)/1024),w=(decoded.width+step-1)/step,h=(decoded.height+step-1)/step;
 std::vector<float> rgb(size_t(w)*h*3);for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(int c=0;c<3;c++)rgb[(size_t(y)*w+x)*3+c]=decoded.rgb[(size_t(y*step)*decoded.width+x*step)*3+c];
 Json filters=Json::object();Params p;const std::string stock=settings.film_stock=="harman_phoenix_200_round5"?"harman_phoenix_200_exp_v1":"harman_phoenix_ii_200_exp_v1";
 need(init_params(resources,settings.film_stock,"kodak_portra_endura",filters,p,err));
 Json delta=Json::object();delta.set("input_color_space",Json(std::string("ProPhoto RGB")));delta.set("input_cctf_decoding",Json(false));delta.set("output_color_space",Json(std::string("ProPhoto RGB")));delta.set("auto_exposure",Json(false));delta.set("grain_active",Json(settings.grain_active));delta.set("halation_active",Json(settings.halation_active));delta.set("glare_active",Json(false));delta.set("exposure_compensation_ev",Json(settings.film_exposure_ev));delta.set("grain_amount",Json(settings.grain_amount));delta.set("halation_amount",Json(settings.halation_amount));apply_delta(p,delta);
 Pipeline pipe(l.gpu.get(),&l.colour,&l.blob,&l.cache);need(pipe.build(p,err));auto negative=l.film(pipe,l.upload(rgb,w,h));
 auto calibrationParams=p;calibrationParams.camera.exposure_compensation_ev=0;calibrationParams.film_render.grain.active=false;calibrationParams.film_render.halation.active=false;Pipeline calibration(l.gpu.get(),&l.colour,&l.blob,&l.cache);need(calibration.build(calibrationParams,err));Scanner<phoenix_preview::Context> scanner(l,calibrationParams,calibration);
 auto positive=scanner.positive(scanner.invert(scanner.spectral(negative)));auto values=l.values(positive);for(auto& value:values)value*=settings.print_exposure;pixels.resize(size_t(w)*h*4);
 for(size_t i=0;i<size_t(w)*h;i++){double Y=.2880402*values[i*3]+.7118741*values[i*3+1]+.0000857*values[i*3+2];double gain=Y>.184?(.184+.816*(1-std::exp(-(Y-.184)/.816)))/Y:1.;for(int c=0;c<3;c++)pixels[i*4+c]=uint16_t(std::clamp(l.colour.cctf_encode(values[i*3+c]*gain,"ProPhoto RGB"),0.,1.)*65535+.5);pixels[i*4+3]=65535;}
 result={};result.rgba16=pixels.data();result.width=w;result.height=h;result.row_stride_px=w;
}
}
