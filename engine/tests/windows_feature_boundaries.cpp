// Exercise Windows feature refusals through the exported DLL boundary.
#include "spektrafilm/spk_engine.h"

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string params(spk_session* session) {
    char* json = nullptr;
    require(spk_get_params(session, &json) == SPK_OK && json, "get_params failed");
    std::string result(json);
    spk_string_free(json);
    return result;
}

void render(spk_engine* engine, spk_session* session) {
    spk_result result{};
    const spk_status status = spk_render(session, "full", &result);
    const bool valid = status == SPK_OK && result.rgba16 && result.texture &&
                       result.width == 32 && result.height == 16;
    const std::string error = spk_last_error(engine);
    spk_result_free(&result);
    require(valid, "default render failed after a refused request: " + error);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: spk_windows_feature_boundaries resources_dir\n"; return 2; }
    try {
        std::unique_ptr<spk_engine, decltype(&spk_engine_destroy)> engine(
            spk_engine_create(argv[1], nullptr), spk_engine_destroy);
        require(bool(engine), std::string("engine creation failed: ") + spk_last_error(nullptr));
        const char* capabilities = spk_capabilities(engine.get());
        require(capabilities && std::string(capabilities).find("unsupported_features") != std::string::npos,
                "Windows capabilities must disclose unported effects");

        std::vector<float> pixels(32 * 16 * 3);
        for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = 0.025f + float(i % 31) * 0.025f;
        const spk_image input{pixels.data(), 32, 16, 3};
        std::unique_ptr<spk_session, decltype(&spk_session_release)> session(
            spk_open(engine.get(), &input, nullptr, nullptr), spk_session_release);
        require(bool(session), std::string("default open failed: ") + spk_last_error(engine.get()));
        render(engine.get(), session.get());
        require(spk_set_params(session.get(),"{\"grain_active\":false,\"glare_active\":false}",nullptr)==SPK_OK,"Deterministic scene setup failed");
        const auto readPixels=[&] {
            spk_result r{};const auto status=spk_render(session.get(),"full",&r);
            require(status==SPK_OK&&r.rgba16,"Scene latitude render failed: "+std::string(spk_last_error(engine.get())));
            std::vector<uint16_t> values;for(unsigned y=0;y<r.height;++y)
                values.insert(values.end(),r.rgba16+y*r.row_stride_px*4,r.rgba16+y*r.row_stride_px*4+r.width*4);
            spk_result_free(&r);return values;
        };
        const auto neutral=readPixels();
        require(spk_set_params(session.get(),"{\"scene_latitude_active\":true,\"scene_latitude_highlight_knee\":0,\"scene_latitude_highlight_room\":1,\"scene_latitude_shadow_knee\":-1,\"scene_latitude_shadow_room\":1}",nullptr)==SPK_OK,"Scene latitude remains refused");
        require(readPixels()!=neutral,"Scene latitude enabled but did not alter the rendered picture");
        require(spk_set_params(session.get(),"{\"scene_latitude_active\":false}",nullptr)==SPK_OK,"Scene bypass failed");
        require(readPixels()==neutral,"Scene latitude bypass is not bit-exact");
        char* latitude=nullptr;
        require(spk_scene_latitude(session.get(),"{}",&latitude)==SPK_OK&&latitude,"Measured scene-latitude API failed");
        spk_string_free(latitude);
        const std::string before = params(session.get());

        struct Feature { const char* key; const char* label; };
        const std::vector<Feature> features{
            {"contrast_mask_active", "contrast mask"},
#ifndef _WIN32
            {"overscan_active", "overscan"},
            {"date_imprint_active", "date imprint"},
#endif
        };
        for (const Feature& feature : features) {
            // Include a supported change to catch partial application before
            // the unsupported field is rejected.
            const std::string delta = std::string("{\"") + feature.key +
                                      "\":true,\"print_exposure\":1.25}";
            const spk_status status = spk_set_params(session.get(), delta.c_str(), nullptr);
            const std::string error = spk_last_error(engine.get());
            require(status == SPK_ERR_USER && error.find(feature.label) != std::string::npos &&
                    error.find("Vulkan backend") != std::string::npos,
                    std::string("unclear feature refusal: ") + feature.key + ": " + error);
            require(params(session.get()) == before, std::string("refused delta mutated params: ") + feature.key);
            render(engine.get(), session.get());

            spk_session* rejected = spk_open(engine.get(), &input, delta.c_str(), nullptr);
            const std::string open_error = spk_last_error(engine.get());
            if (rejected) spk_session_release(rejected);
            require(!rejected && open_error.find(feature.label) != std::string::npos,
                    std::string("open accepted an unported effect: ") + feature.key);
        }

        spk_result di{};
        char* di_json = nullptr;
        const spk_status di_status = spk_render_digital_intermediate(session.get(), &di, &di_json);
        const std::string di_error = spk_last_error(engine.get());
        const bool empty = !di.texture && !di.rgba16 && !di_json;
        spk_result_free(&di);
        spk_string_free(di_json);
        require(di_status == SPK_OK && !empty, "DI render failed: " + di_error);
        require(params(session.get()) == before, "DI export mutated the session");
        render(engine.get(), session.get());

        const float* mask = nullptr;
        uint32_t width = 0, height = 0;
        require(spk_contrast_mask_field(session.get(), "full", &mask, &width, &height) == SPK_OK &&
                !mask && width == 0 && height == 0, "disabled contrast mask must have an empty field");

        // Latitude analysis uses the existing pipeline; only applying its
        // mapping is unported. This also exercises probe_medium's readback.
        char* latitude_json = nullptr;
        const spk_status latitude_status = spk_scene_latitude(session.get(), nullptr, &latitude_json);
        const std::string latitude_error = spk_last_error(engine.get());
        const bool latitude_valid = latitude_json &&
            std::string(latitude_json).find("\"medium\"") != std::string::npos &&
            std::string(latitude_json).find("\"scene\"") != std::string::npos;
        spk_string_free(latitude_json);
        require(latitude_status == SPK_OK && latitude_valid, "latitude analysis failed: " + latitude_error);
        require(params(session.get()) == before, "latitude analysis mutated the session");
        render(engine.get(), session.get());
        std::cout << "Vulkan feature boundaries passed (unported flags, DI rendering, mask off, active/bypassed latitude and analysis)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
