// Owned scene-linear input, real C ABI and Vulkan renders: not a screenshot mock.
#include "spektrafilm/spk_engine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
struct Picture { unsigned w, h; std::vector<uint16_t> pixels; };
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        std::unique_ptr<spk_engine, decltype(&spk_engine_destroy)> engine(
            spk_engine_create(argv[1], nullptr), spk_engine_destroy);
        check(bool(engine), spk_last_error(nullptr));
        const unsigned w=512, h=336;
        std::vector<float> input(w*h*3);
        for (unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            input[3*(y*w+x)]=.04f+.5f*x/w;
            input[3*(y*w+x)+1]=.05f+.4f*y/h;
            input[3*(y*w+x)+2]=.08f;
        }
        spk_image image{input.data(),w,h,3};
        std::unique_ptr<spk_session, decltype(&spk_session_release)> session(
            spk_open(engine.get(), &image,
                "{\"grain_active\":false,\"glare_active\":false,\"halation_active\":false}", nullptr), spk_session_release);
        check(bool(session), spk_last_error(engine.get()));
#ifndef _WIN32
        // Linux intentionally has no native coverage rasteriser. Exercise its
        // refusal transaction rather than asking it to implement Windows text.
        char* before=nullptr;
        check(spk_get_params(session.get(),&before)==SPK_OK&&before,"get params");
        const std::string original(before);spk_string_free(before);
        for(const char* key : {"date_imprint_active","overscan_active"}) {
            const auto delta=std::string("{\"")+key+"\":true,\"print_exposure\":1.25}";
            check(spk_set_params(session.get(),delta.c_str(),nullptr)==SPK_ERR_USER,
                  std::string("unsupported feature accepted: ")+key);
            check(std::string(spk_last_error(engine.get())).find("not implemented")!=std::string::npos,
                  "unclear platform refusal");
            char* after=nullptr;
            check(spk_get_params(session.get(),&after)==SPK_OK&&after,"get params after refusal");
            const std::string current(after);spk_string_free(after);
            check(current==original,"unsupported request partially mutated session");
        }
        spk_result result{};
        const auto status=spk_render(session.get(),"full",&result);
        const bool valid=status==SPK_OK&&result.rgba16&&result.width==w&&result.height==h;
        spk_result_free(&result);check(valid,"session unusable after feature refusal");
        std::cout<<"Film edge/date unsupported on Linux: refusal and session recovery verified (no skip)\n";
        return 0;
