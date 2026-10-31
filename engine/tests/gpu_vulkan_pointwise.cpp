// Independent headless checks for the Windows pointwise ports. Guard values
// exercise the rounded-up final workgroup, not just the visible image plane.
#include "gpu/gpu.hpp"
#include "gpu_vulkan_read.hpp"
#include "core/latitude_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
using spk::gpu::Arg;
using spk::gpu::BufferRef;
using spk::gpu::Gpu;
constexpr float kGuard = -12345.25f;
constexpr size_t kGuardCount = 11;
size_t cases = 0;
double maximum_error = 0;
double maximum_scaled_error = 0;

BufferRef output_buffer(Gpu* gpu, size_t count, std::string& error) {
    const std::vector<float> guarded(count + kGuardCount, kGuard);
    return gpu->upload(guarded.data(), guarded.size() * sizeof(float), error);
}

bool check_values(Gpu* gpu, const BufferRef& output, const std::vector<double>& expected,
                  const char* name, double absolute_tolerance, double relative_tolerance,
                  std::string& error) {
    if (!gpu->flush(error)) return false;
    std::vector<float> actual_readback;
    if (!read_test_buffer(gpu, output, actual_readback, error)) return false;
    const float* actual = actual_readback.data();
    for (size_t i = 0; i < expected.size(); ++i) {
        const double delta = std::fabs(double(actual[i]) - expected[i]);
        const double tolerance = absolute_tolerance + relative_tolerance * std::fabs(expected[i]);
        const bool ok = std::isnan(expected[i]) ? std::isnan(actual[i])
                      : std::isinf(expected[i]) ? double(actual[i]) == expected[i]
                      : std::isfinite(actual[i]) && delta <= tolerance;
        if (!ok) {
            char detail[320];
            std::snprintf(detail, sizeof detail,
                "%s case=%zu index=%zu got=%.9g expected=%.17g delta=%.9g tolerance=%.9g",
                name, cases, i, double(actual[i]), expected[i], delta, tolerance);
            error = detail;
            return false;
        }
        if (std::isfinite(delta)) {
            maximum_error = std::max(maximum_error, delta);
            maximum_scaled_error = std::max(maximum_scaled_error,
                                            delta / std::max(1.0, std::fabs(expected[i])));
        }
    }
    for (size_t i = expected.size(); i < expected.size() + kGuardCount; ++i) {
        if (actual[i] != kGuard) {
            error = std::string(name) + " wrote past its logical output count";
            return false;
        }
    }
    ++cases;
    return true;
}

