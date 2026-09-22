#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include "TextureUtils.h"
#include "ThumbnailUtils.h"

using namespace lyra;
using namespace lyra::texture;

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static bool process_exr(JSON& metadata, OSPath source_path, OSPath target_path)
{
    try {
        String source_path_str = Path(source_path).string();
        int     width = 0, height = 0;
        float*  pixels = nullptr;
        CString err    = nullptr;
        int     ret    = LoadEXR(&pixels, &width, &height, source_path_str.c_str(), &err);
        if (ret != TINYEXR_SUCCESS || !pixels || width <= 0 || height <= 0) {
            get_logger()->error("Failed to load EXR file: {} (error: {})", source_path_str, err ? err : "unknown or invalid dimensions");
            if (err) FreeEXRErrorMessage(err);
            if (pixels) free(pixels);
            return false;
        }

        generate_thumbnail_from_pixels(metadata, pixels, width, height, sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, target_path);

        bool success = encode_and_save_simple(metadata, pixels, width, height, sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, target_path, get_logger());
        free(pixels);
        return success;
    } catch (const std::exception& e) {
        get_logger()->error("Exception while processing EXR file {}: {}", Path(source_path).string(), e.what());
        return false;
    } catch (...) {
        get_logger()->error("Unknown error while processing EXR file {}", Path(source_path).string());
        return false;
    }
}

static uint get_exr_extensions(CString* extensions)
{
    if (extensions) extensions[0] = ".exr";
    return 1;
}

namespace lyra::texture::cooker::exr
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetCookerAPI
    {
        auto api                     = AssetCookerAPI{};
        api.configure                = configure_cooker;
        api.process                  = process_exr;
        api.get_supported_extensions = get_exr_extensions;
        return api;
    }
} // namespace lyra::texture::cooker::exr
