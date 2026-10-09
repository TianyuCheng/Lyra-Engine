#pragma once

#include <algorithm>
#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Assets/AMSPreview.h>
#include <vulkan/vulkan.h>

namespace lyra::texture
{
    /**
     * @brief Generate a thumbnail for a texture via unified preview_api().
     */
    inline bool generate_thumbnail_from_pixels(
        JSON&    metadata,
        void*    pixels,
        int      width,
        int      height,
        size_t   pixel_size,
        VkFormat format,
        OSPath /*caches_root*/ = nullptr)
    {
        if (!pixels || width <= 0 || height <= 0) return false;

        PreviewTexture tex;
        tex.width    = static_cast<uint>(width);
        tex.height   = static_cast<uint>(height);
        tex.channels = 4;

        if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
            const float* f_pixels = static_cast<const float*>(pixels);
            tex.pixels.resize(width * height * 4);
            for (size_t i = 0; i < tex.pixels.size(); ++i) {
                tex.pixels[i] = static_cast<uint8_t>(std::clamp(f_pixels[i], 0.0f, 1.0f) * 255.0f);
            }
        } else {
            const uint8_t* u_pixels  = static_cast<const uint8_t*>(pixels);
            size_t         num_bytes = static_cast<size_t>(width * height * 4);
            tex.pixels.assign(u_pixels, u_pixels + num_bytes);
        }

        auto scene  = PreviewScene::make_textured_quad(std::move(tex));
        auto future = preview_api().generate_thumbnail(scene, metadata);
        return !future.get().empty();
    }
} // namespace lyra::texture