bool check_mul_glare_boost(Gpu* gpu, std::string& error) {
    const uint32_t counts[] = {0, 1, 2, 255, 256, 257, 1025};
    const float illuminant[] = {0.96422f, 1.0f, 0.82521f};
    const float boost[] = {0.25f, 0.125f, 3.0f, 2.0f};
    auto illuminant_buffer = gpu->upload(illuminant, sizeof illuminant, error);
    auto boost_buffer = gpu->upload(boost, sizeof boost, error);
    if (!illuminant_buffer || !boost_buffer) return false;
    for (uint32_t count : counts) {
        std::vector<float> x(std::max(count, 1u)), y(x.size());
        for (uint32_t i = 0; i < x.size(); ++i) {
            x[i] = float(int(i % 37) - 9) / 8.0f;
            y[i] = float(int(i % 11) - 5) / 16.0f;
        }
        if (count > 1) {
            x[0] = boost[0];  // exact protection threshold must pass through
            x[1] = std::nextafter(boost[0], std::numeric_limits<float>::infinity());
        }
        auto input = gpu->upload(x.data(), x.size() * sizeof(float), error);
        auto field = gpu->upload(y.data(), y.size() * sizeof(float), error);
        auto output = output_buffer(gpu, count, error);
        if (!input || !field || !output) return false;
        const uint32_t n[] = {count};
        std::vector<double> expected(count);
        for (uint32_t i = 0; i < count; ++i) expected[i] = double(x[i]) * y[i];
        if (!gpu->dispatch("spk_mul", {Arg::buf(input), Arg::buf(field),
                           Arg::inline_bytes(n, 1), Arg::buf(output)},
                           std::max(count, 1u), error) ||
            !check_values(gpu, output, expected, "mul", 0, 0, error)) return false;

        output = output_buffer(gpu, count, error);
        for (uint32_t i = 0; i < count; ++i)
            expected[i] = double(x[i]) + double(y[i]) / 100.0 * illuminant[i % 3];
        if (!output || !gpu->dispatch("spk_glare_add", {Arg::buf(input), Arg::buf(field),
                           Arg::buf(illuminant_buffer), Arg::inline_bytes(n, 1), Arg::buf(output)},
                           std::max(count, 1u), error) ||
            !check_values(gpu, output, expected, "glare_add", 2e-7, 2e-7, error)) return false;

        output = output_buffer(gpu, count, error);
        for (uint32_t i = 0; i < count; ++i) {
            const double dx = (double(x[i]) - boost[0]) * boost[1];
            expected[i] = x[i] <= boost[0] ? double(x[i])
                : double(x[i]) + double(boost[3]) * (std::expm1(double(boost[2]) * dx) - double(boost[2]) * dx);
        }
        if (!output || !gpu->dispatch("spk_boost", {Arg::buf(input), Arg::buf(boost_buffer),
                           Arg::inline_bytes(n, 1), Arg::buf(output)},
                           std::max(count, 1u), error) ||
            !check_values(gpu, output, expected, "boost", 6e-7, 4e-7, error)) return false;
        std::vector<float> actual_readback;
        if (!read_test_buffer(gpu, output, actual_readback, error)) return false;
        const float* actual = actual_readback.data();
        for (uint32_t i = 0; i < count; ++i) {
            if (x[i] <= boost[0] && actual[i] != x[i]) {
                error = "boost changed a protected value";
                return false;
            }
        }
    }
    // Elementwise independence permits the pipeline to alias input/output.
    // Keep a guarded tail on this plane to exercise that path independently.
    const uint32_t alias_count = 257;
    std::vector<float> alias_values(alias_count + kGuardCount, kGuard);
    std::vector<float> alias_field(alias_count);
    std::vector<double> alias_expected(alias_count);
    for (uint32_t i = 0; i < alias_count; ++i) {
        alias_values[i] = float(int(i % 13) - 6) * 0.125f;
        alias_field[i] = float(int(i % 7) - 3) * 0.25f;
        alias_expected[i] = double(alias_values[i]) * alias_field[i];
    }
    auto alias_input = gpu->upload(alias_values.data(), alias_values.size() * sizeof(float), error);
    auto alias_field_buffer = gpu->upload(alias_field.data(), alias_field.size() * sizeof(float), error);
    const uint32_t alias_n[] = {alias_count};
    if (!alias_input || !alias_field_buffer ||
        !gpu->dispatch("spk_mul", {Arg::buf(alias_input), Arg::buf(alias_field_buffer),
                           Arg::inline_bytes(alias_n, 1), Arg::buf(alias_input)}, alias_count, error) ||
        !check_values(gpu, alias_input, alias_expected, "mul alias", 0, 0, error)) return false;
    return true;
}

