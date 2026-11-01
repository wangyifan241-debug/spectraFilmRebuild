// The cross-tier headroom reference is the exact full-source statistic,
// measured with bounded bands. No golden photograph or GPU-specific hash.
#include "pipeline/pipeline.hpp"
#include "core/setup_cache.hpp"
#include <QCoreApplication>
#include <QFile>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
    check(argc==2,"resources required");const std::string root=argv[1];std::string error;
    spk::Blob blob;check(blob.open(root+"/spektrafilm_constants.bin",error),error);
    spk::Colour colour;check(colour.init(blob,error),error);spk::SetupCache cache;
    std::unique_ptr<spk::gpu::Gpu> gpu(spk::gpu::Gpu::create_vulkan(root+"/vulkan",error));check(bool(gpu),error);
    QFile filters(QString::fromStdString(root+"/neutral_print_filters.json"));check(filters.open(QIODevice::ReadOnly),"neutral filters");
    spk::Json neutral;check(spk::Json::parse(filters.readAll().toStdString(),neutral,error),error);
    unsigned cases=0;
    for(const auto size:{std::pair{287u,131u},std::pair{131u,287u}})for(double boost:{0.,3.})for(bool black:{false,true}){
        spk::Params params;check(spk::init_params(root,"cinestill_800t","kodak_portra_endura",neutral,params,error),error);
        params.camera.auto_exposure=false;params.io.input_color_space="ProPhoto RGB";params.io.input_cctf_decoding=false;
        params.film_render.halation.boost_ev=boost;
        const auto [w,h]=size;std::vector<float> pixels(size_t(w)*h*3,0);
        if(!black){for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<3;++c)pixels[(size_t(y)*w+x)*3+c]=.003f+float((x*7+y*13+c*17)%256)*.002f;
            for(unsigned c=0;c<3;++c)pixels[(size_t(h-2)*w+w-3)*3+c]=15.f;}
        gpu->begin_frame();spk::Image source;source.w=w;source.h=h;source.c=3;source.buf=gpu->upload(pixels.data(),source.bytes(),error);check(bool(source.buf),error);
        auto base=params;base.film_render.halation.headroom_ev=0;
        spk::Pipeline full(gpu.get(),&colour,&blob,&cache);check(full.build(base,error),error);
        spk::Pipeline::Chain input,exposed,lifted;input.cur=source;
        check(full.film_scale_and_expose(input,exposed,error)&&full.film_boost(exposed,lifted,error),error);
        std::vector<float> raw(lifted.cur.elements());check(gpu->flush(error)&&gpu->read(lifted.cur.buf.get(),0,raw.data(),lifted.cur.bytes(),error),error);
        const double expected=std::max(0.f,*std::max_element(raw.begin(),raw.end()));
        spk::Pipeline bounded(gpu.get(),&colour,&blob,&cache);check(bounded.build(params,error),error);
        check(bounded.needs_highlight_reference(),"fixture lacks no-antihalation headroom");
        double actual=0;check(bounded.measure_highlight_reference(source,actual,error),error);
        check(actual==expected,"banded source peak differs from full-plane peak: "+std::to_string(actual)+" / "+std::to_string(expected));
        ++cases;
    }
    std::cout<<"Source headroom peak: "<<cases<<" exact full-plane/banded cases, portrait/landscape, partial final band, boost 0/3, black/outlier passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
