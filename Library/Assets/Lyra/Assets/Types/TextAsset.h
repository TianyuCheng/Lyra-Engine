#ifndef LYRA_LIBRARY_ASSETS_TEXT_ASSET_H
#define LYRA_LIBRARY_ASSETS_TEXT_ASSET_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/String.h>
#include <Lyra/Assets/AMSAPI.h>

namespace lyra
{
    /**
     * @brief A simple text file asset.
     */
    struct TextAsset
    {
        static constexpr CString name = "TextAsset";

        static constexpr UUID uuid = make_uuid("394a204e-8622-11f0-8de9-0242ac120002");

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        String content; ///< Raw text content.
    };
} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_TEXT_ASSET_H
