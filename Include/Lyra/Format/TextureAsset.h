#ifndef LYRA_LYRA_FORMAT_TEXTUREASSET_H
#define LYRA_LYRA_FORMAT_TEXTUREASSET_H

#include <Lyra/Common/Collections.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Render/RHIEnums.h>

namespace lyra
{
    /**
     * @brief A texture asset containing binary image data and its subresource info.
     */
    struct TextureAsset
    {
        static constexpr CString name = "TextureAsset";

        static constexpr AssetTypeID type = 0xca9028ca;

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

#endif // LYRA_LYRA_FORMAT_TEXTUREASSET_H
