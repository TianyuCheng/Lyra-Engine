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

    struct TextAsset
    {
        static constexpr CString name = "TextAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("394a204e-8622-11f0-8de9-0242ac120002");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".txt"};

        // handler: to let asset server know how to load this type of asset
        static auto handler() -> AssetHandlerAPI;

        // text content
        String content;
    };

    struct JsonAsset
    {
        static constexpr CString name = "JsonAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("0d9c0641-6042-45a5-abea-467c8f2dc325");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".json"};

        // handler: to let asset server know how to load this type of asset
        static auto handler() -> AssetHandlerAPI;

        JSON content;
    };

    struct TomlAsset
    {
        static constexpr CString name = "TomlAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("a5a478e0-e725-4ad6-95bf-36d58cc10101");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".toml"};

        // handler: to let asset server know how to load this type of asset
        static auto handler() -> AssetHandlerAPI;

        TOML content;
    };

    struct TextureAsset
    {
        static constexpr CString name = "TextureAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("30cdfac2-ad77-4297-91fb-832241bc4f3f");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".png", ".jpg", ".exr", ".ktx"};

        // handler: to let asset server know how to load this type of asset
        static auto handler() -> AssetHandlerAPI;

        struct Subresource
        {
            uint offset; // offset into the binary blob
            uint pitch;  // bytes per row
            uint width;
            uint height;
        };

        GPUTextureFormat    format;
        Vector<uint8_t>     binary;
        Vector<Subresource> subresources;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_ASSETS_H
