#pragma once

#ifndef LYRA_LIBRARY_ASSETS_AMS_UTILS_H
#define LYRA_LIBRARY_ASSETS_AMS_UTILS_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/GUID.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/FileIO/VFSTypes.h>
#include <Lyra/Assets/AMSEnums.h>

namespace lyra
{
    /**
     * @brief A basic asset handle containing only a GUID.
     */
    struct RawAssetHandle
    {
        GUID guid = 0ull; ///< The globally unique identifier for the asset.

        /**
         * @brief Check if the handle points to a valid asset.
         */
        FORCE_INLINE bool valid() const { return guid != 0ull; }
    };

    /**
     * @brief A type-safe asset handle that associates a GUID with a specific AssetType.
     * @tparam AssetType The class/struct representing the asset data.
     */
    template <typename AssetType>
    struct AssetHandle : RawAssetHandle
    {
        static constexpr UUID type_uuid = AssetType::uuid; ///< The static UUID of the asset type.
    };

    /**
     * @brief Configuration for importing/cooking assets during development.
     */
    struct AMSImportDescriptor
    {
        OSPath assets_path;    ///< Directory where source assets (e.g. .png, .obj) are located.
        OSPath metadata_path;  ///< Directory where .import metadata files are generated.
        OSPath generated_path; ///< Directory where processed/cooked binary files are stored.
    };

    /**
     * @brief Configuration for loading assets at runtime.
     */
    struct AMSLoaderDescriptor
    {
        FileLoader* assets;
        FileLoader* metadata;
    };

    /**
     * @brief Main configuration for the AssetServer.
     */
    struct AMSDescriptor
    {
        AMSImportDescriptor importer;
        AMSLoaderDescriptor loader;

        bool watch   = false; ///< Whether to monitor the asset directory for hot-reloading.
        uint workers = 1;     ///< Number of worker threads for background asset processing.
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_AMS_UTILS_H
