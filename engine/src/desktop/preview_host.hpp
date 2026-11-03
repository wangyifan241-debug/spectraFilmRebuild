// Windows desktop host. Rendering and image I/O stay behind their existing
// boundaries; the UI only receives immutable, independently owned frames.
#pragma once

#include "io/raw_decoder.hpp"
#include "spektrafilm/spk_engine.h"
#include "output.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace spk::desktop {

enum class DecodeMode { compatible16, headroom };

struct ScenePlacement {
    bool active=false;
    std::string norm="power";
    double highlight_knee=2, highlight_room=0, shadow_knee=-2, shadow_room=0;
    double rolloff=2, max_lift=4;
    // UI pull-backs are saved separately from the resolved render curve.
    double highlight_pull_back=0, shadow_pull_back=0;
    bool operator==(const ScenePlacement&) const = default;
};

struct FilmEdge {
    bool active=false;
    std::string format="135",mode="strip",gate="auto",holes="white",carrier="black";
    int camera_seed=1,frame_seed=1,frame_number=0;
    double fog=1,leaks=0;
    bool operator==(const FilmEdge&) const = default;
};
struct DateBack {
    bool active=false;
    std::string text,style="lcd",corner="br";
    double exposure_ev=3.5,size=1;
    bool operator==(const DateBack&) const = default;
};
// Same source-space geometry as the editor, without a Qt dependency. Applied
// to scene-linear pixels BEFORE film only when an edge/date exposes the frame.
struct InputGeometry {
    double x=0,y=0,w=1,h=1,straighten=0;
    int turns=0;
    bool flip_h=false,flip_v=false;
    bool operator==(const InputGeometry&) const = default;
};
struct PairHole {
    InputGeometry geometry;
    double scale=1,x=0,y=0,exposure_ev=0;
    int turns=0;
    bool operator==(const PairHole&) const = default;
};
struct HalfPair {
    bool active=false,turned=false;
    double spacing_mm=1;
    std::array<PairHole,2> holes;
    bool operator==(const HalfPair&) const = default;
};

struct RenderSettings {
    std::string film_stock = "kodak_portra_400";
    std::string print_stock = "kodak_portra_endura";
    // Desktop edit identity only, not new C API fields or physical controls.
    std::string film_profile_sha256,print_profile_sha256;
    std::string engine_resources_sha256;
    double print_exposure = 1.0;
    // Film exposure places the scene on the characteristic curve. The
    // enlarger compensates its mid-grey; print_exposure controls brightness.
    double film_exposure_ev = 0.0;
    // Preserve the engine's existing 35 mm default, including grain scale.
    double film_format_mm = 35.0;
    bool grain_active = true;
    double grain_amount = 1.0;
    bool halation_active = true;
    double halation_amount = 1.0;
    bool glare_active = true;
    bool auto_negative = false;
    double glare_amount = 1.0;
    double y_filter_shift = 0.0;
    double m_filter_shift = 0.0;
    ScenePlacement placement;
    FilmEdge edge;
    DateBack date;
    HalfPair pair;

    bool operator==(const RenderSettings&) const = default;
};

struct Stock {
    std::string id, label;
    bool is_positive = false;
    bool is_monochrome = false;
    std::string profile_sha256;
};
struct Catalog { std::vector<Stock> films, papers;std::string engine_resources_sha256; };

// Wall-clock calls, including CPU result readback in render_ms. total_ms is
// the complete host call, not a sum of nested measurements. Engine creation
// and TIFF export are separate operations. Re-renders have zero decode/open
// time unless a previous engine failure required reopening the retained input.
struct Timings {
    double decode_ms = 0.0, open_ms = 0.0, params_ms = 0.0;
    double render_ms = 0.0, engine_render_ms = 0.0;
    double display_ms = 0.0, total_ms = 0.0;
};

class PreviewHost;
class Frame final {
public:
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    std::filesystem::path source;
    DecodeMode decode_mode = DecodeMode::compatible16;
    RenderSettings settings;
    // Resolved from the selected film's catalog metadata, not a separate
    // user setting. Slide film is viewed directly without the paper stage.
    bool scan_film = false;
    bool auto_negative_active = false; // pixels() is Cineon; output must decode before grading
    io::RawMetadata metadata;
    io::RawDecodeTimings decode_timings;
    Timings timings;
    // Normalised picture quad on the film canvas, from the engine that drew it.
    std::array<double,8> film_gate{};
    std::array<double,8> film_gate_b{};
    bool film_gate_valid=false,film_vertical=false;
    // Top-down tightly packed BGRA8 encoded sRGB. Only the display copy is
    // reduced to 8 bits; pixels() retains the rendered tier's RGBA16.
    // Export-only originals and ProPhoto working-space frames do not allocate
    // this copy; the latter must be graded/converted by FrameOutput first.
    std::vector<std::uint8_t> bgra8;