bool check_reduce(Gpu* gpu, std::string& error) {
    const uint32_t counts[] = {0, 1, 255, 256, 257, 65537, 131083};
    const uint32_t groups[] = {1, 3, 256};
    for (uint32_t group_count : groups)
        for (uint32_t count : counts)
            for (uint32_t pattern = 0; pattern < 2; ++pattern) {
                std::vector<float> x(std::max(count, 1u));
                for (uint32_t i = 0; i < x.size(); ++i)
                    x[i] = pattern == 0 ? -float(1 + i % 8191) : float(int(i % 8191) - 4095);
                if (pattern == 1 && count > 0) x[count - 1] = std::numeric_limits<float>::infinity();
                auto input = gpu->upload(x.data(), x.size() * sizeof(float), error);
                auto output = output_buffer(gpu, group_count, error);
                if (!input || !output) return false;
                const uint32_t n[] = {count};
                std::vector<double> expected(group_count, -std::numeric_limits<double>::infinity());
                // The group assigned to a sample is analytic; no reduction
                // tree or GPU max primitive is copied into this reference.
                for (uint32_t i = 0; i < count; ++i) {
                    const uint32_t group = (i % (group_count * 256)) / 256;
                    expected[group] = std::max(expected[group], double(x[i]));
                }
                if (!gpu->dispatch("spk_reduce_max", {Arg::buf(input), Arg::inline_bytes(n, 1),
                                   Arg::buf(output)}, size_t(group_count) * 256, error) ||
                    !check_values(gpu, output, expected, "reduce_max", 0, 0, error)) return false;
            }
    return true;
}

bool check_bw(Gpu* gpu, std::string& error) {
    const uint32_t counts[] = {0, 1, 255, 256, 257};
    const float p[][2] = {{1.0f, 0.0f}, {1.5f, -0.1f}, {0.75f, 0.05f}};
    const float luminance[] = {-0.5f, -1e-10f, 0.0f, 1e-10f, 0.01f, 0.06666667f, 0.18f, 0.5f, 0.9f, 1.0f, 2.0f};
    for (const auto& parameters : p)
        for (uint32_t count : counts) {
            std::vector<float> xyz(std::max(count, 1u) * 3);
            std::vector<double> expected(size_t(count) * 3);
            for (uint32_t i = 0; i < std::max(count, 1u); ++i) {
                const float y = luminance[i % 11];
                xyz[3 * i] = 0.75f * y; xyz[3 * i + 1] = y; xyz[3 * i + 2] = 1.25f * y;
                if (i >= count) continue;
                const double corrected = std::clamp(double(parameters[0]) * y + parameters[1], 0.0, 1.0);
                const double scale = corrected / (double(y) + double(1e-10f));
                for (uint32_t ch = 0; ch < 3; ++ch) expected[3 * i + ch] = double(xyz[3 * i + ch]) * scale;
            }
            auto input = gpu->upload(xyz.data(), xyz.size() * sizeof(float), error);
            auto parameters_buffer = gpu->upload(parameters, sizeof parameters, error);
            auto output = output_buffer(gpu, size_t(count) * 3, error);
            const uint32_t n[] = {count};
            if (!input || !parameters_buffer || !output ||
                !gpu->dispatch("spk_bw_correct", {Arg::buf(input), Arg::buf(parameters_buffer),
                                   Arg::inline_bytes(n, 1), Arg::buf(output)}, std::max(count, 1u), error) ||
                !check_values(gpu, output, expected, "bw_correct", 2e-7, 4e-7, error)) return false;
        }
    return true;
}

