#pragma once

#ifndef LYRA_LIBRARY_ENGINE_ASSETS_TEXTURE_ASSET_H
#define LYRA_LIBRARY_ENGINE_ASSETS_TEXTURE_ASSET_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Plugin/RHI/RHIEnums.h>
#include <Lyra/Plugin/AMS/AMSAPI.h>

namespace lyra
{
    struct TextureAsset
    {
        static constexpr CString name = "TextureAsset";

        // uuid: to let asset server know how to reference this type of asset
        static constexpr UUID uuid = make_uuid("30cdfac2-ad77-4297-91fb-832241bc4f3f");

        // extensions: to let assert server what extensions to look for
        static constexpr InitList<CString> extensions = {".png", ".jpg", ".exr"};

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

#endif // LYRA_LIBRARY_ENGINE_ASSETS_TEXTURE_H
