#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
#include <gli/gli.hpp>
#include <gli/load.hpp>

#include <algorithm>
#include <cctype>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/Assets.h>
#include "TextureUtils.h"

using namespace lyra;

static Logger logger = create_logger("TextureCooker", LogLevel::trace);

static VkFormat gli_to_vk_format(gli::format format)
{
    // clang-format off
    switch (format) {
        case gli::FORMAT_RGBA8_UNORM_PACK8:       return VK_FORMAT_R8G8B8A8_UNORM;
        case gli::FORMAT_RGBA8_SRGB_PACK8:        return VK_FORMAT_R8G8B8A8_SRGB;
        case gli::FORMAT_BGRA8_UNORM_PACK8:       return VK_FORMAT_B8G8R8A8_UNORM;
        case gli::FORMAT_BGRA8_SRGB_PACK8:        return VK_FORMAT_B8G8R8A8_SRGB;
        case gli::FORMAT_RGBA32_SFLOAT_PACK32:    return VK_FORMAT_R32G32B32A32_SFLOAT;
        case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:  return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:   return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16: return VK_FORMAT_BC2_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16:  return VK_FORMAT_BC2_SRGB_BLOCK;
        case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16: return VK_FORMAT_BC3_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:  return VK_FORMAT_BC3_SRGB_BLOCK;
        case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:    return VK_FORMAT_BC4_UNORM_BLOCK;
        case gli::FORMAT_R_ATI1N_SNORM_BLOCK8:    return VK_FORMAT_BC4_SNORM_BLOCK;
        case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:  return VK_FORMAT_BC5_UNORM_BLOCK;
        case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16:  return VK_FORMAT_BC5_SNORM_BLOCK;
        case gli::FORMAT_RGB_BP_UFLOAT_BLOCK16:   return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case gli::FORMAT_RGB_BP_SFLOAT_BLOCK16:   return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case gli::FORMAT_RGBA_BP_UNORM_BLOCK16:   return VK_FORMAT_BC7_UNORM_BLOCK;
        case gli::FORMAT_RGBA_BP_SRGB_BLOCK16:    return VK_FORMAT_BC7_SRGB_BLOCK;
        default:                                  return VK_FORMAT_UNDEFINED;
    }
    // clang-format on
}

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static bool process_stb(JSON& metadata, const String& source_path_str, OSPath target_path)
{
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
        logger->error("Failed to load image file via STB: {}", source_path_str);
        return false;
    }

    bool success = encode_and_save_simple(metadata, pixels, width, height, pixel_size, format, target_path, logger);
    stbi_image_free(pixels);
    return success;
}

static bool process_exr(JSON& metadata, const String& source_path_str, OSPath target_path)
{
    int     width, height;
    float*  pixels = nullptr;
    CString err    = nullptr;
    int     ret    = LoadEXR(&pixels, &width, &height, source_path_str.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        logger->error("Failed to load EXR file: {} (error: {})", source_path_str, err ? err : "unknown");
        FreeEXRErrorMessage(err);
        return false;
    }

    bool success = encode_and_save_simple(metadata, pixels, width, height, sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, target_path, logger);
    free(pixels);
    return success;
}

static bool process_dds(JSON& metadata, const String& source_path_str, OSPath target_path)
{
    gli::texture tex = gli::load(source_path_str);
    if (tex.empty()) {
        logger->error("Failed to load DDS file: {}", source_path_str);
        return false;
    }

    VkFormat vk_format = gli_to_vk_format(tex.format());
    if (vk_format == VK_FORMAT_UNDEFINED) {
        logger->error("Unsupported DDS format in file: {}", source_path_str);
        return false;
    }

    ktxTexture2*         ktx_tex;
    ktxTextureCreateInfo create_info;
    create_info.glInternalformat = 0;
    create_info.vkFormat         = vk_format;
    create_info.baseWidth        = tex.extent().x;
    create_info.baseHeight       = tex.extent().y;
    create_info.baseDepth        = tex.extent().z;
    create_info.numDimensions    = tex.target() == gli::TARGET_1D ? 1 : (tex.target() == gli::TARGET_3D ? 3 : 2);
    create_info.numLevels        = static_cast<uint32_t>(tex.levels());
    create_info.numLayers        = static_cast<uint32_t>(tex.layers());
    create_info.numFaces         = static_cast<uint32_t>(tex.faces());
    create_info.isArray          = tex.layers() > 1 ? KTX_TRUE : KTX_FALSE;
    create_info.generateMipmaps  = KTX_FALSE;

    KTX_error_code result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &ktx_tex);
    if (result != KTX_SUCCESS) {
        logger->error("Failed to create KTX2 texture from DDS: {}", ktxErrorString(result));
        return false;
    }

    for (size_t layer = 0; layer < tex.layers(); ++layer) {
        for (size_t face = 0; face < tex.faces(); ++face) {
            for (size_t level = 0; level < tex.levels(); ++level) {
                ktxTexture_SetImageFromMemory(ktxTexture(ktx_tex), static_cast<uint32_t>(level), static_cast<uint32_t>(layer), static_cast<uint32_t>(face),
                    static_cast<const ktx_uint8_t*>(tex.data(layer, face, level)), tex.size(level));
            }
        }
    }

    const Path final_target_path = get_texture_cache_path(metadata["guid"].get<AssetID>(), target_path);
    return save_to_ktx2(metadata, ktx_tex, final_target_path, target_path, logger);
}

static bool process_ktx(JSON& metadata, const String& source_path_str, OSPath target_path)
{
    Path dst = get_texture_cache_path(metadata["guid"].get<AssetID>(), target_path);

    fs::create_directories(dst.parent_path());
    fs::copy_file(Path(source_path_str), dst, fs::copy_options::overwrite_existing);

    metadata["path"] = fs::relative(dst, target_path).string();
    return true;
}

static bool process_texture(JSON& metadata, OSPath source_path, OSPath target_path)
{
    String source_path_str = String(reinterpret_cast<const char*>(source_path));
    Path   path(source_path_str);
    String ext = path.extension().string();

    if (ext == ".png" || ext == ".jpg" || ext == ".hdr") {
        return process_stb(metadata, source_path_str, target_path);
    } else if (ext == ".exr") {
        return process_exr(metadata, source_path_str, target_path);
    } else if (ext == ".dds") {
        return process_dds(metadata, source_path_str, target_path);
    } else if (ext == ".ktx" || ext == ".ktx2") {
        return process_ktx(metadata, source_path_str, target_path);
    }

    logger->error("Unsupported texture extension: {}", ext);
    return false;
}

static uint get_supported_cooker_extensions(CString* extensions)
{
    static const char* exts[] = {".png", ".jpg", ".hdr", ".exr", ".dds", ".ktx", ".ktx2"};
    if (extensions) {
        for (uint i = 0; i < 7; ++i)
            extensions[i] = exts[i];
    }
    return 7;
}

LYRA_EXPORT auto create() -> AssetCookerAPI
{
    auto api                     = AssetCookerAPI{};
    api.configure                = configure_cooker;
    api.process                  = process_texture;
    api.get_supported_extensions = get_supported_cooker_extensions;
    return api;
}
