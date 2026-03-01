#ifndef LYRA_LIBRARY_ASSETS_TEXT_ASSET_H
#define LYRA_LIBRARY_ASSETS_TEXT_ASSET_H

#include <Lyra/Common/String.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>

namespace lyra
{
    /**
     * @brief A simple text file asset.
     */
    struct TextAsset
    {
        static constexpr CString name = "TextAsset";

        static constexpr uint type = static_cast<uint>(AssetType::TEXT);

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        String content; ///< Raw text content.
    };
} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_TEXT_ASSET_H
