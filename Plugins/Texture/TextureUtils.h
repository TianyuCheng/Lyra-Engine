#pragma once

#include <ktx.h>
#include <gli/gli.hpp>
#include <vulkan/vulkan.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Format/TextureAsset.h>

using namespace lyra;

namespace lyra::texture
{

    inline Logger get_logger()
    {
        static Logger logger = create_logger("Texture", LogLevel::trace);
        return logger;
    }

    inline VkFormat gli_to_vk_format(gli::format format)
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

    inline Path get_texture_cache_path(AssetID guid, OSPath caches_root)
    {
        return Path(caches_root) / "Textures" / (std::to_string(guid) + ".ktx2");
    }

    inline bool save_to_ktx2(JSON& metadata, ktxTexture2* texture, const Path& target_path, OSPath caches_root, Logger logger)
    {
        fs::create_directories(target_path.parent_path());
        KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(texture), target_path.string().c_str());
        ktxTexture_Destroy(ktxTexture(texture));

        if (result != KTX_SUCCESS) {
            logger->error("Failed to write KTX2 texture to file: {}", ktxErrorString(result));
            return false;
        }

        metadata["path"] = fs::relative(target_path, caches_root).string();
        return true;
    }

    inline bool encode_and_save_simple(JSON& metadata, void* pixels, int width, int height, size_t pixel_size, VkFormat format, OSPath caches_root, Logger logger)
    {
        const Path target_path = get_texture_cache_path(metadata["guid"].get<AssetID>(), caches_root);

        ktxTexture2*         texture;
        ktxTextureCreateInfo create_info;
        create_info.glInternalformat = 0;
        create_info.vkFormat         = format;
        create_info.baseWidth        = width;
        create_info.baseHeight       = height;
        create_info.baseDepth        = 1;
        create_info.numDimensions    = 2;
        create_info.numLevels        = 1;
        create_info.numLayers        = 1;
        create_info.numFaces         = 1;
        create_info.isArray          = KTX_FALSE;
        create_info.generateMipmaps  = KTX_TRUE;

        KTX_error_code result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
        if (result != KTX_SUCCESS) {
            logger->error("Failed to create KTX2 texture: {}", ktxErrorString(result));
            return false;
        }

        result = ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, 0, 0, static_cast<ktx_uint8_t*>(pixels), width * height * 4 * pixel_size);
        if (result != KTX_SUCCESS) {
            logger->error("Failed to set image data for KTX2 texture: {}", ktxErrorString(result));
            ktxTexture_Destroy(ktxTexture(texture));
            return false;
        }

        return save_to_ktx2(metadata, texture, target_path, caches_root, logger);
    }
} // namespace lyra::texture
