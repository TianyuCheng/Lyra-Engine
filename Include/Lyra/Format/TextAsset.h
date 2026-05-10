#ifndef LYRA_LYRA_FORMAT_TEXTASSET_H
#define LYRA_LYRA_FORMAT_TEXTASSET_H

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

        static constexpr AssetTypeID type = 0xe0fada30;

        static auto loader() -> AssetLoaderAPI;

        static auto cooker() -> AssetCookerAPI;

        String content; ///< Raw text content.
    };

    using TextAssetHandle = AssetHandle<TextAsset>;

} // namespace lyra

#endif // LYRA_LYRA_FORMAT_TEXTASSET_H