    const spk_result& pixels() const noexcept { return result_; }
    std::uint32_t width() const noexcept { return result_.width; }
    std::uint32_t height() const noexcept { return result_.height; }
    std::uint32_t source_width() const noexcept { return decoded_ ? decoded_->width : width(); }
    std::uint32_t source_height() const noexcept { return decoded_ ? decoded_->height : height(); }
    bool reprint() const noexcept { return result_.reprint != 0; }
    bool negative_was_cached() const noexcept { return result_.negative_was_cached != 0; }
    bool prophoto() const noexcept { return bool(display_transform_); }
    const spkhost::OutputTransform* display_transform() const noexcept { return display_transform_.get(); }
    // Scene-linear, orientation-applied decode retained by this snapshot.
    const io::DecodedRaw& original() const { return *decoded_; }
    bool input_cut() const noexcept { return settings.edge.active||settings.date.active; }
    const InputGeometry& input_geometry() const noexcept { return input_geometry_; }
    const io::DecodedRaw& prepared() const { return prepared_?*prepared_:*decoded_; }

private:
    Frame() = default;
    spk_result result_{};
    std::vector<uint16_t> phoenix_preview_pixels_;
    // Retain the exact decode behind the displayed frame. Original exports
    // never re-read a changed file or accidentally use a newer photo/decode.
    std::shared_ptr<const io::DecodedRaw> decoded_;
    std::shared_ptr<const io::DecodedRaw> prepared_;
    std::array<std::shared_ptr<const io::DecodedRaw>,2> pair_sources_;
    InputGeometry input_geometry_;
    bool native_=false;
    std::shared_ptr<const spkhost::OutputTransform> display_transform_;
    friend class PreviewHost;
};

using FramePtr = std::shared_ptr<const Frame>;

// Pure conversion, including row-pitch handling, used by the host and tested
// independently. Throws on invalid dimensions/pitch or non-opaque pixels.
// Rounding is (value + 128) / 257; no colour transform or tone mapping.
std::vector<std::uint8_t> rgba16_to_bgra8(const spk_result& result);

// Construct, call and destroy on one serial worker thread. Public operations
// throw std::runtime_error on failure. The UI may concurrently read shared
// const Frames, which stay valid after this host and its engine are destroyed.
class PreviewHost final {
public:
    explicit PreviewHost(const std::filesystem::path& resources, bool prophoto = false);
    ~PreviewHost();
    PreviewHost(const PreviewHost&) = delete;
    PreviewHost& operator=(const PreviewHost&) = delete;

    const Catalog& catalog() const noexcept;
    // Atomic directory rescan for verified, immutable user material additions.
    // Existing sessions/Frames keep their own already-resolved material state.
    void reload_catalog();
    double engine_create_ms() const noexcept;
    FramePtr current() const noexcept;
    // Read-only diagnostics of the live engine session, not a second params
    // implementation. Throws when no interactive session is alive.
    std::string resolved_params_json() const;

    // New-file open is transactional: decode/open/render/display conversion
    // must all succeed before replacing the previous session and frame.
    // Historical C++ name retained for callers: accepts RAW and ICC-aware
    // JPEG/PNG. Raster inputs use compatible16; headroom remains RAW-only.
    FramePtr open_raw(const std::filesystem::path& source, DecodeMode mode,
                      const RenderSettings& settings = {}, const InputGeometry& geometry = {});
    FramePtr open_pair(const std::filesystem::path& identity,
                       const std::array<std::filesystem::path,2>& sources,
                       const std::array<DecodeMode,2>& modes,const RenderSettings& settings);
    // Only changed settings are sent. Print-side edits request spk_reprint;
    // the result's actual cache flag determines whether reuse occurred.
    // A failure never mutates the previously returned Frame.
    FramePtr render(const RenderSettings& settings, const InputGeometry& geometry = {});
    // Re-render this immutable snapshot at decoded native size, not an
    // upscaled preview. Does not replace current(); releases the interactive
    // GPU session first and rebuilds it lazily on the next edit. Small frames
    // already at native size are returned unchanged (including their grain).
    FramePtr render_original(const FramePtr& snapshot, bool negative_scan = false);
    // Measure/fit the supplied immutable decode, never a possibly newer live
    // session or a reread of its path. No render setting is committed here.
    // Read-only Process probe on the immutable shown frame.
    std::string solve(const FramePtr& snapshot);
    std::string scene_latitude(const FramePtr& snapshot, const std::string& request="{}");
    // Exports exactly the supplied sRGB frame, even after a newer one opened.
    // ProPhoto frames are refused here: use the colour-managed FrameOutput.
    // Existing files are never overwritten. Returns only after the writer
    // has completed and closed the file; errors retain its cleanup details.
    void export_tiff(const Frame& frame, const std::filesystem::path& destination) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace spk::desktop
