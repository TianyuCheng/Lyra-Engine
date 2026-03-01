#ifndef LYRA_LIBRARY_ASSETS_JSON_ASSET_H
#define LYRA_LIBRARY_ASSETS_JSON_ASSET_H

#include <Lyra/Common/Config.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>

namespace lyra
{
    /**
     * @brief A JSON document asset.
     */
    struct JsonAsset
    {
        static constexpr CString name = "JsonAsset";

        static constexpr AssetTypeID type = 0x5372a266;

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        JSON content; ///< Parsed JSON data.
    };
} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_JSON_ASSET_H
