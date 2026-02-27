#pragma once

#include <ktx.h>
#include <vulkan/vulkan.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Assets/Assets.h>

namespace lyra {

inline JSON save_to_ktx2(ktxTexture2* texture, OSPath target_path, Logger logger)
{
    KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(texture), reinterpret_cast<const char*>(target_path));
    ktxTexture_Destroy(ktxTexture(texture));

    if (result != KTX_SUCCESS) {
        logger->error("Failed to write KTX2 texture to file: {}", ktxErrorString(result));
        return {};
    }

    auto metadata    = JSON{};
    metadata["path"] = reinterpret_cast<const char*>(target_path);
    return metadata;
}

inline JSON encode_and_save_simple(void* pixels, int width, int height, size_t pixel_size, VkFormat format, OSPath target_path, Logger logger)
{
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
        return {};
    }

    result = ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, 0, 0, static_cast<ktx_uint8_t*>(pixels), width * height * 4 * pixel_size);
    if (result != KTX_SUCCESS) {
        logger->error("Failed to set image data for KTX2 texture: {}", ktxErrorString(result));
        ktxTexture_Destroy(ktxTexture(texture));
        return {};
    }

    return save_to_ktx2(texture, target_path, logger);
}

} // namespace lyra
