#include <stb_image_resize2.h>
#include <stb_image_write.h>

#include <Lyra/Common/Logger.h>
#include <Lyra/Assets/AMSUtils.h>
#include "ThumbnailUtils.h"

namespace lyra::texture
{
    bool generate_thumbnail_from_pixels(
        JSON& metadata, 
        void* pixels, 
        int width, 
        int height, 
        size_t pixel_size, 
        VkFormat format, 
        OSPath caches_root)
    {
        constexpr int thumb_size = 128;
        int thumb_width = thumb_size;
        int thumb_height = thumb_size;

        // maintain aspect ratio
        if (width > height) {
            thumb_height = (int)((float)height / width * thumb_size);
        } else {
            thumb_width = (int)((float)width / height * thumb_size);
        }

        Vector<uint8_t> thumb_pixels(thumb_width * thumb_height * 4);

        bool resize_success = false;
        if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
            Vector<float> temp_float(thumb_width * thumb_height * 4);
            
            float* res = stbir_resize_float_linear(
                (float*)pixels, width, height, 0,
                temp_float.data(), thumb_width, thumb_height, 0,
                STBIR_RGBA
            );
            resize_success = (res != nullptr);
            
            if (resize_success) {
                // convert float to uint8
                for (size_t i = 0; i < thumb_pixels.size(); ++i) {
                    float f = temp_float[i];
                    thumb_pixels[i] = (uint8_t)(std::clamp(f, 0.0f, 1.0f) * 255.0f);
                }
            }
        } else {
            uint8_t* res = stbir_resize_uint8_linear(
                (uint8_t*)pixels, width, height, 0,
                thumb_pixels.data(), thumb_width, thumb_height, 0,
                STBIR_RGBA
            );
            resize_success = (res != nullptr);
        }

        if (!resize_success) {
            return false;
        }

        Path thumb_dir = Path(caches_root) / "Thumbnails";
        if (!std::filesystem::exists(thumb_dir)) {
            std::filesystem::create_directories(thumb_dir);
        }

        AssetID guid = metadata["guid"].get<AssetID>();
        Path thumb_path = thumb_dir / (std::to_string(guid) + ".thumb.png");

        if (!stbi_write_png(thumb_path.string().c_str(), thumb_width, thumb_height, 4, thumb_pixels.data(), thumb_width * 4)) {
            return false;
        }

        metadata["thumbnail"] = std::filesystem::relative(thumb_path, caches_root).string();
        return true;
    }
}
