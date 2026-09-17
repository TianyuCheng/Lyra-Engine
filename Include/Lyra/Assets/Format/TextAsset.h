#ifndef LYRA_LYRA_ASSETS_FORMAT_TEXTASSET_H
#define LYRA_LYRA_ASSETS_FORMAT_TEXTASSET_H

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

        static constexpr AssetTypeID type = make_uuid("e0fada30-0005-4e54-c332-5041a9c8f005");

        static auto saver() -> AssetSaverAPI;

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        String content; ///< Raw text content.
    };

    using TextAssetHandle = AssetHandle<TextAsset>;

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_FORMAT_TEXTASSET_H
