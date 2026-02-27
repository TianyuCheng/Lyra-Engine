#ifndef LYRA_LIBRARY_ASSETS_JSON_ASSET_H
#define LYRA_LIBRARY_ASSETS_JSON_ASSET_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Assets/AMSAPI.h>

namespace lyra
{
    /**
     * @brief A JSON document asset.
     */
    struct JsonAsset
    {
        static constexpr CString name = "JsonAsset";

        static constexpr UUID uuid = make_uuid("0d9c0641-6042-45a5-abea-467c8f2dc325");

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        JSON content; ///< Parsed JSON data.
    };
} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_JSON_ASSET_H
