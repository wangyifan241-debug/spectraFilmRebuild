#include "spektrafilm/spk_engine.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <iostream>
#include <memory>
#include <vector>
int main(int argc,char** argv){QCoreApplication app(argc,argv);if(argc!=2)return 1;
    std::unique_ptr<spk_engine,decltype(&spk_engine_destroy)> engine(spk_engine_create(argv[1],nullptr),spk_engine_destroy);if(!engine)return 2;
    constexpr unsigned w=1280,h=1024;std::vector<float> rgb(w*h*3,.02f);
    for(unsigned y=90;y<170;++y)for(unsigned x=140;x<230;++x)for(int c=0;c<3;++c)rgb[(y*w+x)*3+c]=2.f;
    for(int c=0;c<3;++c)rgb[(70*w+110)*3+c]=100.f;
    const char* params=R"({"film_stock":"cinestill_800t","print_stock":"kodak_portra_endura","input_color_space":"ProPhoto RGB","input_cctf_decoding":false,"output_color_space":"ProPhoto RGB","output_cctf_encoding":true,"preview_long_edge":800,"auto_exposure":false,"grain_active":false,"glare_active":false,"halation_active":true,"striped":true,"strip_rows":128})";
    spk_image image{rgb.data(),w,h,3};std::unique_ptr<spk_session,decltype(&spk_session_release)> session(spk_open(engine.get(),&image,params,nullptr),spk_session_release);if(!session){std::cerr<<spk_last_error(engine.get());return 3;}
    for(const char* tier:{"full","live"}){spk_result result{};if(spk_render(session.get(),tier,&result)!=SPK_OK){std::cerr<<spk_last_error(engine.get());return 4;}
        QCryptographicHash hash(QCryptographicHash::Sha256);for(unsigned y=0;y<result.height;++y)hash.addData(QByteArrayView(reinterpret_cast<const char*>(result.rgba16+y*result.row_stride_px*4),result.width*8));
        std::cout<<tier<<" "<<hash.result().toHex().constData()<<'\n';spk_result_free(&result);
    }return 0;
}
