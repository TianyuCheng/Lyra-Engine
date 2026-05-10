#ifndef LYRA_LYRA_FORMAT_MATERIALASSET_H
#define LYRA_LYRA_FORMAT_MATERIALASSET_H

#include <Lyra/Format/MaterialSchema.h>

// macro collision with Windows GDI
#undef OPAQUE

namespace lyra
{

    using MaterialSchemaHandle = AssetHandle<MaterialSchema>;

    /**
     * @brief A material asset defining the visual appearance of a mesh.
     */
    struct MaterialAsset
    {
        static constexpr CString name = "MaterialAsset";

        static constexpr AssetTypeID type = 0xe3473e70;

        static auto loader() -> AssetLoaderAPI;

        MaterialSchemaHandle schema; ///< The schema this material follows.
        MaterialParams       params; ///< Actual values for parameters defined in the schema.

        // Overrides for pipeline states (if empty, defaults from schema are used)
        Optional<GPUCullMode> cull_mode;
        Optional<bool>        depth_write;
        Optional<bool>        depth_test;
    };

    using MaterialAssetHandle = AssetHandle<MaterialAsset>;

} // namespace lyra

#endif // LYRA_LYRA_FORMAT_MATERIALASSET_H
