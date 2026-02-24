#ifndef LYRA_LIBRARY_ASSETS_ASSETS_H
#define LYRA_LIBRARY_ASSETS_ASSETS_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Render/RHIAPI.h>

namespace lyra
{
    /**
     * @brief A simple text file asset.
     */
    struct TextAsset
    {
        static constexpr CString name = "TextAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("394a204e-8622-11f0-8de9-0242ac120002");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".txt"};

        /**
         * @brief Get the built-in handler for TextAssets.
         */
        static auto handler() -> AssetHandlerAPI;

        String content; ///< Raw text content.
    };

    /**
     * @brief A JSON document asset.
     */
    struct JsonAsset
    {
        static constexpr CString name = "JsonAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("0d9c0641-6042-45a5-abea-467c8f2dc325");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".json"};

        /**
         * @brief Get the built-in handler for JsonAssets.
         */
        static auto handler() -> AssetHandlerAPI;

        JSON content; ///< Parsed JSON data.
    };

    /**
     * @brief A TOML document asset.
     */
    struct TomlAsset
    {
        static constexpr CString name = "TomlAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("a5a478e0-e725-4ad6-95bf-36d58cc10101");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".toml"};

        /**
         * @brief Get the built-in handler for TomlAssets.
         */
        static auto handler() -> AssetHandlerAPI;

        TOML content; ///< Parsed TOML data.
    };

    /**
     * @brief A texture asset containing binary image data and its subresource info.
     */
    struct TextureAsset
    {
        static constexpr CString name = "TextureAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("30cdfac2-ad77-4297-91fb-832241bc4f3f");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".png", ".jpg", ".exr", ".ktx"};

        /**
         * @brief Get the handler for TextureAssets (typically loaded via a plugin).
         */
        static auto handler() -> AssetHandlerAPI;

        /**
         * @brief Metadata for an individual mip-level or subresource.
         */
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

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_ASSETS_H
