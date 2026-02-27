#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/Assets.h>
#include "TextureUtils.h"

using namespace lyra;

static Logger logger = create_logger("StbCooker", LogLevel::trace);

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static JSON process_stb(OSPath source_path, OSPath target_path)
{
    auto source_path_str = String(reinterpret_cast<const char*>(source_path));
    int width, height, channels;
    bool is_hdr = stbi_is_hdr(source_path_str.c_str());
    void* pixels = nullptr;
    size_t pixel_size = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;

    if (is_hdr) {
        pixels     = stbi_loadf(source_path_str.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        pixel_size = sizeof(float);
        format     = VK_FORMAT_R32G32B32A32_SFLOAT;
    } else {
        pixels     = stbi_load(source_path_str.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        pixel_size = sizeof(stbi_uc);
        format     = VK_FORMAT_R8G8B8A8_SRGB;
    }

    if (!pixels) {
        logger->error("Failed to load image file via STB: {}", source_path_str);
        return {};
    }

    auto metadata = encode_and_save_simple(pixels, width, height, pixel_size, format, target_path, logger);
    stbi_image_free(pixels);
    return metadata;
}

static uint get_supported_cooker_extensions(CString* extensions)
{
    static const char* exts[] = {".png", ".jpg", ".hdr"};
    if (extensions) {
        for (uint i = 0; i < 3; ++i) extensions[i] = exts[i];
    }
    return 3;
}

LYRA_EXPORT auto create() -> AssetCookerAPI
{
    auto api    = AssetCookerAPI{};
    api.configure = configure_cooker;
    api.process = process_stb;
    api.get_supported_extensions = get_supported_cooker_extensions;
    return api;
}
