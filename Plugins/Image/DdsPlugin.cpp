#include <gli/gli.hpp>
#include <gli/load.hpp>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/Assets.h>
#include "TextureUtils.h"

using namespace lyra;

static Logger logger = create_logger("DdsCooker", LogLevel::trace);

static VkFormat gli_to_vk_format(gli::format format)
{
    switch (format) {
        case gli::FORMAT_RGBA8_UNORM_PACK8: return VK_FORMAT_R8G8B8A8_UNORM;
        case gli::FORMAT_RGBA8_SRGB_PACK8:  return VK_FORMAT_R8G8B8A8_SRGB;
        case gli::FORMAT_BGRA8_UNORM_PACK8: return VK_FORMAT_B8G8R8A8_UNORM;
        case gli::FORMAT_BGRA8_SRGB_PACK8:  return VK_FORMAT_B8G8R8A8_SRGB;
        case gli::FORMAT_RGBA32_SFLOAT_PACK32: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16: return VK_FORMAT_BC2_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16: return VK_FORMAT_BC2_SRGB_BLOCK;
        case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16: return VK_FORMAT_BC3_UNORM_BLOCK;
        case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16: return VK_FORMAT_BC3_SRGB_BLOCK;
        case gli::FORMAT_R_ATI1N_UNORM_BLOCK8: return VK_FORMAT_BC4_UNORM_BLOCK;
        case gli::FORMAT_R_ATI1N_SNORM_BLOCK8: return VK_FORMAT_BC4_SNORM_BLOCK;
        case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16: return VK_FORMAT_BC5_UNORM_BLOCK;
        case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16: return VK_FORMAT_BC5_SNORM_BLOCK;
        case gli::FORMAT_RGB_BP_UFLOAT_BLOCK16: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case gli::FORMAT_RGB_BP_SFLOAT_BLOCK16: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case gli::FORMAT_RGBA_BP_UNORM_BLOCK16: return VK_FORMAT_BC7_UNORM_BLOCK;
        case gli::FORMAT_RGBA_BP_SRGB_BLOCK16: return VK_FORMAT_BC7_SRGB_BLOCK;
        default: return VK_FORMAT_UNDEFINED;
    }
}

static void configure_cooker(AssetServer* manager, const JSON& options) {}

static JSON process_dds(OSPath source_path, OSPath target_path)
{
    auto source_path_str = String(reinterpret_cast<const char*>(source_path));
    gli::texture tex = gli::load(source_path_str);
    if (tex.empty()) {
        logger->error("Failed to load DDS file: {}", source_path_str);
        return {};
    }

    VkFormat vk_format = gli_to_vk_format(tex.format());
    if (vk_format == VK_FORMAT_UNDEFINED) {
        logger->error("Unsupported DDS format in file: {}", source_path_str);
        return {};
    }

    ktxTexture2* ktx_tex;
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
        return {};
    }

    for (size_t layer = 0; layer < tex.layers(); ++layer) {
        for (size_t face = 0; face < tex.faces(); ++face) {
            for (size_t level = 0; level < tex.levels(); ++level) {
                ktxTexture_SetImageFromMemory(ktxTexture(ktx_tex), static_cast<uint32_t>(level), static_cast<uint32_t>(layer), static_cast<uint32_t>(face), 
                    static_cast<const ktx_uint8_t*>(tex.data(layer, face, level)), tex.size(level));
            }
        }
    }

    return save_to_ktx2(ktx_tex, target_path, logger);
}

static uint get_supported_cooker_extensions(CString* extensions)
{
    static const char* exts[] = {".dds"};
    if (extensions) {
        extensions[0] = exts[0];
    }
    return 1;
}

LYRA_EXPORT auto create() -> AssetCookerAPI
{
    auto api    = AssetCookerAPI{};
    api.configure = configure_cooker;
    api.process = process_dds;
    api.get_supported_extensions = get_supported_cooker_extensions;
    return api;
}
