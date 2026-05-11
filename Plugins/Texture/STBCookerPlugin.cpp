#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include "TextureUtils.h"

using namespace lyra;
using namespace lyra::texture;

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static bool process_stb(JSON& metadata, OSPath source_path, OSPath target_path)
{
    String   source_path_str = Path(source_path).string();
    int      width, height, channels;
    bool     is_hdr     = stbi_is_hdr(source_path_str.c_str());
    void*    pixels     = nullptr;
    size_t   pixel_size = 0;
    VkFormat format     = VK_FORMAT_UNDEFINED;

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
        get_logger()->error("Failed to load image file via STB: {}", source_path_str);
        return false;
    }

    bool success = encode_and_save_simple(metadata, pixels, width, height, pixel_size, format, target_path, get_logger());
    stbi_image_free(pixels);
    return success;
}

static uint get_stb_extensions(CString* extensions)
{
    static const char* exts[] = {".png", ".jpg", ".hdr"};
    if (extensions) {
        extensions[0] = exts[0];
        extensions[1] = exts[1];
        extensions[2] = exts[2];
    }
    return 3;
}

namespace lyra::texture::cooker::stb
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetCookerAPI
    {
        auto api                     = AssetCookerAPI{};
        api.configure                = configure_cooker;
        api.process                  = process_stb;
        api.get_supported_extensions = get_stb_extensions;
        return api;
    }
} // namespace lyra::texture::cooker::stb
