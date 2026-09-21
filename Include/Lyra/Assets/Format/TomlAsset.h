#ifndef LYRA_LYRA_ASSETS_FORMAT_TOMLASSET_H
#define LYRA_LYRA_ASSETS_FORMAT_TOMLASSET_H

#include <Lyra/Common/Config.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>

namespace lyra
{
    /**
     * @brief A TOML document asset.
     */
    struct TomlAsset
    {
        static constexpr CString name = "TomlAsset";

        static constexpr AssetTypeID type = make_uuid("c8e8e7c7-0007-4076-e554-7263a9c8f007");

        static auto saver() -> AssetSaverAPI;

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        TOML content; ///< Parsed TOML data.
    };

    using TomlAssetHandle = AssetHandle<TomlAsset>;

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_FORMAT_TOMLASSET_H
