#pragma once

#include "desktop/preview_host.hpp"
#include <QImage>
#include <QRectF>
#include <filesystem>
#include <array>

// Geometry belongs after the engine. Every output surface uses these same
// pixel edges; cropping never changes the emulsion or re-renders its grain.
namespace spk::desktop::output {

enum class Format { TIFF16, PNG8, PNG16, JPEG8 };
// Scan-side adjustments, independent of measured film/paper. Frame sources
// carry their actual working space; the contrast pivot follows its encoding.
struct CurvePoint { double x=0,y=0; bool operator==(const CurvePoint&) const = default; };
using Curve = std::vector<CurvePoint>;
struct ColorZone {
    double hue=0,saturation=0,luminance=0;
    bool operator==(const ColorZone&) const = default;
};
struct Develop {
    bool enabled = true;
    bool legacySrgb = false; // preserve v2 edits, never silently reinterpret
    bool legacyExposure = false; // v1-v8 recipes used gamma 2.2 in either space
    double temperature=0, tint=0, exposure=0, contrast=0, brightness=0;
    double saturation=0, highlights=0, shadows=0, blackPoint=0, whitePoint=0;
    // rgb, luma, red, green, blue; Fritsch–Carlson, 256 samples (Mac).
    std::array<Curve,5> curves{{{{0,0},{1,1}},{{0,0},{1,1}},{{0,0},{1,1}},{{0,0},{1,1}},{{0,0},{1,1}}}};
    // master, shadows, midtones, highlights
    std::array<ColorZone,4> zones{};
    double vignetteAmount=0,vignetteMidpoint=50;
    bool operator==(const Develop&) const = default;
    bool neutral() const { auto copy=*this;copy.enabled=true;copy.legacySrgb=false;copy.legacyExposure=false;return !enabled||copy==Develop{}; }
};
void validateDevelop(const Develop& develop);
QImage applyDevelop(const QImage& rgba64, const Develop& develop);
double evaluateCurve(const Curve& curve, double x);
struct ExportOptions {
    Format format = Format::TIFF16;
    int jpegQuality = 95;
    int maxLongEdge = 0; // zero preserves the cropped size; never upscale
    Develop develop;
    bool negativeScan = false; // TIFF16, ProPhoto; bypass print-side grading
};

struct Geometry {
    // Crop coordinates always refer to the original, unturned source.
    QRectF crop{0,0,1,1};
    int quarterTurns = 0; // clockwise, 0 through 3
    bool flipHorizontal = false; // in the final output axes
    bool flipVertical = false;
    double straightenDegrees = 0; // clockwise, -45 through +45
    bool operator==(const Geometry&) const = default;
};

struct Histogram {
    // Encoded display RGB and Rec.709 luma, 256 bins per channel. This is a
    // display readout, not the scene-linear exposure/latitude instrument.
    std::array<std::array<quint64,256>,4> bins{};
    quint64 samples = 0;
    int stride = 1;
    bool operator==(const Histogram&) const = default;
};
// Input must be the final visible image (after crop/turn/straighten). Sample
// deterministically on a grid, as the Mac shader does, bounding readout work.
Histogram displayHistogram(const QImage& visible);

// Normalized top-left crop in [0,1]. All fields must be finite, its area
// positive, and it must fit inside the full frame. Both edges round to the
// nearest pixel, with a minimum extent of one pixel for a valid small crop.
// Invalid input throws std::runtime_error; an empty rectangle is not "full".
QRect cropPixels(QSize fullSize, QRectF normalizedCrop);
void validateGeometry(QSize fullSize, const Geometry& geometry);
QImage displayImage(const QImage& full8, QRectF normalizedCrop);
QImage displayImage(const QImage& full8, const Geometry& geometry);
// Use this for transformed rendered previews: geometry is applied to the
// original RGBA16 before quantization, as it is for an 8-bit exported file.
QImage displayFrame(const FramePtr& frame, const Geometry& geometry, const Develop& develop = {});
QImage originalFrame(const FramePtr& frame, const Geometry& geometry, bool sourceOnly=false);
// Immutable working-space comparison pixels allow geometry changes to reuse
// the RAW resample/CCTF work. The destination transform stays identical.
QImage originalWorkingFrame(const FramePtr& frame,bool sourceOnly=false);
QImage displayWorkingImage(const QImage& working,const FramePtr& frame,const Geometry& geometry);

// The source is opaque encoded working-space RGBA64. Processing order is
// original-space crop -> bilinear straighten with zoom-to-fill (fixed crop
// dimensions) -> clockwise quarter turns -> output-axis flips -> optional
// downsize -> output depth. Identity and orthogonal geometry are lossless;
// straightening interpolates 16-bit samples without a colour transform.
QImage exportImage(const QImage& fullRgba64, QRectF normalizedCrop,
                   const ExportOptions& options);
QImage exportImage(const QImage& fullRgba64, const Geometry& geometry,
                   const ExportOptions& options);
bool formatAvailable(Format format);

// File inputs must already be sRGB (or the legacy untagged-sRGB contract),
// except negativeScan TIFFs, which require tagged ProPhoto RGB pixels.
// Frame overloads grade/resample ProPhoto and convert using the frame's owned
// engine transform before writing. Uses resources/io/sRGB.icc. Writes a file
// in a private temporary directory beside the destination, then publishes it
// atomically with no replacement, including if another writer wins a race.
// The caller's frame/image and any existing destination stay untouched.
// The final suffix must match the format (case insensitive); a missing suffix
// is rejected rather than creating an ambiguously typed image file.
void exportImageFile(const QImage& fullRgba64,
                     const std::filesystem::path& destination,
                     QRectF normalizedCrop, const ExportOptions& options,
                     const std::filesystem::path& resources);
void exportImageFile(const QImage& fullRgba64,
                     const std::filesystem::path& destination,
                     const Geometry& geometry, const ExportOptions& options,
                     const std::filesystem::path& resources);
void exportFrame(const FramePtr& frame, const std::filesystem::path& destination,
                 QRectF normalizedCrop, const ExportOptions& options,
                 const std::filesystem::path& resources);
void exportFrame(const FramePtr& frame, const std::filesystem::path& destination,
                 const Geometry& geometry, const ExportOptions& options,
                 const std::filesystem::path& resources);

} // namespace spk::desktop::output
