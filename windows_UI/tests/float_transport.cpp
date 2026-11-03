#include "spektrafilm/spk_engine.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    spk_engine* engine=nullptr;spk_session* session=nullptr;
    spk_float_result floating{};spk_result packed{};
    try {
        if(argc!=2)throw std::runtime_error("Expected resources");
        engine=spk_engine_create(argv[1],nullptr);
        if(!engine)throw std::runtime_error(spk_last_error(nullptr));
        std::vector<float> pixels(32*32*3);
        for(size_t i=0;i<pixels.size();++i)pixels[i]=float(.01+double(i%97)/20.);
        spk_image image{pixels.data(),32,32,3};
        session=spk_open(engine,&image,R"({"grain_active":false,"glare_active":false,"output_cctf_encoding":false,"extended_dynamic_range":true,"print_exposure":0.05})",nullptr);
        if(!session)throw std::runtime_error(spk_last_error(engine));
        if(spk_reprint_rgb32f(session,"live",&floating)!=SPK_OK||!floating.rgb)
            throw std::runtime_error(spk_last_error(engine));
        if(spk_reprint(session,"live",&packed)!=SPK_OK)throw std::runtime_error(spk_last_error(engine));
        if(floating.width!=packed.width||floating.height!=packed.height)throw std::runtime_error("Float dimensions differ");
        float peak=0;
        for(unsigned y=0;y<floating.height;++y)for(unsigned x=0;x<floating.width;++x)for(unsigned c=0;c<3;++c) {
            const auto f=floating.rgb[(size_t(y)*floating.width+x)*3+c];
            if(!std::isfinite(f))throw std::runtime_error("Nonfinite float pixel");
            peak=std::max(peak,f);
            const auto quantized=std::lround(std::clamp(f,0.f,1.f)*65535.f);
            if(std::abs(long(quantized)-packed.rgba16[(size_t(y)*packed.row_stride_px+x)*4+c])>1)
                throw std::runtime_error("Float transport differs from actual same-recipe packed render");
        }
        spk_result_free(&packed);spk_session_release(session);session=nullptr;spk_engine_destroy(engine);engine=nullptr;
        if(!std::isfinite(floating.rgb[0]))throw std::runtime_error("Float result lifetime depends on engine");
        spk_float_result_free(&floating);spk_float_result_free(&floating);
        if(floating.rgb||floating.width||floating.height)throw std::runtime_error("Float free did not clear result");
        std::cout<<"Actual Vulkan RGB32F readback, packed-pixel consistency, independent lifetime and double-free guard passed; peak="<<peak<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';spk_result_free(&packed);spk_float_result_free(&floating);if(session)spk_session_release(session);if(engine)spk_engine_destroy(engine);return 1;}
}
