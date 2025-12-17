#pragma once

#ifndef LYRA_LIBRARY_ENGINE_ASSETS_JSON_ASSET_H
#define LYRA_LIBRARY_ENGINE_ASSETS_JSON_ASSET_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Common/String.h>
#include <Lyra/Plugin/AMS/AMSAPI.h>

namespace lyra
{
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

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_ASSETS_JSON_ASSET_H
