#ifndef LYRA_LIBRARY_ASSETS_TOML_ASSET_H
#define LYRA_LIBRARY_ASSETS_TOML_ASSET_H

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

        static constexpr AssetTypeID type = 0xc8e8e7c7;

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        TOML content; ///< Parsed TOML data.
    };
} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_TOML_ASSET_H
