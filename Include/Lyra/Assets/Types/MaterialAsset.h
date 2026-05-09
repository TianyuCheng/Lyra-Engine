#ifndef LYRA_LYRA_ASSETS_TYPES_MATERIALASSET_H
#define LYRA_LYRA_ASSETS_TYPES_MATERIALASSET_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Render/RHIEnums.h>

// macro collision with Windows GDI
#undef OPAQUE

namespace lyra
{
    struct TextureAsset;

    using AssetHandleTexture = AssetHandle<TextureAsset>;

    /**
     * @brief A material asset defining the visual appearance of a mesh.
     */
    struct MaterialAsset
    {
        static constexpr CString name = "MaterialAsset";

        static constexpr AssetTypeID type = 0xe3473e70;

        static auto loader() -> AssetLoaderAPI;

        enum struct BlendMode : uint
        {
            OPAQUE,
            MASK,
            BLEND
        };

        String                              shader_id; ///< Identifier for the shader/technique to use.
        HashMap<String, AssetHandleTexture> textures;  ///< Map of texture parameter names to asset handles.
        HashMap<String, Vector4>            constants; ///< Map of constant parameter names to values.

        BlendMode   blend_mode  = BlendMode::OPAQUE; ///< Blending mode for this material.
        GPUCullMode cull_mode   = GPUCullMode::BACK; ///< Culling mode for the pipeline.
        bool        depth_write = true;              ///< Whether to write to the depth buffer.
        bool        depth_test  = true;              ///< Whether to perform depth testing.
    };
} // namespace lyra

#endif // LYRA_LYRA_ASSETS_TYPES_MATERIALASSET_H
