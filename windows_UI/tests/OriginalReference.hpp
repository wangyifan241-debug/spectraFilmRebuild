#pragma once
// Independent host-side native render for controller export tests. The test
// must compare native export with native pixels, not a 3400-pixel preview.
#include "FrameOutput.hpp"
#include <stdexcept>

inline QImage originalReference(const std::filesystem::path& resources,
                               const spk::desktop::FramePtr& snapshot,
                               const spk::desktop::output::Geometry& geometry) {
    if(!snapshot || snapshot->settings.grain_active || snapshot->settings.glare_active)
        throw std::runtime_error("Pixel reference requires deterministic grain/glare-off input");
    spk::desktop::PreviewHost reference(resources);
    return spk::desktop::output::displayFrame(reference.render_original(snapshot),geometry);
}
