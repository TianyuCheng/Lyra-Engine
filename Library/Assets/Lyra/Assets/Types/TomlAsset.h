#ifndef LYRA_LIBRARY_ASSETS_TOML_ASSET_H
#define LYRA_LIBRARY_ASSETS_TOML_ASSET_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Assets/AMSAPI.h>

namespace lyra
{
    /**
     * @brief A TOML document asset.
     */
    struct TomlAsset
    {
        static constexpr CString name = "TomlAsset";

        static constexpr UUID uuid = make_uuid("a5a478e0-e725-4ad6-95bf-36d58cc10101");

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        TOML content; ///< Parsed TOML data.
    };
} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_TOML_ASSET_H
