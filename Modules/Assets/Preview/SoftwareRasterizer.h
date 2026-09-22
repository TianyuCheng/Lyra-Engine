#pragma once

#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Assets/AMSPreview.h>

namespace lyra::preview
{
    /**
     * @brief Configure pipeline-wide preview settings.
     */
    void configure(AssetServer* manager, const JSON& options);

    /**
     * @brief Offline CPU software rasterizer for preview scenes.
     */
    auto rasterize_scene(const PreviewScene& scene) -> PreviewTexture;

    /**
     * @brief Saves a thumbnail image from a preview scene and updates metadata.
     * @return The relative thumbnail path if successful, empty path otherwise.
     */
    auto generate_scene_thumbnail(
        const PreviewScene& scene,
        JSON&               metadata) -> Path;
}
