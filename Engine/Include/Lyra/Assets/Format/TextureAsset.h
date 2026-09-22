#ifndef LYRA_ENGINE_ASSETS_FORMAT_TEXTUREASSET_H
#define LYRA_ENGINE_ASSETS_FORMAT_TEXTUREASSET_H

#include <Lyra/Utilities/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Graphics/RHIEnums.h>

namespace lyra
{
    /**
     * @brief A texture asset containing binary image data and its subresource info.
     */
    struct TextureAsset
    {
        static constexpr CString name = "TextureAsset";

        static constexpr AssetTypeID type = make_uuid("5a18a8b0-0004-4d43-b221-4930a9c8f004");

        static auto loader() -> AssetLoaderAPI;

        // stb cooker
        struct stb
        {
            static auto cooker() -> AssetCookerAPI;
        };

        // exr cooker
        struct exr
        {
            static auto cooker() -> AssetCookerAPI;
        };

        // dds cooker
        struct dds
        {
            static auto cooker() -> AssetCookerAPI;
        };

        // ktx cooker
        struct ktx
        {
            static auto cooker() -> AssetCookerAPI;
        };

        struct Subresource
        {
            uint offset; ///< Offset into the binary blob (bytes).
            uint pitch;  ///< Bytes per row.
            uint width;  ///< Width of the subresource level.
            uint height; ///< Height of the subresource level.
        };

        GPUTextureFormat    format;       ///< Format required by the GPU.
        Vector<uint8_t>     binary;       ///< Raw optimized image bits.
        Vector<Subresource> subresources; ///< List of subresource headers.
    };

    using TextureAssetHandle = AssetHandle<TextureAsset>;

} // namespace lyra

#endif // LYRA_ENGINE_ASSETS_FORMAT_TEXTUREASSET_H
