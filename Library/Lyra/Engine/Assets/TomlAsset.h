#pragma once

#ifndef LYRA_LIBRARY_ENGINE_ASSETS_TOML_ASSET_H
#define LYRA_LIBRARY_ENGINE_ASSETS_TOML_ASSET_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Common/String.h>
#include <Lyra/Plugin/AMS/AMSAPI.h>

namespace lyra
{
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

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_ASSETS_TOML_ASSET_H
