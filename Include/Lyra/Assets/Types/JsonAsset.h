#ifndef LYRA_LYRA_ASSETS_TYPES_JSONASSET_H
#define LYRA_LYRA_ASSETS_TYPES_JSONASSET_H

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

#endif // LYRA_LYRA_ASSETS_TYPES_JSONASSET_H
