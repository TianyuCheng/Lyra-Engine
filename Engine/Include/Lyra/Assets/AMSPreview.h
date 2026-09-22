#pragma once

#ifndef LYRA_ENGINE_ASSETS_AMSPREVIEW_H
#define LYRA_ENGINE_ASSETS_AMSPREVIEW_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Promise.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Assets/AMSAPI.h>

namespace lyra
{
    /**
     * @brief Raw pixel container for preview images and texture buffers.
     */
    struct PreviewTexture
    {
        uint            width    = 0;
        uint            height   = 0;
        uint            channels = 4;
        Vector<uint8_t> pixels; // RGBA8
    };

    /**
     * @brief Surface material properties for preview rendering.
     */
    struct PreviewMaterial
    {
        Vector4 base_color_factor = Vector4(1.0f);
        float   roughness         = 1.0f;
        float   metallic          = 0.0f;
        int     albedo_texture_id = -1; // Index into PreviewScene::textures
    };

    /**
     * @brief Mesh geometry container for offline preview rendering.
     */
    struct PreviewMesh
    {
        Vector<Vector3> positions;
        Vector<Vector3> normals;
        Vector<Vector2> uvs;
        Vector<uint>    indices;
        Matrix4x4       transform   = Matrix4x4(1.0f);
        int             material_id = -1; // Index into PreviewScene::materials
    };

    /**
     * @brief Complete preview scene containing meshes, materials, and textures.
     */
    struct PreviewScene
    {
        Vector<PreviewMesh>     meshes;
        Vector<PreviewMaterial> materials;
        Vector<PreviewTexture>  textures;
        bool                    is_flat = false; ///< True for 2D flat textured quad (e.g. image previews)

        auto calculate_bounds() const -> std::pair<Vector3, Vector3>;

        /// Convenience helper: create a textured quad (e.g. for image preview)
        static auto make_textured_quad(PreviewTexture texture) -> PreviewScene;

        /// Convenience helper: create a single mesh preview scene
        static auto make_mesh(
            Vector<Vector3> positions,
            Vector<Vector3> normals = {},
            Vector<Vector2> uvs     = {},
            Vector<uint>    indices = {}) -> PreviewScene;
    };

    /**
     * @brief Retrieve the global AssetPreviewAPI instance.
     */
    auto preview_api() -> AssetPreviewAPI&;

} // namespace lyra

#endif // LYRA_ENGINE_ASSETS_AMSPREVIEW_H
