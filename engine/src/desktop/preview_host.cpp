#include "preview_host.hpp"
#include "io/image_writer.hpp"
#include "image_io.hpp"
#include "json.hpp"
#include "core/profile.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
#include <cctype>

namespace spk::desktop {
namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using EnginePtr = std::unique_ptr<spk_engine, decltype(&spk_engine_destroy)>;
using SessionPtr = std::unique_ptr<spk_session, decltype(&spk_session_release)>;

double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

bool decode_photo(const fs::path& path,DecodeMode mode,io::DecodedRaw& out,std::string& error) {
    const auto kind=spkhost::sniff_kind(path);
    if(kind!=spkhost::FileKind::Jpeg&&kind!=spkhost::FileKind::Png)
        return mode==DecodeMode::headroom?io::decode_raw_headroom(path,out,error):io::decode_raw_compatible(path,out,error);
    if(mode!=DecodeMode::compatible16){error="RAW headroom decoding is not available for JPEG/PNG";return false;}
    const auto start=Clock::now();spkhost::FloatImage image;spkhost::Probe probe;spkhost::DecodeOptions options;
    options.strict_icc=true;
    if(!spkhost::decode_linear_prophoto(path,options,image,probe,error))return false;
    io::DecodedRaw next;next.width=image.width;next.height=image.height;next.rgb=std::move(image.rgb);
    next.metadata.input_kind=spkhost::kind_name(kind);next.metadata.input_profile=probe.colour_description;
    next.metadata.make=probe.metadata.make;next.metadata.model=probe.metadata.model;
    next.metadata.decoder_name=kind==spkhost::FileKind::Jpeg?"JPEG / ICC":"PNG / ICC";
    next.timings.total_ms=elapsed(start);out=std::move(next);return true;
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string utf8(const fs::path& path) {
    const auto s = path.u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}

Catalog read_catalog(const fs::path& resources) {
    Catalog catalog;
    std::set<std::string> ids;
    for (const auto& entry : fs::directory_iterator(resources / "profiles")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        // Use the engine loader rather than assuming every profile is a full
        // measurement. CineStill 800T is an alias with inherited stage/type;
        // reading only its info object made the entire application refuse to
        // initialize once product profiles were included in the bundle.
        Profile profile;
        std::string error;
        const bool loaded = load_profile(utf8(resources), utf8(entry.path().stem()), profile, error);
        require(loaded, "Bundled stock profile: " + error);
        const auto& info = profile.info;
        Stock stock{info.stock, info.name};
        const auto& stage = info.stage;
        const auto& type = info.type;
        require(!stock.id.empty() && stock.id == utf8(entry.path().stem()) &&
                    std::all_of(stock.id.begin(), stock.id.end(), [](unsigned char c) {
                        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                               (c >= '0' && c <= '9') || c == '_';
                    }), "Invalid stock identifier in bundled profile");
        require(!stock.label.empty() && ids.insert(stock.id).second,
                "Missing or duplicate bundled stock metadata");
        require(type == "negative" || type == "positive", "Unknown bundled stock type: " + type);
        stock.is_positive = type == "positive";
        stock.is_monochrome = info.channel_model == "bw";
        if (stage == "filming") catalog.films.push_back(std::move(stock));
        else if (stage == "printing") catalog.papers.push_back(std::move(stock));
        else throw std::runtime_error("Unknown bundled stock stage: " + stage);
    }
    const auto by_label = [](const Stock& a, const Stock& b) { return a.label < b.label; };
    std::sort(catalog.films.begin(), catalog.films.end(), by_label);
    std::sort(catalog.papers.begin(), catalog.papers.end(), by_label);
    require(!catalog.films.empty() && !catalog.papers.empty(), "Bundled film/paper catalog is empty");
    return catalog;
}

bool validate_settings(const RenderSettings& settings, const Catalog& catalog) {
    const auto contains = [](const std::vector<Stock>& stocks, const std::string& id) {
        return std::any_of(stocks.begin(), stocks.end(), [&](const Stock& s) { return s.id == id; });
    };
    const auto film = std::find_if(catalog.films.begin(), catalog.films.end(),
        [&](const Stock& stock) { return stock.id == settings.film_stock; });
    require(film != catalog.films.end(), "Unknown film stock: " + settings.film_stock);
    require(contains(catalog.papers, settings.print_stock), "Unknown paper stock: " + settings.print_stock);
    const auto paper = std::find_if(catalog.papers.begin(), catalog.papers.end(),
        [&](const Stock& stock) { return stock.id == settings.print_stock; });
    require(film->is_positive || film->is_monochrome == paper->is_monochrome,
            "Film and paper must use the same colour or silver process");
    require(std::isfinite(settings.print_exposure) && settings.print_exposure >= 0.05 &&
                settings.print_exposure <= 20.0, "Print exposure must be between 0.05 and 20");
    const auto range = [](double value, double low, double high, const char* label) {
        require(std::isfinite(value) && value >= low && value <= high,
                std::string(label) + " must be between " + std::to_string(low) + " and " + std::to_string(high));
    };
    // Reject before touching the live session, including NaN/Inf values
    // that cannot be represented faithfully in the JSON parameter delta.
    range(settings.film_exposure_ev, -8.0, 8.0, "Film exposure (EV)");
    range(settings.film_format_mm, 4.0, 200.0, "Film format (mm)");
    range(settings.grain_amount, 0.0, 2.0, "Grain amount");
    range(settings.halation_amount, 0.0, 4.0, "Halation amount");
    range(settings.glare_amount, 0.0, 30.0, "Glare amount");
    range(settings.y_filter_shift, -1.0, 1.0, "Yellow filter shift");
    range(settings.m_filter_shift, -1.0, 1.0, "Magenta filter shift");
    const auto& p=settings.placement;
    require(p.norm=="power"||p.norm=="y"||p.norm=="max","Unknown scene-placement norm");
    range(p.highlight_knee,-24,24,"Highlight knee");range(p.shadow_knee,-24,24,"Shadow knee");
    range(p.highlight_room,0,24,"Highlight room");range(p.shadow_room,0,24,"Shadow room");
    range(p.rolloff,1,4,"Placement rolloff");range(p.max_lift,.25,12,"Shadow lift bound");
    range(p.highlight_pull_back,0,8,"Highlight pull-back");range(p.shadow_pull_back,0,8,"Shadow pull-back");
    const auto& e=settings.edge; const auto& d=settings.date;
    const std::set<std::string> formats{"135","135_half","120_645","120_6x6","120_6x7","120_6x8","120_6x9","135_xpan","120_6x12","120_6x17"};
    require(formats.count(e.format),"Unknown film-edge format");
    require(e.mode=="strip"||e.mode=="filed","Unknown film-edge view");
    require(e.holes=="white"||e.holes=="black","Unknown perforation backing");
    require(e.carrier=="black"||e.carrier=="open"||e.carrier=="none","Unknown film carrier");
    require(std::set<std::string>{"auto","square","rounded","eared","shouldered","kicked"}.count(e.gate),"Unknown film gate");
    require(e.camera_seed>=0&&e.frame_seed>=0&&e.frame_number>=0&&e.frame_number<=99,"Invalid film registration or frame number");
    range(e.fog,0,4,"Edge fog");range(e.leaks,0,4,"Light leaks");
    require(d.style=="lcd"||d.style=="dots"||d.style=="data","Unknown date-back style");
    require(d.corner=="br"||d.corner=="bl"||d.corner=="tr"||d.corner=="tl","Unknown date-back corner");
    require(d.text.size()<=64&&std::all_of(d.text.begin(),d.text.end(),[](unsigned char c){return (c>='0'&&c<='9')||c==' '||c=='-'||c==':'||c=='/'||c=='.'||c=='\'';}),"Date accepts up to 64 digits and date/time separators");
    range(d.exposure_ev,-2,8,"Date exposure");range(d.size,.4,3,"Date size");
    if(settings.pair.active) {
        require(!e.active||e.format=="135_half","A pair requires the half-frame film format");
        range(settings.pair.spacing_mm,.5,2,"Pair spacing");
        for(const auto& h:settings.pair.holes) {
            range(h.scale,1,4,"Pair scale");range(h.x,-1,1,"Pair horizontal placement");range(h.y,-1,1,"Pair vertical placement");
            range(h.exposure_ev,-8,8,"Pair exposure");require(h.turns>=0&&h.turns<=3,"Invalid pair rotation");
        }
    }
    return film->is_positive;
}

std::shared_ptr<const io::DecodedRaw> prepare_input(const std::shared_ptr<const io::DecodedRaw>& raw,
                                                   const RenderSettings& settings,const InputGeometry& g) {
    if(!settings.edge.active&&!settings.date.active)return raw;
    if(settings.pair.active){require(g==InputGeometry{},"A pair uses per-hole geometry, not a whole-piece crop");return raw;}
    require(std::isfinite(g.x)&&std::isfinite(g.y)&&std::isfinite(g.w)&&std::isfinite(g.h)&&g.x>=0&&g.y>=0&&g.w>0&&g.h>0&&g.x+g.w<=1&&g.y+g.h<=1,"Invalid pre-film crop");
    require(g.turns>=0&&g.turns<=3&&std::isfinite(g.straighten)&&std::abs(g.straighten)<=45,"Invalid pre-film direction");
    int x0=std::clamp(int(std::lround(g.x*raw->width)),0,int(raw->width)-1);
    int y0=std::clamp(int(std::lround(g.y*raw->height)),0,int(raw->height)-1);
    int w=std::clamp(int(std::lround((g.x+g.w)*raw->width)),x0+1,int(raw->width))-x0;
    int h=std::clamp(int(std::lround((g.y+g.h)*raw->height)),y0+1,int(raw->height))-y0;
    if(settings.edge.active) {
        // A real gate cannot stretch to an arbitrary crop. Inscribe it inside
        // that crop, centred, with no rescale of the source pixels.
        const std::pair<const char*,double> ratios[]={{"135",36.25/24.3},{"135_half",24./18.},{"120_645",56./41.5},{"120_6x6",1},{"120_6x7",69.5/56.},{"120_6x8",76./56.},{"120_6x9",84./56.},{"135_xpan",65./24.},{"120_6x12",112./56.},{"120_6x17",168./56.}};
        double ratio=1;for(const auto& [name,r]:ratios)if(settings.edge.format==name)ratio=r;
        if(settings.pair.active)ratio=37./24.;
        if(w<h)ratio=1/ratio;
        if(double(w)/h>ratio) {int nw=std::max(1,int(std::lround(h*ratio)));x0+=(w-nw)/2;w=nw;}
        else {int nh=std::max(1,int(std::lround(w/ratio)));y0+=(h-nh)/2;h=nh;}
    }
    if(x0==0&&y0==0&&w==int(raw->width)&&h==int(raw->height)&&g.turns==0&&!g.flip_h&&!g.flip_v&&g.straighten==0)return raw;
    auto cut=std::make_shared<io::DecodedRaw>();
    cut->metadata=raw->metadata;cut->timings=raw->timings;
    cut->width=g.turns%2?h:w;cut->height=g.turns%2?w:h;
    cut->rgb.resize(size_t(cut->width)*cut->height*3);
    const double a=g.straighten*3.14159265358979323846/180,c=std::cos(a),s=std::sin(a);
    const double zoom=std::max(std::abs(c)+std::abs(s)*h/w,std::abs(c)+std::abs(s)*w/h);
    for(unsigned y=0;y<cut->height;++y)for(unsigned x=0;x<cut->width;++x) {
        int xx=g.flip_h?int(cut->width)-1-int(x):int(x),yy=g.flip_v?int(cut->height)-1-int(y):int(y),u=xx,v=yy;
        if(g.turns==1){u=yy;v=h-1-xx;}else if(g.turns==2){u=w-1-xx;v=h-1-yy;}else if(g.turns==3){u=w-1-yy;v=xx;}
        const double sx=std::clamp((w-1)*.5+(c*(u-(w-1)*.5)+s*(v-(h-1)*.5))/zoom,0.,double(w-1))+x0;
        const double sy=std::clamp((h-1)*.5+(-s*(u-(w-1)*.5)+c*(v-(h-1)*.5))/zoom,0.,double(h-1))+y0;
        const unsigned ax=unsigned(sx),ay=unsigned(sy),bx=std::min(ax+1,raw->width-1),by=std::min(ay+1,raw->height-1);
        const double fx=sx-ax,fy=sy-ay;
        for(unsigned k=0;k<3;++k) {
            const auto at=[&](unsigned xx,unsigned yy){return raw->rgb[(size_t(yy)*raw->width+xx)*3+k];};
            cut->rgb[(size_t(y)*cut->width+x)*3+k]=float((at(ax,ay)*(1-fx)+at(bx,ay)*fx)*(1-fy)+(at(ax,by)*(1-fx)+at(bx,by)*fx)*fy);
        }
    }
    return cut;
}

bool monochrome(const RenderSettings& settings, const Catalog& catalog) {
    return std::any_of(catalog.films.begin(), catalog.films.end(), [&](const Stock& s) {
        return s.id == settings.film_stock && s.is_monochrome;
    });
}

void placement_params(Json& params,const ScenePlacement& p) {
    params.set("scene_latitude_active",Json(p.active));params.set("scene_latitude_norm",Json(p.norm));
    params.set("scene_latitude_highlight_knee",Json(p.highlight_knee));params.set("scene_latitude_highlight_room",Json(p.highlight_room));
    params.set("scene_latitude_shadow_knee",Json(p.shadow_knee));params.set("scene_latitude_shadow_room",Json(p.shadow_room));
    params.set("scene_latitude_rolloff",Json(p.rolloff));params.set("scene_latitude_max_lift",Json(p.max_lift));
}
void film_edge_params(Json& p,const RenderSettings& s) {
    const auto& e=s.edge;const auto& d=s.date;
    p.set("overscan_active",Json(e.active));p.set("overscan_format",Json(e.format));
    p.set("overscan_mode",Json(e.mode));p.set("overscan_gate",Json(e.gate));
    p.set("overscan_holes",Json(e.holes));p.set("overscan_carrier",Json(e.carrier));
    p.set("overscan_camera_seed",Json(double(e.camera_seed)));p.set("overscan_frame_seed",Json(double(e.frame_seed)));
    p.set("overscan_frame_number",Json(double(e.frame_number)));p.set("overscan_fog",Json(e.fog));p.set("overscan_leaks",Json(e.leaks));
    // Same stock/gauge inscriptions as Session+FilmEdge.swift, not marketing
    // names substituted into measured emulsion data.
    struct Inscription {const char* id;const char* small;const char* medium;};
    static const Inscription names[]={
        {"kodak_gold_200","KODAK GB 200-7","KODAK 200"},
        {"kodak_portra_800","KODAK PORTRA 800-3","KODAK PORTRA 800"},
        {"kodak_portra_800_push1","KODAK PORTRA 800-3","KODAK PORTRA 800"},
        {"kodak_portra_800_push2","KODAK PORTRA 800-3","KODAK PORTRA 800"},
        {"kodak_ultramax_400","KODAK GC 400","KODAK GC 400"},
        {"kodak_ektachrome_100","KODAK E100","KODAK E100"},
        {"fujifilm_c200","FUJI 200","FUJI 200"},
        {"fujifilm_xtra_400","S-400","FUJI S-400"},
        {"fujifilm_pro_400h","PRO400H","FUJI PRO400H"},
        {"fujifilm_provia_100f","FUJI RDPIII","FUJI RDPIII"},
        {"fujifilm_velvia_100","RVP100","FUJI RVP100"},
        {"kodak_vision3_50d","EASTMAN 5203","EASTMAN 5203"},
        {"kodak_vision3_250d","EASTMAN 5207","EASTMAN 5207"},
        {"kodak_vision3_200t","EASTMAN 5213","EASTMAN 5213"},
        {"kodak_vision3_500t","EASTMAN 5219","EASTMAN 5219"},
        {"cinestill_800t","CINESTILL 800T","CINESTILL 800T"},
        {"open_kodak_trix_7266_033","KODAK TRI-X 7266","KODAK TRI-X 7266"},
        {"open_kodak_doublex_5222_65m_033","EASTMAN 5222","EASTMAN 5222"},
        {"kodak_tri_x_400","KODAK 400TX","KODAK 400TX"},
        {"kodak_tmax_100","KODAK 100TMX","KODAK 100TMX"},
        {"fujifilm_neopan_acros_100_ii","FUJI 100 ACROS II","FUJI 100 ACROS II"},
        {"ilford_hp5_plus_400","ILFORD HP5 PLUS","ILFORD HP5 PLUS"}};
    std::string inscription=s.film_stock;
    if(inscription.starts_with("usr_")&&inscription.size()>29)inscription=inscription.substr(4,inscription.size()-29);
    const auto logical_stock=inscription;
    for(char& c:inscription)c=c=='_'?' ':char(std::toupper(static_cast<unsigned char>(c)));
    for(const auto& n:names)if(logical_stock==n.id)inscription=e.format.starts_with("120")?n.medium:n.small;
    p.set("overscan_edge_text",Json(inscription));
    p.set("overscan_pair",Json(s.pair.active));
    p.set("auto_exposure",Json(!s.pair.active));
    p.set("date_imprint_active",Json(d.active));p.set("date_imprint_text",Json(d.text));
    p.set("date_imprint_text_b",Json(s.pair.active?d.text:std::string{}));
    p.set("date_imprint_style",Json(d.style));p.set("date_imprint_corner",Json(d.corner));
    p.set("date_imprint_ev",Json(d.exposure_ev));p.set("date_imprint_size",Json(d.size));
    // The upstream film-canvas executor explicitly rejects stripes. Do not
    // pretend the 128-row memory guarantee still applies when it is active.
    p.set("striped",Json(!e.active));
}
double silver_print_y_filter(const std::string& id) {
    // Grade 2 belongs to this variable-contrast PAPER, not to B&W films.
    // Namespaced imports retain the source logical ID and content suffix.
    const auto logical=id.starts_with("usr_")&&id.size()>29?id.substr(4,id.size()-29):id;
    // Only the new, explicitly versioned cine stock opts into unfiltered
    // printing. Preserve the historical pack of every existing material,
    // including unknown custom silver papers: no silent old-edit look change.
    return logical=="open_kodak_2302_5m_033"?0.0:68.0;
}
Json opening_params(const RenderSettings& settings, bool scan_film, bool mono, bool prophoto) {
    require(!settings.auto_negative || scan_film || mono || prophoto,
            "Automatic negative inversion requires ProPhoto working output");
    // Unexposed effects keep their engine defaults. The exposed settings
    // start at those same defaults, so opening an unedited image retains
    // the existing look, including the physical 35 mm grain/halation scale.
    Json params = Json::object();
    params.set("input_color_space", Json(std::string("ProPhoto RGB")));
    params.set("input_cctf_decoding", Json(false));
    params.set("output_color_space", Json(std::string(prophoto ? "ProPhoto RGB" : "sRGB")));
    params.set("output_cctf_encoding", Json(true));
    params.set("extended_dynamic_range", Json(false));
    // Enable the striped executor, but do not claim that it alone bounds all
    // Vulkan allocations. The live-tier limit below remains necessary.
    params.set("striped", Json(true));
    // `striped=true` with strip_rows=0 is a whole-plane plan, not a memory
    // bound. Use real bands while preserving whole-frame/spatial stages.
    params.set("strip_rows", Json(128.0));
    // Keep the interactive canvas at the engine's detail-preview tier.  A
    // full Sony frame needs several 400+ MB Vulkan planes and cannot be the
    // default UI render on common 4 GB Windows GPUs.
    params.set("preview_long_edge", Json(3400.0));
    params.set("film_stock", Json(settings.film_stock));
    // Desktop policy matches the macOS session: a slide is already a
    // positive and must bypass negative-to-paper printing. The generic
    // engine API still permits deliberate positive-to-paper experiments.
    params.set("scan_film", Json(scan_film));
    params.set("digital_intermediate", Json(settings.auto_negative && !scan_film && !mono));
    params.set("print_stock", Json(settings.print_stock));
    params.set("print_exposure", Json(settings.print_exposure));
    params.set("exposure_compensation_ev", Json(settings.film_exposure_ev));
    params.set("film_format_mm", Json(settings.pair.active?36+(settings.edge.active?1:settings.pair.spacing_mm):settings.film_format_mm));
    params.set("grain_active", Json(settings.grain_active));
    params.set("grain_sublayers_active", Json(settings.grain_active && !mono));
    // Preserve legacy silver packs; the new blue-sensitive 2302 version
    // explicitly uses an unfiltered print head, not an Ilford grade pack.
    if (mono) {
        params.set("c_filter_neutral", Json(0.0));
        params.set("m_filter_neutral", Json(0.0));
        params.set("y_filter_neutral", Json(silver_print_y_filter(settings.print_stock)));
    }
    params.set("grain_amount", Json(settings.grain_amount));
    params.set("halation_active", Json(settings.halation_active));
    params.set("halation_amount", Json(settings.halation_amount));
    params.set("glare_active", Json(settings.glare_active));
    params.set("glare_amount", Json(settings.glare_amount));
    params.set("y_filter_shift", Json(mono ? 0.0 : settings.y_filter_shift));
    params.set("m_filter_shift", Json(mono ? 0.0 : settings.m_filter_shift));
    placement_params(params,settings.placement);
    film_edge_params(params,settings);
    return params;
}

Json changed_params(const RenderSettings& before, const RenderSettings& after, bool scan_film, bool mono) {
    Json delta = Json::object();
    if(before.auto_negative!=after.auto_negative || before.film_stock!=after.film_stock)
        delta.set("digital_intermediate", Json(after.auto_negative && !scan_film && !mono));
    // Sending an unchanged film_stock still invalidates the negative in the
    // C API. A print-only edit must never include it incidentally.
    if (before.film_stock != after.film_stock) {
        delta.set("film_stock", Json(after.film_stock));
        // Send false too when returning to negative film, otherwise the
        // previous slide's direct-scan mode would survive the stock change.
        delta.set("scan_film", Json(scan_film));
    }
    if (before.film_stock != after.film_stock || before.grain_active != after.grain_active)
        delta.set("grain_sublayers_active", Json(after.grain_active && !mono));
    if (mono && (before.film_stock != after.film_stock || before.print_stock != after.print_stock)) {
        delta.set("c_filter_neutral", Json(0.0));
        delta.set("m_filter_neutral", Json(0.0));
        delta.set("y_filter_neutral", Json(silver_print_y_filter(after.print_stock)));
    }
    if (before.print_stock != after.print_stock) delta.set("print_stock", Json(after.print_stock));
    if (before.print_exposure != after.print_exposure) delta.set("print_exposure", Json(after.print_exposure));
    // The C API retains carried user controls when loading another stock.
    // Send only changed fields: an unchanged SHOOT field in a paper-only
    // delta would unnecessarily invalidate the cached grain realization.
    if (before.film_exposure_ev != after.film_exposure_ev) delta.set("exposure_compensation_ev", Json(after.film_exposure_ev));
    if (before.film_format_mm != after.film_format_mm) delta.set("film_format_mm", Json(after.film_format_mm));
    if (before.grain_active != after.grain_active) delta.set("grain_active", Json(after.grain_active));
    if (before.grain_amount != after.grain_amount) delta.set("grain_amount", Json(after.grain_amount));
    if (before.halation_active != after.halation_active) delta.set("halation_active", Json(after.halation_active));
    if (before.halation_amount != after.halation_amount) delta.set("halation_amount", Json(after.halation_amount));
    if (before.glare_active != after.glare_active) delta.set("glare_active", Json(after.glare_active));
    if (before.glare_amount != after.glare_amount) delta.set("glare_amount", Json(after.glare_amount));
    // A colour frame's colour-correction shifts must not turn into hidden
    // silver-paper contrast controls. Restore them on returning to colour.
    if (before.y_filter_shift != after.y_filter_shift || before.film_stock != after.film_stock)
        delta.set("y_filter_shift", Json(mono ? 0.0 : after.y_filter_shift));
    if (before.m_filter_shift != after.m_filter_shift || before.film_stock != after.film_stock)
        delta.set("m_filter_shift", Json(mono ? 0.0 : after.m_filter_shift));
    if(before.placement!=after.placement)placement_params(delta,after.placement);
    if(before.edge!=after.edge||before.date!=after.date||before.pair.active!=after.pair.active||((after.edge.active||after.date.active)&&before.film_stock!=after.film_stock))film_edge_params(delta,after);
    return delta;
}

SessionPtr open_session(spk_engine* engine, const io::DecodedRaw& decoded,
                        const RenderSettings& settings, bool scan_film, bool mono, bool prophoto) {
    const spk_image input{decoded.rgb.data(), decoded.width, decoded.height, 3};
    const auto params = opening_params(settings, scan_film, mono, prophoto).dump();
    // No reply is needed here; the host has no allocator or JSON lifetime to
    // share with the UI, and the C API validates the complete delta itself.
    SessionPtr session(spk_open(engine, &input, params.c_str(), nullptr), &spk_session_release);
    require(bool(session), "Open image: " + std::string(spk_last_error(engine)));
    return session;
}
void read_film_geometry(spk_session* session,Frame& frame) {
    if(!frame.settings.edge.active)return;
    char* raw=nullptr;
    require(spk_overscan_geometry(session,&raw)==SPK_OK&&raw,"Film geometry unavailable");
    std::unique_ptr<char,decltype(&spk_string_free)> owned(raw,spk_string_free);
    Json json;std::string error;
    require(Json::parse(raw,json,error)&&json.at("valid").as_bool(),"Invalid film geometry");
    const auto& gates=json.at("gates").items();
    require(gates.size()==(frame.settings.pair.active?2u:1u)&&gates[0].items().size()==8,"Unexpected picture gate count");
    for(size_t i=0;i<8;++i)frame.film_gate[i]=gates[0].items()[i].as_double();
    if(gates.size()==2)for(size_t i=0;i<8;++i)frame.film_gate_b[i]=gates[1].items()[i].as_double();
    frame.film_gate_valid=true;frame.film_vertical=json.at("vertical").as_bool();
}

std::shared_ptr<const io::DecodedRaw> compose_pair(spk_engine* engine,
    const std::array<std::shared_ptr<const io::DecodedRaw>,2>& sources,const RenderSettings& settings) {
    // PairComposer's layout: no upscaling, and no piece larger than the
    // longest source. Gap is zero scene radiance, not a painted black print.
    std::array<std::shared_ptr<const io::DecodedRaw>,2> framed;
    RenderSettings cutSettings;cutSettings.date.active=true;
    double across=1e30,longest=0;
    const double aspect=settings.pair.turned?24./18.:18./24.;
    for(size_t i=0;i<2;++i) {
        require(bool(sources[i]),"A pair requires both source decodes");
        framed[i]=prepare_input(sources[i],cutSettings,settings.pair.holes[i].geometry);
        InputGeometry turn;turn.turns=settings.pair.holes[i].turns;
        framed[i]=prepare_input(framed[i],cutSettings,turn);
        const double w=framed[i]->width,h=framed[i]->height;
        const double fitW=std::min(w,h*aspect),fitH=std::min(h,w/aspect);
        across=std::min(across,settings.pair.turned?fitW:fitH);
        longest=std::max({longest,w,h});
    }
    across=std::min(across,longest/(37./24.));
    const unsigned a=std::max(8u,unsigned(std::floor(across))),along=std::max(6u,unsigned(std::lround(a*18./24.)));
    const double spacing=settings.edge.active?1:settings.pair.spacing_mm;
    const unsigned advance=unsigned(std::lround(along*(18+spacing)/18));
    const unsigned hw=settings.pair.turned?a:along,hh=settings.pair.turned?along:a;
    auto result=std::make_shared<io::DecodedRaw>();
    result->width=settings.pair.turned?a:advance+along;result->height=settings.pair.turned?advance+along:a;
    result->metadata=sources[0]->metadata;result->metadata.model+=" + "+sources[1]->metadata.model;
    result->rgb.assign(size_t(result->width)*result->height*3,0);
    for(size_t side=0;side<2;++side) {
        const auto& raw=*framed[side];const auto& hole=settings.pair.holes[side];
        const double cw=std::min(double(raw.width),raw.height*aspect)/hole.scale;
        const double ch=std::min(double(raw.height),raw.width/aspect)/hole.scale;
        const double left=(raw.width-cw)*(hole.x+1)*.5,top=(raw.height-ch)*(hole.y+1)*.5;
        io::DecodedRaw picture;picture.width=hw;picture.height=hh;picture.rgb.resize(size_t(hw)*hh*3);
        for(unsigned y=0;y<hh;++y)for(unsigned x=0;x<hw;++x) {
            const double sx=std::clamp(left+(x+.5)*cw/hw-.5,0.,double(raw.width-1));
            const double sy=std::clamp(top+(y+.5)*ch/hh-.5,0.,double(raw.height-1));
            const unsigned x0=unsigned(sx),y0=unsigned(sy),x1=std::min(x0+1,raw.width-1),y1=std::min(y0+1,raw.height-1);
            const double fx=sx-x0,fy=sy-y0;
            for(unsigned c=0;c<3;++c) {
                const auto at=[&](unsigned xx,unsigned yy){return raw.rgb[(size_t(yy)*raw.width+xx)*3+c];};
                picture.rgb[(size_t(y)*hw+x)*3+c]=float((at(x0,y0)*(1-fx)+at(x1,y0)*fx)*(1-fy)+(at(x0,y1)*(1-fx)+at(x1,y1)*fx)*fy);
            }
        }
        auto meterSettings=settings;meterSettings.pair={};meterSettings.edge.active=false;meterSettings.date.active=false;
        meterSettings.placement={};meterSettings.film_exposure_ev=0;
        auto meter=open_session(engine,picture,meterSettings,false,false,false);
        char* rawReply=nullptr;const auto status=spk_solve(meter.get(),"exposure",&rawReply);
        std::unique_ptr<char,decltype(&spk_string_free)> reply(rawReply,spk_string_free);
        require(status==SPK_OK&&rawReply,"Pair meter: "+std::string(spk_last_error(engine)));
        Json json;std::string error;require(Json::parse(rawReply,json,error),"Invalid pair meter reply");
        const auto& ev=json.at("exposure_ev_by_method").at("balanced");
        require(ev.is_number()&&std::isfinite(ev.as_double()),"Pair meter did not return an exposure");
        const double gain=std::exp2(ev.as_double()+hole.exposure_ev);
        const unsigned ox=settings.pair.turned?0:unsigned(side)*advance,oy=settings.pair.turned?unsigned(side)*advance:0;
        for(unsigned y=0;y<hh;++y)for(unsigned x=0;x<hw;++x)for(unsigned c=0;c<3;++c)
            result->rgb[(size_t(y+oy)*result->width+x+ox)*3+c]=float(picture.rgb[(size_t(y)*hw+x)*3+c]*gain);
    }
    return result;
}
}  // namespace

Frame::~Frame() { spk_result_free(&result_); }

std::vector<std::uint8_t> rgba16_to_bgra8(const spk_result& result) {
    require(result.rgba16 && result.width && result.height && result.row_stride_px >= result.width,
            "Invalid RGBA16 display dimensions or row pitch");
    constexpr auto max_size = std::numeric_limits<std::size_t>::max();
    // Both the source address calculation and destination allocation must fit;
    // checking pixels alone would miss overflow of the row padding.
    require(std::size_t(result.height) <= max_size / result.row_stride_px / 4 / sizeof(std::uint16_t) &&
                std::size_t(result.height) <= max_size / result.width / 4,
            "RGBA16 display dimensions overflow address space");
    const std::size_t count = std::size_t(result.width) * result.height * 4;
    require(count <= std::vector<std::uint8_t>().max_size(), "BGRA8 display allocation is too large");
    std::vector<std::uint8_t> bgra(count);
    const auto byte = [](std::uint16_t value) {
        return std::uint8_t((std::uint32_t(value) + 128u) / 257u);
    };
    for (std::size_t y = 0; y < result.height; ++y) {
        const auto* src = result.rgba16 + y * result.row_stride_px * 4;
        auto* dst = bgra.data() + y * result.width * 4;
        for (std::size_t x = 0; x < result.width; ++x) {
            if (src[4 * x + 3] != 65535)
                throw std::runtime_error("Display requires opaque RGBA16 pixels");
            dst[4 * x] = byte(src[4 * x + 2]);
            dst[4 * x + 1] = byte(src[4 * x + 1]);
            dst[4 * x + 2] = byte(src[4 * x]);
            dst[4 * x + 3] = 255;
        }
    }
    return bgra;
}

struct PreviewHost::Impl {
    fs::path resources;
    Catalog catalog;
    double engine_create_ms = 0.0;
    EnginePtr engine{nullptr, &spk_engine_destroy};
    // Declaration order makes session release precede engine destruction.
    SessionPtr session{nullptr, &spk_session_release};
    // Retained only for reopening after a failed settings/render operation.
    // Ordinary parameter edits reuse the current GPU source and negative.
    std::shared_ptr<const io::DecodedRaw> decoded;
    FramePtr frame;
    std::shared_ptr<const spkhost::OutputTransform> display_transform;
};

PreviewHost::PreviewHost(const fs::path& resources, bool prophoto) : impl_(std::make_unique<Impl>()) {
    impl_->resources = fs::absolute(resources);
    const auto path = utf8(impl_->resources);
    require(fs::is_directory(impl_->resources), "Engine resources directory is missing");
    impl_->catalog = read_catalog(impl_->resources);
    const auto started = Clock::now();
    impl_->engine.reset(spk_engine_create(path.c_str(), nullptr));
    impl_->engine_create_ms = elapsed(started);
    require(bool(impl_->engine), "Engine creation failed: " + std::string(spk_last_error(nullptr)));
    if(prophoto) {
        auto transform=std::make_shared<spkhost::OutputTransform>();std::string error;
        require(transform->fetch(impl_->engine.get(),"ProPhoto RGB","sRGB",error),error);
        impl_->display_transform=std::move(transform);
    }
}

PreviewHost::~PreviewHost() = default;
const Catalog& PreviewHost::catalog() const noexcept { return impl_->catalog; }
void PreviewHost::reload_catalog() { auto next=read_catalog(impl_->resources);impl_->catalog=std::move(next); }
double PreviewHost::engine_create_ms() const noexcept { return impl_->engine_create_ms; }
FramePtr PreviewHost::current() const noexcept { return impl_->frame; }
std::string PreviewHost::resolved_params_json() const {
    require(bool(impl_->session),"No live interactive session");char* raw=nullptr;
    const auto status=spk_get_params(impl_->session.get(),&raw);
    std::unique_ptr<char,decltype(&spk_string_free)> held(raw,&spk_string_free);
    require(status==SPK_OK&&raw,"Cannot read resolved session parameters");return raw;
}

FramePtr PreviewHost::open_raw(const fs::path& source, DecodeMode mode, const RenderSettings& settings,const InputGeometry& geometry) {
    const auto started = Clock::now();
    const bool scan_film = validate_settings(settings, impl_->catalog);
    require(!settings.pair.active,"A paired project must open both sources");
    require(mode == DecodeMode::compatible16 || mode == DecodeMode::headroom, "Unknown RAW decode mode");
    const auto path = fs::absolute(source);
    require(fs::is_regular_file(path), "Photo input is missing: " + utf8(path));
    auto next = std::shared_ptr<Frame>(new Frame());
    next->source = path;
    next->decode_mode = mode;
    next->settings = settings;
    next->scan_film = scan_film;
    auto decoded = std::make_shared<io::DecodedRaw>();
    std::string error;
    auto t = Clock::now();
    const bool decoded_ok = decode_photo(path,mode,*decoded,error);
    require(decoded_ok, "Photo decode: " + error);
    next->timings.decode_ms = elapsed(t);
    next->metadata = decoded->metadata;
    next->decode_timings = decoded->timings;
    next->decoded_ = decoded;
    next->input_geometry_=geometry;
    next->prepared_=prepare_input(decoded,settings,geometry);
    next->native_=std::max(next->prepared_->width,next->prepared_->height)<=3400;
    next->display_transform_ = impl_->display_transform;
    t = Clock::now();
    auto session = open_session(impl_->engine.get(), *next->prepared_, settings, scan_film, monochrome(settings, impl_->catalog), next->prophoto());
    next->timings.open_ms = elapsed(t);
    t = Clock::now();
    const auto status = spk_render(session.get(), "live", &next->result_);
    require(status == SPK_OK, "Render: " + std::string(spk_last_error(impl_->engine.get())));
    next->auto_negative_active=next->settings.auto_negative && !next->scan_film && !monochrome(next->settings,impl_->catalog);
    read_film_geometry(session.get(),*next);
    next->timings.render_ms = elapsed(t);
    next->timings.engine_render_ms = next->result_.elapsed_ms;
    t = Clock::now();
    if(!next->prophoto())next->bgra8 = rgba16_to_bgra8(next->result_);
    next->timings.display_ms = elapsed(t);
    // Commit only after all allocations and GPU/CPU work succeeded.
    impl_->session = std::move(session);
    impl_->decoded = std::move(decoded);
    impl_->frame = next;
    next->timings.total_ms = elapsed(started);
    return next;
}

FramePtr PreviewHost::open_pair(const fs::path& identity,const std::array<fs::path,2>& paths,
    const std::array<DecodeMode,2>& modes,const RenderSettings& requested) {
    const auto started=Clock::now();auto settings=requested;settings.pair.active=true;
    settings.edge.format="135_half";
    const bool scan=validate_settings(settings,impl_->catalog);
    auto next=std::shared_ptr<Frame>(new Frame());next->source=fs::absolute(identity);next->settings=settings;
    next->decode_mode=modes[0];next->scan_film=scan;next->display_transform_=impl_->display_transform;
    std::string error;auto t=Clock::now();
    for(size_t i=0;i<2;++i) {
        require(modes[i]==DecodeMode::compatible16||modes[i]==DecodeMode::headroom,"Unknown pair RAW decode mode");
        auto raw=std::make_shared<io::DecodedRaw>();
        const bool ok=decode_photo(paths[i],modes[i],*raw,error);
        require(ok,"Pair source "+std::to_string(i+1)+": "+error);next->pair_sources_[i]=std::move(raw);
    }
    next->timings.decode_ms=elapsed(t);
    impl_->session.reset(); // Never retain two whole-frame GPU sources alongside the pair's meters.
    next->decoded_=compose_pair(impl_->engine.get(),next->pair_sources_,settings);
    next->prepared_=prepare_input(next->decoded_,settings,{});
    next->metadata=next->decoded_->metadata;
    next->native_=std::max(next->prepared_->width,next->prepared_->height)<=3400;
    t=Clock::now();auto session=open_session(impl_->engine.get(),*next->prepared_,settings,scan,monochrome(settings,impl_->catalog),next->prophoto());
    next->timings.open_ms=elapsed(t);t=Clock::now();
    const auto status=spk_render(session.get(),"live",&next->result_);
    require(status==SPK_OK,"Pair render: "+std::string(spk_last_error(impl_->engine.get())));
    next->auto_negative_active=next->settings.auto_negative && !next->scan_film && !monochrome(next->settings,impl_->catalog);
    read_film_geometry(session.get(),*next);next->timings.render_ms=elapsed(t);next->timings.engine_render_ms=next->result_.elapsed_ms;
    if(!next->prophoto())next->bgra8=rgba16_to_bgra8(next->result_);
    impl_->session=std::move(session);impl_->decoded=next->decoded_;impl_->frame=next;
    next->timings.total_ms=elapsed(started);return next;
}

FramePtr PreviewHost::render(const RenderSettings& settings,const InputGeometry& geometry) {
    const auto started = Clock::now();
    require(bool(impl_->frame), "Open a RAW image before rendering");
    const bool scan_film = validate_settings(settings, impl_->catalog);
    require(!settings.auto_negative || scan_film || monochrome(settings,impl_->catalog) || impl_->display_transform,
            "Automatic negative inversion requires ProPhoto working output");
    auto next = std::shared_ptr<Frame>(new Frame());
    next->source = impl_->frame->source;
    next->decode_mode = impl_->frame->decode_mode;
    next->settings = settings;
    next->scan_film = scan_film;
    next->metadata = impl_->frame->metadata;
    next->decode_timings = impl_->frame->decode_timings;
    next->decoded_ = impl_->decoded;
    next->pair_sources_=impl_->frame->pair_sources_;
    const bool pair_changed=settings.pair!=impl_->frame->settings.pair||(settings.pair.active&&settings.edge.active!=impl_->frame->settings.edge.active);
    require(settings.pair.active==impl_->frame->settings.pair.active,"Cannot change a frame's source type through render settings");
    if(pair_changed) {
        impl_->session.reset();
        next->decoded_=compose_pair(impl_->engine.get(),next->pair_sources_,settings);
    }
    next->input_geometry_=geometry;
    const auto& old=*impl_->frame;
    const bool new_input=pair_changed||((settings.edge.active||settings.date.active||old.input_cut())&&geometry!=old.input_geometry_)||settings.edge.active!=old.settings.edge.active||
        settings.date.active!=old.settings.date.active||(settings.edge.active&&settings.edge.format!=old.settings.edge.format);
    next->prepared_=new_input?prepare_input(next->decoded_,settings,geometry):old.prepared_;
    next->native_=std::max(next->prepared_->width,next->prepared_->height)<=3400;
    next->display_transform_ = impl_->display_transform;
    const auto delta = changed_params(impl_->frame->settings, settings, scan_film, monochrome(settings, impl_->catalog));
    try {
        auto t = Clock::now();
        if (!impl_->session||new_input) {
            // C API settings changes may fail after applying part of their
            // state. Never reuse such a session; rebuild from the retained
            // decoded input on the next request, without decoding the file.
            impl_->session.reset();
            impl_->session = open_session(impl_->engine.get(), *next->prepared_, settings, scan_film, monochrome(settings, impl_->catalog), next->prophoto());
            next->timings.open_ms = elapsed(t);
        } else if (!delta.fields().empty()) {
            const auto text = delta.dump();
            const auto status = spk_set_params(impl_->session.get(), text.c_str(), nullptr);
            require(status == SPK_OK, "Apply settings: " + std::string(spk_last_error(impl_->engine.get())));
            next->timings.params_ms = elapsed(t);
        }
        t = Clock::now();
        const auto status = spk_reprint(impl_->session.get(), "live", &next->result_);
        require(status == SPK_OK, "Render: " + std::string(spk_last_error(impl_->engine.get())));
        next->auto_negative_active=next->settings.auto_negative && !next->scan_film && !monochrome(next->settings,impl_->catalog);
        read_film_geometry(impl_->session.get(),*next);
        next->timings.render_ms = elapsed(t);
        next->timings.engine_render_ms = next->result_.elapsed_ms;
        t = Clock::now();
        if(!next->prophoto())next->bgra8 = rgba16_to_bgra8(next->result_);
        next->timings.display_ms = elapsed(t);
    } catch (...) {
        impl_->session.reset();
        throw;
    }
    impl_->frame = next;
    impl_->decoded=next->decoded_;
    next->timings.total_ms = elapsed(started);
    return next;
}

FramePtr PreviewHost::render_original(const FramePtr& snapshot, bool negative_scan) {
    require(snapshot && snapshot->decoded_, "Original export requires a decoded frame snapshot");
    if(negative_scan) {
        require(!validate_settings(snapshot->settings,impl_->catalog), "Negative scan export requires a negative film");
        require(snapshot->prophoto(), "Negative scan export requires ProPhoto working pixels");
    }
    if(snapshot->native_ && !negative_scan)
        return snapshot;
    const auto started=Clock::now();
    const bool scan_film=negative_scan || validate_settings(snapshot->settings,impl_->catalog);
    auto next=std::shared_ptr<Frame>(new Frame());
    next->source=snapshot->source;next->decode_mode=snapshot->decode_mode;
    next->settings=snapshot->settings;next->scan_film=scan_film;
    next->metadata=snapshot->metadata;next->decode_timings=snapshot->decode_timings;
    next->decoded_=snapshot->decoded_;
    next->pair_sources_=snapshot->pair_sources_;
    next->prepared_=snapshot->prepared_;next->input_geometry_=snapshot->input_geometry_;next->native_=true;
    next->display_transform_=snapshot->display_transform_;
    // Keep CPU snapshots alive, but never keep both preview and original
    // negatives/sources resident on a small GPU. A later edit reconstructs
    // the preview session from impl_->decoded, not from the export's source.
    impl_->session.reset();
    auto t=Clock::now();
    auto original=open_session(impl_->engine.get(),*next->prepared_,next->settings,
                               scan_film,monochrome(next->settings,impl_->catalog),next->prophoto());
    next->timings.open_ms=elapsed(t);t=Clock::now();
    const auto status=spk_render(original.get(),"full",&next->result_);
    require(status==SPK_OK,"Original render: "+std::string(spk_last_error(impl_->engine.get())));
    next->auto_negative_active=next->settings.auto_negative && !next->scan_film && !monochrome(next->settings,impl_->catalog);
    read_film_geometry(original.get(),*next);
    require(next->settings.edge.active||(next->width()==next->prepared_->width && next->height()==next->prepared_->height),
            "Original render returned preview dimensions");
    next->timings.render_ms=elapsed(t);next->timings.engine_render_ms=next->result_.elapsed_ms;
    next->timings.total_ms=elapsed(started);
    return next;
}

void PreviewHost::export_tiff(const Frame& frame, const fs::path& destination) const {
    require(!frame.prophoto(), "Working-space frames require the colour-managed output path");
    const auto& pixels = frame.pixels();
    std::string error;
    const bool written = io::write_srgb_tiff(destination, pixels.rgba16, pixels.width, pixels.height,
                                            pixels.row_stride_px, impl_->resources / "io" / "sRGB.icc", error);
    require(written, "TIFF export: " + error);
}

std::string PreviewHost::scene_latitude(const FramePtr& snapshot,const std::string& request) {
    require(snapshot&&snapshot->decoded_,"Scene measurement requires a decoded frame");
    const auto& s=snapshot->settings;
    const bool scan=validate_settings(s,impl_->catalog);
    // Keep only one image session resident on a small GPU. The interactive
    // session is reconstructed lazily, just as after an original-size export.
    impl_->session.reset();
    auto probe=open_session(impl_->engine.get(),*snapshot->prepared_,s,scan,monochrome(s,impl_->catalog),snapshot->prophoto());
    char* raw=nullptr;
    const auto status=spk_scene_latitude(probe.get(),request.c_str(),&raw);
    std::unique_ptr<char,decltype(&spk_string_free)> owned(raw,spk_string_free);
    require(status==SPK_OK&&raw,"Scene measurement: "+std::string(spk_last_error(impl_->engine.get())));
    return std::string(raw);
}

}  // namespace spk::desktop
