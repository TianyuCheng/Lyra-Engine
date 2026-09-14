#ifndef LYRA_LYRA_FORMAT_MATERIALASSET_H
#define LYRA_LYRA_FORMAT_MATERIALASSET_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Render/RHIEnums.h>
#include <Lyra/Format/TextureAsset.h>

// macro collision with Windows GDI
#undef OPAQUE

namespace lyra
{

    /**
     * @brief Blending modes for materials.
     */
    enum struct MaterialBlendMode : uint
    {
        OPAQUE,
        MASK,
        BLEND
    };

    /**
     * @brief A material asset defining the visual appearance of a mesh surface.
     *
     * Serialized as a USDA file (.material) using the UsdPreviewSurface shading model.
     * All USD I/O is confined to the implementation (.cpp) files.
     */
    struct MaterialAsset
    {
        static constexpr CString name = "MaterialAsset";

        static constexpr AssetTypeID type = 0xe3473e70;

        static auto saver() -> AssetSaverAPI;
        static auto loader() -> AssetLoaderAPI;

        // PBR parameters
        Vector4 base_color_factor  = Vector4(1.0f);
        float   metallic_factor    = 0.0f;
        float   roughness_factor   = 1.0f;
        Vector3 emissive_factor    = Vector3(0.0f);
        float   alpha_cutoff       = 0.5f;
        float   occlusion_strength = 1.0f;
        float   normal_scale       = 1.0f;

        // Texture maps
        TextureAssetHandle albedo_map;
        TextureAssetHandle normal_map;
        TextureAssetHandle metallic_roughness_map;
        TextureAssetHandle emissive_map;
        TextureAssetHandle occlusion_map;

        // Pipeline state
        MaterialBlendMode blend_mode  = MaterialBlendMode::OPAQUE;
        GPUCullMode       cull_mode   = GPUCullMode::BACK;
        bool              depth_write = true;
        bool              depth_test  = true;
    };

    using MaterialAssetHandle = AssetHandle<MaterialAsset>;

} // namespace lyra

#endif // LYRA_LYRA_FORMAT_MATERIALASSET_H
