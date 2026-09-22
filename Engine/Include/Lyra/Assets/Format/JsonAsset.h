#ifndef LYRA_ENGINE_ASSETS_FORMAT_JSONASSET_H
#define LYRA_ENGINE_ASSETS_FORMAT_JSONASSET_H

#include <Lyra/Utilities/Config.h>
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

        static constexpr AssetTypeID type = make_uuid("5372a266-0006-4f65-d443-6152a9c8f006");

        static auto saver() -> AssetSaverAPI;

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        JSON content; ///< Parsed JSON data.
    };

    using JsonAssetHandle = AssetHandle<JsonAsset>;

} // namespace lyra

#endif // LYRA_ENGINE_ASSETS_FORMAT_JSONASSET_H