bool check_edr(Gpu* gpu, std::string& error) {
    const uint32_t counts[] = {0, 1, 255, 256, 257};
    // Neutral input with Y=G makes exact joins independent of dot-product
    // rounding. LUT entries are samples of an analytic affine log-Y curve.
    const float y_values[] = {-1, 0, 1e-10f, 1.0f / 256, 0.03125f,
                             std::nextafter(0.125f, 0.0f), 0.125f, 0.18f,
                             0.5f, std::nextafter(0.5f, 1.0f), 1, 4, 16};
    const uint32_t lut_counts[] = {1, 2, 17};
    for (uint32_t lut_count : lut_counts)
        for (uint32_t count : counts) {
            const float p[] = {0, 1, 0, -8, 0.1f, 0.125f, 0.5f, float(lut_count)};
            const uint32_t effective_lut_count = std::max(lut_count, 2u);
            std::vector<float> lut(effective_lut_count), rgb(std::max(count, 1u) * 3);
            for (uint32_t i = 0; i < effective_lut_count; ++i)
                lut[i] = -7.0f + 0.75f * (10.0f * float(i) / float(effective_lut_count - 1));
            std::vector<double> expected(size_t(count) * 3);
            for (uint32_t i = 0; i < std::max(count, 1u); ++i) {
                const float raw = y_values[i % 13];
                rgb[3 * i] = 0.5f * raw; rgb[3 * i + 1] = raw; rgb[3 * i + 2] = 1.5f * raw;
                if (i >= count) continue;
                const double y = std::max(double(raw), 0.0);
                double mapped = y;
                if (y > 0 && (y < p[5] || y > p[6])) {
                    const double log_y = std::clamp(std::log2(y), -8.0, 2.0);
                    mapped = std::exp2(-7.0 + 0.75 * (log_y + 8.0));
                }
                const double scale = mapped / (y + double(1e-10f));
                for (uint32_t ch = 0; ch < 3; ++ch)
                    expected[3 * i + ch] = double(rgb[3 * i + ch]) * scale;
            }
            auto input = gpu->upload(rgb.data(), rgb.size() * sizeof(float), error);
            auto parameters = gpu->upload(p, sizeof p, error);
            auto lut_buffer = gpu->upload(lut.data(), lut.size() * sizeof(float), error);
            auto output = output_buffer(gpu, size_t(count) * 3, error);
            const uint32_t n[] = {count};
            if (!input || !parameters || !lut_buffer || !output ||
                !gpu->dispatch("spk_edr", {Arg::buf(input), Arg::buf(parameters), Arg::buf(lut_buffer),
                                   Arg::inline_bytes(n, 1), Arg::buf(output)}, std::max(count, 1u), error) ||
                !check_values(gpu, output, expected, "edr", 5e-7, 8e-7, error)) return false;
        }
    return true;
}

double reference_decode(float value, uint32_t mode, float bt_breakpoint) {
    const double v = value;
    if (mode == 0) return value <= 0.040449936f ? v / double(12.92f)
                        : std::pow((v + double(0.055f)) / double(1.055f), double(2.4f));
    if (mode == 1) return value < 0.03125f ? v / 16 : std::pow(v, double(1.8f));
    if (mode == 2) return v < 0 ? std::numeric_limits<double>::quiet_NaN()
                              : std::pow(v, double(563.0f / 256.0f));
    if (mode == 3) return value < bt_breakpoint ? v / 4.5
                        : std::pow((v + double(1.099f - 1.0f)) / double(1.099f), double(1.0f / 0.45f));
    return v;
}

bool check_highlight_headroom(Gpu* gpu, std::string& error) {
    const float p[]={1.0f/8.0f,-1.0f,1.0f,4.0f,1.0f/0.184f,2.0f,0.5f};
    auto parameters=gpu->upload(p,sizeof p,error);
    if(!parameters)return false;
    const auto smooth=[](double t){t=std::clamp(t,0.0,1.0);return t*t*(3.0-2.0*t);};
    for(uint32_t count : {0u,1u,2u,255u,256u,257u,1025u}) {
        std::vector<float> input(std::max(count,1u)*3);
        for(uint32_t i=0;i<std::max(count,1u);++i) {
            // Zero, negative-only, channel-clipped red, both transition ends,
            // then bright values above the frame maximum. Never log a zero.
            const float m[]={0,-1,0.184f,0.736f,2.944f,4,6,8,16};
            input[3*i]=m[i%9];input[3*i+1]=m[i%9]*0.25f;input[3*i+2]=-0.125f;
        }
        auto in=gpu->upload(input.data(),input.size()*sizeof(float),error);
        auto out=output_buffer(gpu,size_t(count)*3,error);
        if(!in||!out)return false;
        std::vector<double> expected(size_t(count)*3);
        for(uint32_t i=0;i<count;++i) {
            const double m=std::max({input[3*i],input[3*i+1],input[3*i+2]});
            double gain=1;
            if(m>0) {
                const auto lm=std::log2(m);
                const auto near=smooth((lm+std::log2(double(p[0]))-p[1])*p[2]);
                const auto floor=smooth((lm+std::log2(double(p[4]))-p[5])*p[6]);
                gain=std::exp2(p[3]*near*floor);
            }
            for(unsigned c=0;c<3;++c)expected[3*i+c]=input[3*i+c]*gain;
        }
        const uint32_t n[]={count};
        if(!gpu->dispatch("spk_highlight_headroom",{Arg::buf(in),Arg::buf(parameters),
            Arg::inline_bytes(n,1),Arg::buf(out)},std::max(count,1u),error)||
            !check_values(gpu,out,expected,"highlight_headroom",2e-5,3e-5,error))return false;
    }
    return true;
}

