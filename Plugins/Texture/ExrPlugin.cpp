#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/Assets.h>
#include "TextureUtils.h"

using namespace lyra;

static Logger logger = create_logger("ExrCooker", LogLevel::trace);

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static JSON process_exr(OSPath source_path, OSPath target_path)
{
    auto source_path_str = String(reinterpret_cast<const char*>(source_path));
    int width, height;
    float* pixels = nullptr;
    const char* err = nullptr;
    int ret = LoadEXR(&pixels, &width, &height, source_path_str.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        logger->error("Failed to load EXR file: {} (error: {})", source_path_str, err ? err : "unknown");
        FreeEXRErrorMessage(err);
        return {};
    }

    auto metadata = encode_and_save_simple(pixels, width, height, sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, target_path, logger);
    free(pixels);
    return metadata;
}

static uint get_supported_cooker_extensions(CString* extensions)
{
    static const char* exts[] = {".exr"};
    if (extensions) {
        extensions[0] = exts[0];
    }
    return 1;
}

LYRA_EXPORT auto create() -> AssetCookerAPI
{
    auto api    = AssetCookerAPI{};
    api.configure = configure_cooker;
    api.process = process_exr;
    api.get_supported_extensions = get_supported_cooker_extensions;
    return api;
}
