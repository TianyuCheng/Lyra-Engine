#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include "TextureUtils.h"

using namespace lyra;
using namespace lyra::texture;

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static bool process_exr(JSON& metadata, OSPath source_path, OSPath target_path)
{
    String  source_path_str = String(reinterpret_cast<const char*>(source_path));
    int     width, height;
    float*  pixels = nullptr;
    CString err    = nullptr;
    int     ret    = LoadEXR(&pixels, &width, &height, source_path_str.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        get_logger()->error("Failed to load EXR file: {} (error: {})", source_path_str, err ? err : "unknown");
        FreeEXRErrorMessage(err);
        return false;
    }

    bool success = encode_and_save_simple(metadata, pixels, width, height, sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, target_path, get_logger());
    free(pixels);
    return success;
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