bool check_scene_latitude(Gpu* gpu,std::string& error) {
    for(const unsigned count:{0u,1u,255u,256u,257u})for(const unsigned norm:{0u,1u,2u})
    for(const float order:{1.f,2.f,3.5f,4.f})for(const unsigned geometry:{0u,1u,2u}) {
        const float p[]={2,1.5f,-2,1,order,4,float(norm),.288f,.711f,.001f,.184f,
            geometry==0?257.f:16.f,geometry==0?1.f:1024.f,.5f,1,2,-1,2,geometry==2?512.f:0.f};
        std::vector<float> rgb(std::max(count,1u)*3);
        for(unsigned i=0;i<std::max(count,1u);++i) {
            const float value=p[10]*std::exp2(float(int(i%37)-20));
            rgb[i*3]=value;rgb[i*3+1]=value*.7f;rgb[i*3+2]=(i%7==0?-.1f:.4f)*value;
            if(i%41==0)rgb[i*3]=rgb[i*3+1]=rgb[i*3+2]=0;
        }
        std::vector<double> expected(size_t(count)*3);
        for(unsigned i=0;i<count;++i) {
            const double r=std::max(double(rgb[3*i]),0.0),g=std::max(double(rgb[3*i+1]),0.0),b=std::max(double(rgb[3*i+2]),0.0);
            const double ss=r*r+g*g+b*b;
            const double v=norm==1?p[7]*r+p[8]*g+p[9]*b:norm==2?std::max({r,g,b}):ss>0?(r*r*r+g*g*g+b*b*b)/ss:0;
            spk::SceneLatitudeParams settings;settings.highlight_knee=p[0];settings.highlight_room=p[1];
            settings.shadow_knee=p[2];settings.shadow_room=p[3];settings.rolloff=p[4];settings.max_lift=p[5];
            const unsigned w=unsigned(p[11]);
            const double along=p[11]>=p[12]?(double(i%w)+.5)/p[11]:(double(i/w)+p[18]+.5)/p[12];
            if(along>=p[13]){settings.highlight_knee=p[14];settings.highlight_room=p[15];settings.shadow_knee=p[16];settings.shadow_room=p[17];}
            const double E=v>0?std::clamp(std::log2(std::max(v,double(p[10])*std::exp2(-24.0))/p[10]),-24.0,24.0):0;
            const double gain=v>0?std::exp2(spk::slm::delta(E,settings)):1;
            for(unsigned c=0;c<3;++c)expected[3*i+c]=rgb[3*i+c]*gain;
        }
        auto input=gpu->upload(rgb.data(),rgb.size()*sizeof(float),error),parameters=gpu->upload(p,sizeof p,error);
        auto output=output_buffer(gpu,size_t(count)*3,error);const unsigned n[]={count};
        if(!input||!parameters||!output||!gpu->dispatch("spk_scene_latitude",{Arg::buf(input),Arg::buf(parameters),Arg::inline_bytes(n,1),Arg::buf(output)},std::max(count,1u),error)||
           !check_values(gpu,output,expected,"scene_latitude",2e-5,3e-5,error))return false;
    }
    return true;
}

