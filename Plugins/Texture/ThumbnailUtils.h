#pragma once

#include <Lyra/Common/Config.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Path.h>
#include <vulkan/vulkan.h>

namespace lyra::texture
{
    /**
     * @brief Generate a 128x128 thumbnail from raw pixel data.
     * @param metadata The JSON metadata to update with the thumbnail path.
     * @param pixels Pointer to the raw pixel data.
     * @param width Original width of the image.
     * @param height Original height of the image.
     * @param pixel_size Size of each pixel component (e.g., sizeof(float) or sizeof(uint8_t)).
     * @param format Vulkan format of the source pixels.
     * @param caches_root Root path of the cache directory.
     * @return True if successful.
     */
    bool generate_thumbnail_from_pixels(
        JSON& metadata, 
        void* pixels, 
        int width, 
        int height, 
        size_t pixel_size, 
        VkFormat format, 
        OSPath caches_root);
}
