#include "output.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <thread>

namespace spkhost {
std::string engine_space_name(const std::string& wire) {
    std::string w;
    for (char c : wire) w.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    if (w == "srgb") return "sRGB";
    if (w == "display-p3" || w == "display p3" || w == "p3") return "Display P3";
    if (w == "prophoto" || w == "prophoto rgb" || w == "romm") return "ProPhoto RGB";
    return {};
}

// The transfer functions, as `Canvas/Shaders.metal` numbers them: 0 sRGB
// (and Display P3), 1 ProPhoto, 2 Adobe RGB (1998), 3 BT.709/2020, 4 identity.
static inline float spow(float v, float p) {
    const float s = v < 0.0f ? -1.0f : (v > 0.0f ? 1.0f : 0.0f);
    return s * std::pow(std::fabs(v), p);
}

float cctf_decode_mode(float v, uint32_t mode) {
    switch (mode) {
        case 0: return (0.040449936f >= v) ? v / 12.92f : spow((v + 0.055f) / 1.055f, 2.4f);
        case 1: return (v < 16.0f * (1.0f / 512.0f)) ? v / 16.0f : std::pow(v, 1.8f);
        case 2: return std::pow(v, 563.0f / 256.0f);
        case 3: {
            const float alpha = 1.099f, beta = 0.018f;
            const float bp = alpha * std::pow(beta, 0.45f) - (alpha - 1.0f);
            return (bp > v) ? v / 4.5f : spow((v + (alpha - 1.0f)) / alpha, 1.0f / 0.45f);
        }
        default: return v;
    }
}

float cctf_encode_mode(float v, uint32_t mode) {
    switch (mode) {
        case 0: return (v <= 0.0031308f) ? 12.92f * v : 1.055f * spow(v, 1.0f / 2.4f) - 0.055f;
        case 1: return (v < 1.0f / 512.0f) ? v * 16.0f : std::pow(v, 1.0f / 1.8f);
        case 2: return std::pow(v, 256.0f / 563.0f);
        case 3: {
            const float alpha = 1.099f, beta = 0.018f;
            return (beta > v) ? v * 4.5f : alpha * spow(v, 0.45f) - (alpha - 1.0f);
        }
        default: return v;
    }
}

bool OutputTransform::fetch(spk_engine* engine, const std::string& src, const std::string& dst,
                            std::string& error) {
    src_ = src;
    dst_ = dst;
    char* json = nullptr;
    const float* cmax = nullptr;
    uint32_t count = 0;
    if (spk_output_transform(engine, src.c_str(), dst.c_str(), nullptr, &json, &cmax, &count) != SPK_OK) {
        error = spk_last_error(engine);
        return false;
    }
    spk::Json reply;
    const bool parsed = spk::Json::parse(json, reply, error);
    spk_string_free(json);
    if (!parsed) return false;
    src_mode_ = uint32_t(reply.at("source_cctf_mode").as_int());
    dst_mode_ = uint32_t(reply.at("target_cctf_mode").as_int());
    const auto& m = reply.at("matrix").items();
    for (size_t i = 0; i < 9 && i < m.size(); ++i) matrix_[i] = float(m[i].as_double());
    const spk::Json& g = reply.at("gamut_compress");
    cam16_ = g.at("algorithm").as_string() == "cam16ucs";
    lightness_ = g.at("lightness_compression_active").as_bool();
    for (size_t i = 0; i < 9; ++i) {
        m2x_[i] = float(g.at("m_to_xyz").items()[i].as_double());
        m2r_[i] = float(g.at("m_to_rgb").items()[i].as_double());
    }
    for (size_t i = 0; i < 22; ++i) k_[i] = float(g.at("k").items()[i].as_double());
    nl_ = uint32_t(g.at("cmax_rows").as_int());
    nh_ = uint32_t(g.at("cmax_cols").as_int());
    cmax_.assign(cmax, cmax + count);
    if (size_t(nl_) * nh_ != count) { error = "output transform: C_max table size mismatch"; return false; }
    // Same space in and out: the engine already compressed into it, so the
    // pixels pass through untouched (only quantisation can change them).
    identity_ = src == dst;
    return true;
}

// `Canvas/Shaders.metal` cam16ucsInto / `shaders/gamut.metal`, line for line.
static inline void cam16ucs_into(float rgb[3], const std::array<float, 9>& m2x, const std::array<float, 9>& m2r,
                                 const std::array<float, 22>& k, const float* cmax, uint32_t nL, uint32_t nh,
                                 bool lc_active) {
    using std::pow; using std::fabs; using std::sqrt; using std::log; using std::exp;
    const float F_L = k[0], N_bb = k[1], N_cb = k[2], n_ = k[3], z = k[4], A_w = k[5], c_ = k[6], N_c = k[7];
    const float L_grid0 = k[8], L_grid1 = k[9], h_grid0 = k[10], h_step = k[11];
    const float threshold = k[12], limit = k[13], power_ = k[14];
    const float lc_threshold = k[15], lc_limit = k[16], lc_power = k[17], L_white = k[18];
    const float D0 = k[19], D1 = k[20], D2 = k[21];
    const float e_c = pow(1.64f - pow(0.29f, n_), 0.73f);
    const float inv_FL4 = pow(F_L, 0.25f);

    const float r = rgb[0], g = rgb[1], b_ = rgb[2];
    float X = m2x[0] * r + m2x[1] * g + m2x[2] * b_;
    float Y = m2x[3] * r + m2x[4] * g + m2x[5] * b_;
    float Z = m2x[6] * r + m2x[7] * g + m2x[8] * b_;
    X *= 100.0f; Y *= 100.0f; Z *= 100.0f;

    const float R = 0.401288f * X + 0.650173f * Y - 0.051461f * Z;
    const float G = -0.250268f * X + 1.204414f * Y + 0.045854f * Z;
    const float B = -0.002079f * X + 0.048952f * Y + 0.953127f * Z;

    const float Rc = D0 * R, Gc = D1 * G, Bc = D2 * B;
    const float sR = Rc >= 0.0f ? 1.0f : -1.0f, sG = Gc >= 0.0f ? 1.0f : -1.0f, sB = Bc >= 0.0f ? 1.0f : -1.0f;
    const float fR = pow(F_L * fabs(Rc) / 100.0f, 0.42f);
    const float fG = pow(F_L * fabs(Gc) / 100.0f, 0.42f);
    const float fB = pow(F_L * fabs(Bc) / 100.0f, 0.42f);
    const float Ra = 400.0f * sR * fR / (27.13f + fR) + 0.1f;
    const float Ga = 400.0f * sG * fG / (27.13f + fG) + 0.1f;
    const float Ba = 400.0f * sB * fB / (27.13f + fB) + 0.1f;

    const float a = Ra - 12.0f * Ga / 11.0f + Ba / 11.0f;
    const float bb = (Ra + Ga - 2.0f * Ba) / 9.0f;
    const float hrad = std::atan2(bb, a);
    const float e_t = 0.25f * (std::cos(hrad + 2.0f) + 3.8f);

    const float A = (2.0f * Ra + Ga + Ba / 20.0f - 0.305f) * N_bb;
    const float Aratio = A / A_w;
    const float sJ = Aratio >= 0.0f ? 1.0f : -1.0f;
    const float J = 100.0f * sJ * pow(fabs(Aratio), c_ * z);

    const float den = Ra + Ga + 21.0f * Ba / 20.0f;
    float t = 0.0f;
    if (den != 0.0f) t = (50000.0f / 13.0f * N_c * N_cb * e_t * sqrt(a * a + bb * bb)) / den;
    const float sq = sqrt(fabs(J) / 100.0f);
    const float C = (t > 0.0f) ? pow(t, 0.9f) * sq * e_c : 0.0f;
    const float M = C * inv_FL4;

    const float Mp = (1.0f / 0.0228f) * log(1.0f + 0.0228f * M);
    float Jp = 1.7f * J / (1.0f + 0.007f * J);

    if (lc_active) {
        float Ln = Jp / L_white;
        if (Ln > lc_threshold) {
            const float lsc = lc_limit - lc_threshold;
            const float lx = (Ln - lc_threshold) / lsc;
            const float ly = lx / pow(1.0f + pow(lx, lc_power), 1.0f / lc_power);
            Ln = lc_threshold + lsc * ly;
        }
        Jp = Ln * L_white;
    }

    const float Lc = std::min(std::max(Jp, L_grid0), L_grid1);
    const float Li = (Lc - L_grid0) / (L_grid1 - L_grid0) * float(nL - 1u);
    int l0 = int(std::floor(Li));
    l0 = std::min(std::max(l0, 0), int(nL) - 2);
    const float lf = Li - float(l0);
    const float hi_ = (hrad - h_grid0) / h_step;
    const float hfl = std::floor(hi_);
    const int nhi = int(nh);
    const int h0 = (int(hfl) % nhi + nhi) % nhi;
    const int h1 = (h0 + 1) % nhi;
    const float hf = hi_ - hfl;
    const float c00 = cmax[l0 * nhi + h0], c01 = cmax[l0 * nhi + h1];
    const float c10 = cmax[(l0 + 1) * nhi + h0], c11 = cmax[(l0 + 1) * nhi + h1];
    const float Cmax = (1.0f - lf) * ((1.0f - hf) * c00 + hf * c01) + lf * ((1.0f - hf) * c10 + hf * c11);
    const float safe = Cmax > 1e-9f ? Cmax : 1e-9f;

    float d = Mp / safe;
    if (d > threshold) {
        const float sc = limit - threshold;
        const float xk = (d - threshold) / sc;
        const float yk = xk / pow(1.0f + pow(xk, power_), 1.0f / power_);
        d = threshold + sc * yk;
    }
    const float Mp_new = d * safe;

    const float M_new = (exp(Mp_new * 0.0228f) - 1.0f) / 0.0228f;
    const float J_new = Jp / (1.7f - 0.007f * Jp);
    const float C_new = M_new / inv_FL4;

    const float sJ2 = J_new >= 0.0f ? 1.0f : -1.0f;
    const float A2 = A_w * sJ2 * pow(fabs(J_new) / 100.0f, 1.0f / (c_ * z));
    const float sq2 = sqrt(fabs(J_new) / 100.0f);
    const float t2 = (sq2 > 0.0f && C_new > 0.0f) ? pow(C_new / (sq2 * e_c), 1.0f / 0.9f) : 0.0f;
    const float ca = std::cos(hrad), sa = std::sin(hrad);
    const float p2 = A2 / N_bb + 0.305f;
    const float p3 = 21.0f / 20.0f;
    float a2, b2;
    if (t2 == 0.0f) { a2 = 0.0f; b2 = 0.0f; }
    else {
        const float p1 = ((50000.0f / 13.0f) * N_c * N_cb * e_t) / t2;
        if (fabs(sa) >= fabs(ca)) {
            const float p4 = p1 / sa;
            b2 = (p2 * (2.0f + p3) * (460.0f / 1403.0f)) /
                 (p4 + (2.0f + p3) * (220.0f / 1403.0f) * (ca / sa) - (27.0f / 1403.0f) + p3 * (6300.0f / 1403.0f));
            a2 = b2 * (ca / sa);
        } else {
            const float p5 = p1 / ca;
            a2 = (p2 * (2.0f + p3) * (460.0f / 1403.0f)) /
                 (p5 + (2.0f + p3) * (220.0f / 1403.0f) - ((27.0f / 1403.0f) - p3 * (6300.0f / 1403.0f)) * (sa / ca));
            b2 = a2 * (sa / ca);
        }
    }
    const float Ra2 = (460.0f * p2 + 451.0f * a2 + 288.0f * b2) / 1403.0f;
    const float Ga2 = (460.0f * p2 - 891.0f * a2 - 261.0f * b2) / 1403.0f;
    const float Ba2 = (460.0f * p2 - 220.0f * a2 - 6300.0f * b2) / 1403.0f;

    auto inverse = [&](float va, float Dc) {
        const float vm = va - 0.1f;
        const float sv = vm >= 0.0f ? 1.0f : -1.0f;
        const float base_ = (fabs(vm) < 400.0f) ? (27.13f * fabs(vm)) / (400.0f - fabs(vm)) : 0.0f;
        return (100.0f / F_L) * sv * pow(base_, 1.0f / 0.42f) / Dc;
    };
    const float Rf = inverse(Ra2, D0), Gf = inverse(Ga2, D1), Bf = inverse(Ba2, D2);

    float Xn = 1.86206786f * Rf - 1.01125463f * Gf + 0.14918677f * Bf;
    float Yn = 0.38752654f * Rf + 0.62144744f * Gf - 0.00897398f * Bf;
    float Zn = -0.01584150f * Rf - 0.03412294f * Gf + 1.04996444f * Bf;
    Xn /= 100.0f; Yn /= 100.0f; Zn /= 100.0f;

    rgb[0] = m2r[0] * Xn + m2r[1] * Yn + m2r[2] * Zn;
    rgb[1] = m2r[3] * Xn + m2r[4] * Yn + m2r[5] * Zn;
    rgb[2] = m2r[6] * Xn + m2r[7] * Yn + m2r[8] * Zn;
}

template <typename F>
static void parallel(uint32_t rows, F&& body) {
    const unsigned threads = std::min<unsigned>(std::max(1u, std::thread::hardware_concurrency()), std::max<uint32_t>(1, rows / 8));
    if (threads <= 1) { body(0u, rows); return; }
    std::vector<std::thread> pool;
    const uint32_t step = (rows + threads - 1) / threads;
    for (unsigned t = 0; t < threads; ++t) {
        const uint32_t y0 = t * step, y1 = std::min(rows, y0 + step);
        if (y0 >= y1) break;
        pool.emplace_back([&body, y0, y1] { body(y0, y1); });
    }
    for (auto& th : pool) th.join();
}

void OutputTransform::to_linear(const uint16_t* rgba16, uint32_t w, uint32_t h, uint32_t stride,
                                std::vector<float>& rgb) const {
    rgb.resize(size_t(w) * h * 3);
    // One decode per code value: the source is 16-bit.
    std::vector<float> decode(65536);
    for (uint32_t i = 0; i < 65536; ++i) decode[i] = cctf_decode_mode(float(i) / 65535.0f, src_mode_);
    parallel(h, [&](uint32_t y0, uint32_t y1) {
        for (uint32_t y = y0; y < y1; ++y) {
            const uint16_t* row = rgba16 + size_t(y) * stride * 4;
            float* out = &rgb[size_t(y) * w * 3];
            for (uint32_t x = 0; x < w; ++x) {
                float v[3] = {decode[row[x * 4]], decode[row[x * 4 + 1]], decode[row[x * 4 + 2]]};
                float lin[3];
                if (identity_) {
                    lin[0] = v[0]; lin[1] = v[1]; lin[2] = v[2];
                } else {
                    for (int i = 0; i < 3; ++i)
                        lin[i] = matrix_[i * 3] * v[0] + matrix_[i * 3 + 1] * v[1] + matrix_[i * 3 + 2] * v[2];
                    if (cam16_) cam16ucs_into(lin, m2x_, m2r_, k_, cmax_.data(), nl_, nh_, lightness_);
                }
                out[x * 3] = lin[0]; out[x * 3 + 1] = lin[1]; out[x * 3 + 2] = lin[2];
            }
        }
    });
}

void OutputTransform::encode_rgba8(const std::vector<float>& rgb, std::vector<uint8_t>& rgba) const {
    const size_t n = rgb.size() / 3;
    rgba.resize(n * 4);
    const uint32_t rows = uint32_t((n + 4095) / 4096);
    parallel(rows, [&](uint32_t r0, uint32_t r1) {
        for (size_t p = size_t(r0) * 4096; p < std::min(n, size_t(r1) * 4096); ++p) {
            for (int c = 0; c < 3; ++c) {
                const float e = cctf_encode_mode(rgb[p * 3 + c], dst_mode_);
                rgba[p * 4 + c] = uint8_t(std::lround(std::clamp(e, 0.0f, 1.0f) * 255.0f));
            }
            rgba[p * 4 + 3] = 255;
        }
    });
}

void OutputTransform::encode_rgba16(const std::vector<float>& rgb, std::vector<uint16_t>& rgba) const {
    const size_t n = rgb.size() / 3;
    rgba.resize(n * 4);
    const uint32_t rows = uint32_t((n + 4095) / 4096);
    parallel(rows, [&](uint32_t r0, uint32_t r1) {
        for (size_t p = size_t(r0) * 4096; p < std::min(n, size_t(r1) * 4096); ++p) {
            for (int c = 0; c < 3; ++c) {
                const float e = cctf_encode_mode(rgb[p * 3 + c], dst_mode_);
                rgba[p * 4 + c] = uint16_t(std::lround(std::clamp(e, 0.0f, 1.0f) * 65535.0f));
            }
            rgba[p * 4 + 3] = 65535;
        }
    });
}


} // namespace spkhost

