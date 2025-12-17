#pragma once

#ifndef LYRA_LIBRARY_ENGINE_ASSETS_TEXT_ASSET_H
#define LYRA_LIBRARY_ENGINE_ASSETS_TEXT_ASSET_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/String.h>
#include <Lyra/Plugin/AMS/AMSAPI.h>

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

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_ASSETS_TEXT_ASSET_H