#endif
        auto set=[&](const char* json) {const auto status=spk_set_params(session.get(),json,nullptr);
            check(status==SPK_OK,std::string("set: ")+spk_last_error(engine.get()));};
        auto render=[&] {
            spk_result r{}; auto status=spk_render(session.get(),"full",&r);
            if(status!=SPK_OK){std::string e=spk_last_error(engine.get());spk_result_free(&r);throw std::runtime_error(e);}
            check(r.rgba16&&r.width&&r.height,"empty image");
            Picture p{r.width,r.height,{}};
            for(unsigned y=0;y<r.height;++y) p.pixels.insert(p.pixels.end(),
                r.rgba16+y*r.row_stride_px*4,r.rgba16+y*r.row_stride_px*4+r.width*4);
            spk_result_free(&r); return p;
        };
        const auto plain=render();
        set("{\"date_imprint_active\":true,\"date_imprint_text\":\"26 10 30\",\"date_imprint_size\":2,\"date_imprint_ev\":3}");
        const auto date=render();
        check(date.w==w&&date.h==h&&date.pixels!=plain.pixels,"date must expose the existing frame");
        check(render().pixels==date.pixels,"date is not deterministic");
        set("{\"date_imprint_text\":\"25 01 01\",\"date_imprint_style\":\"dots\"}");
        check(render().pixels!=date.pixels,"date style/text ignored");
        set("{\"date_imprint_active\":false}");
        check(render().pixels==plain.pixels,"disabled date is not bit-exact");
        set("{\"overscan_active\":true,\"overscan_format\":\"135\",\"overscan_edge_text\":\"KODAK PORTRA 400\"}");
        const auto strip=render();
        check(strip.w>w&&strip.h>h,"film canvas must extend beyond the picture");
        check(render().pixels==strip.pixels,"film layout or edge lettering is not deterministic");
        set("{\"overscan_edge_text\":\"KODAK TEST 123\"}");
        check(render().pixels!=strip.pixels,"stock edge lettering did not expose the film");
        set("{\"overscan_edge_text\":\"KODAK PORTRA 400\"}");
        check(render().pixels==strip.pixels,"restoring stock lettering did not restore exposure");
        char* geometry=nullptr;
        check(spk_overscan_geometry(session.get(),&geometry)==SPK_OK&&geometry,"missing film geometry");
        const std::string geo(geometry); spk_string_free(geometry);
        check(geo.find("\"valid\":true")!=std::string::npos,"invalid film geometry");
        set("{\"overscan_holes\":\"black\"}");
        const auto black=render();
        check(black.w==strip.w&&black.h==strip.h&&black.pixels!=strip.pixels,"hole backing ignored");
        set("{\"overscan_camera_seed\":42,\"overscan_frame_seed\":7}");
        check(render().pixels!=black.pixels,"camera/frame registration ignored");
        set("{\"overscan_format\":\"135_half\",\"overscan_pair\":true,\"date_imprint_active\":true,\"date_imprint_text_b\":\"26 10 31\"}");
        const auto pair=render();
        check(pair.w&&pair.h&&pair.pixels!=strip.pixels,"pair layout failed");
        set("{\"overscan_pair\":false,\"date_imprint_active\":false,\"overscan_format\":\"120_6x6\"}");
        spk_result wrong{};
        check(spk_render(session.get(),"full",&wrong)!=SPK_OK,"non-square source must not be stretched into 6x6");
        spk_result_free(&wrong);
        std::vector<float> square(336*336*3,.18f);
        spk_image squareImage{square.data(),336,336,3};
        std::unique_ptr<spk_session,decltype(&spk_session_release)> squareSession(
            spk_open(engine.get(),&squareImage,"{\"overscan_active\":true,\"overscan_format\":\"120_6x6\",\"grain_active\":false,\"glare_active\":false}",nullptr),spk_session_release);
        check(bool(squareSession),"square open failed");
        spk_result six{};
        const auto sixStatus=spk_render(squareSession.get(),"full",&six);
        const bool sixValid=sixStatus==SPK_OK&&six.rgba16&&six.width>336&&six.height>336;
        spk_result_free(&six);check(sixValid,"120 layout failed");
        set("{\"overscan_active\":false}");
        check(render().pixels==plain.pixels,"film bypass is not bit-exact");
        // Every advertised gate, both orientations. Each owns a gate-shaped
        // synthetic input; no unavailable fixture can silently skip coverage.
        struct Gate {const char* name;double along,across;};
        const Gate gates[]={{"135",36.25,24.3},{"135_half",18,24},{"120_645",41.5,56},
            {"120_6x6",56,56},{"120_6x7",69.5,56},{"120_6x8",76,56},{"120_6x9",84,56},
            {"135_xpan",65,24},{"120_6x12",112,56},{"120_6x17",168,56}};
        for(const auto& gate:gates)for(bool vertical:{false,true}) {
            const unsigned a=unsigned(std::lround(192*gate.along/gate.across));
            const unsigned gw=vertical?192:a,gh=vertical?a:192;
            std::vector<float> scene(gw*gh*3,.18f);spk_image im{scene.data(),gw,gh,3};
            const auto json=std::string("{\"overscan_active\":true,\"overscan_format\":\"")+gate.name+
                "\",\"grain_active\":false,\"glare_active\":false,\"overscan_edge_text\":\"TEST 123\"}";
            std::unique_ptr<spk_session,decltype(&spk_session_release)> s(spk_open(engine.get(),&im,json.c_str(),nullptr),spk_session_release);
            check(bool(s),std::string("gate open: ")+gate.name);
            spk_result r{};const auto status=spk_render(s.get(),"full",&r);
            const bool valid=status==SPK_OK&&r.rgba16&&r.width>=gw&&r.height>=gh;
            spk_result_free(&r);check(valid,std::string("gate render: ")+gate.name+": "+spk_last_error(engine.get()));
        }
        std::cout<<"Film edge/date/pair engine checks passed; 135 canvas "<<strip.w<<'x'<<strip.h<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