bool check_decode(Gpu* gpu, std::string& error) {
    const float bt_breakpoint = 1.099f * std::pow(0.018f, 0.45f) - (1.099f - 1.0f);
    const float srgb_breakpoint = 0.040449936f;
    const std::vector<float> samples = {-0.25f, -0.0f, 0, 1e-10f, 1.0f / 512,
        std::nextafter(0.03125f, 0.0f), 0.03125f, std::nextafter(0.03125f, 1.0f),
        std::nextafter(srgb_breakpoint, 0.0f), srgb_breakpoint,
        std::nextafter(srgb_breakpoint, 1.0f), 0.04045f, 0.081f,
        std::nextafter(bt_breakpoint, 0.0f), bt_breakpoint,
        std::nextafter(bt_breakpoint, 1.0f), 0.18f, 0.5f, 1, 2, 8};
    const uint32_t counts[] = {0, 1, 255, 256, 257, 1025};
    for (uint32_t mode = 0; mode < 6; ++mode)
        for (uint32_t count : counts) {
            std::vector<float> input(std::max(count, 1u));
            std::vector<double> expected(count);
            for (uint32_t i = 0; i < input.size(); ++i) input[i] = samples[i % samples.size()];
            for (uint32_t i = 0; i < count; ++i) expected[i] = reference_decode(input[i], mode, bt_breakpoint);
            auto input_buffer = gpu->upload(input.data(), input.size() * sizeof(float), error);
            auto output = output_buffer(gpu, count, error);
            const uint32_t meta[] = {count, mode};
            if (!input_buffer || !output ||
                !gpu->dispatch("spk_cctf_decode", {Arg::buf(input_buffer), Arg::inline_bytes(meta, 2),
                                   Arg::buf(output)}, std::max(count, 1u), error) ||
                !check_values(gpu, output, expected, "cctf_decode", 1e-7, 7e-7, error)) return false;
        }
    return true;
}
bool check_film_canvas(Gpu* gpu,std::string& error) {
    // Independent analytic cases: identity exposure, a genuinely unexposed
    // pair gap, clear-film density, carrier light, and clipped in-place masks.
    for(uint32_t count:{0u,1u,255u,256u,257u,1025u}) {
        const uint32_t width=std::max(count,1u),n[]={count};
        std::vector<float> p(261,0),rgb(width*3);
        p[0]=p[3]=float(width);p[1]=p[4]=1;p[2]=1;p[5]=p[8]=1;
        p[16]=float(width);p[17]=1;p[44]=1;p[199]=2;
        for(size_t i=0;i<rgb.size();++i)rgb[i]=float(i%43)/64;
        auto in=gpu->upload(rgb.data(),rgb.size()*sizeof(float),error);
        auto params=gpu->upload(p.data(),p.size()*sizeof(float),error);
        auto out=output_buffer(gpu,count*3,error);
        if(!in||!params||!out||!gpu->dispatch("spk_overscan_canvas",{Arg::buf(in),Arg::buf(params),Arg::inline_bytes(n,1),Arg::buf(out)},width,error)||
           !check_values(gpu,out,std::vector<double>(rgb.begin(),rgb.begin()+count*3),"film_canvas_identity",0,0,error))return false;
    }
    const uint32_t w=7,h=4,n[]={w*h};
    std::vector<float> p(261,0),rgb(w*h*3,.25f);
    p[0]=p[3]=w;p[1]=p[4]=h;p[2]=1;p[5]=p[8]=1;p[16]=3;p[17]=h;p[44]=h;p[199]=2;p[260]=4;
    auto in=gpu->upload(rgb.data(),rgb.size()*sizeof(float),error);
    auto params=gpu->upload(p.data(),p.size()*sizeof(float),error);
    auto out=output_buffer(gpu,rgb.size(),error);
    std::vector<double> expected(rgb.size(),.25);
    for(unsigned y=0;y<h;++y)for(unsigned c=0;c<3;++c)expected[(y*w+3)*3+c]=0;
    if(!gpu->dispatch("spk_overscan_canvas",{Arg::buf(in),Arg::buf(params),Arg::inline_bytes(n,1),Arg::buf(out)},w*h,error)||
        !check_values(gpu,out,expected,"film_pair_unexposed_gap",0,0,error))return false;
    p[44]=2;p[199]=0;p[90]=.1f;p[91]=.2f;p[92]=.3f;
    params=gpu->upload(p.data(),p.size()*sizeof(float),error);out=output_buffer(gpu,rgb.size(),error);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<3;++c)expected[(y*w+x)*3+c]=y<2?.25:double(p[90+c]);
    if(!gpu->dispatch("spk_overscan_film_present",{Arg::buf(in),Arg::buf(params),Arg::inline_bytes(n,1),Arg::buf(out)},w*h,error)||
        !check_values(gpu,out,expected,"film_clear_base_density",0,0,error))return false;
    p[184]=p[185]=p[186]=1;
    params=gpu->upload(p.data(),p.size()*sizeof(float),error);out=output_buffer(gpu,rgb.size(),error);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<3;++c)expected[(y*w+x)*3+c]=y<2?.25:0;
    if(!gpu->dispatch("spk_overscan_light",{Arg::buf(in),Arg::buf(params),Arg::inline_bytes(n,1),Arg::buf(out)},w*h,error)||
        !check_values(gpu,out,expected,"film_carrier_light",0,0,error))return false;
    std::vector<float> mask(15,.5f),initial(rgb.size()+kGuardCount,kGuard);
    std::fill(initial.begin(),initial.begin()+rgb.size(),.25f);
    auto maskBuffer=gpu->upload(mask.data(),mask.size()*sizeof(float),error);
    out=gpu->upload(initial.data(),initial.size()*sizeof(float),error);
    const float m[]={-2,1,5,3,float(w),float(h),.5f,1,2};const uint32_t mn[]={15};
    expected.assign(rgb.size(),.25);
    for(unsigned y=1;y<4;++y)for(unsigned x=0;x<3;++x)for(unsigned c=0;c<3;++c)expected[(y*w+x)*3+c]+=.5*m[6+c];
    return gpu->dispatch("spk_overscan_add_mask",{Arg::buf(maskBuffer),Arg::inline_bytes(m,9),Arg::inline_bytes(mn,1),Arg::buf(out)},15,error)&&
        check_values(gpu,out,expected,"film_clipped_in_place_imprint",0,0,error);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: spk_vulkan_pointwise <resources>\n"); return 2; }
    std::string error;
    std::unique_ptr<Gpu> gpu(Gpu::create_vulkan(std::string(argv[1]) + "/vulkan", error));
    if (!gpu) { std::fprintf(stderr, "Vulkan device: %s\n", error.c_str()); return 1; }
    std::printf("Vulkan device: %s\n", gpu->device_name().c_str());
    if (!check_mul_glare_boost(gpu.get(), error) || !check_reduce(gpu.get(), error) ||
        !check_bw(gpu.get(), error) || !check_edr(gpu.get(), error) || !check_decode(gpu.get(), error) ||
        !check_highlight_headroom(gpu.get(), error) || !check_scene_latitude(gpu.get(), error)||!check_film_canvas(gpu.get(),error)) {
        std::fprintf(stderr, "pointwise: %s\n", error.c_str()); return 1;
    }
    std::printf("pointwise: %zu analytic/float64 cases, max abs error %.9g, "
                "max error/max(1,abs(reference)) %.9g, all guards intact\n",
                cases, maximum_error, maximum_scaled_error);
    return 0;
}
