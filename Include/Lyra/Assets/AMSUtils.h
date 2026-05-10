#pragma once

#ifndef LYRA_LYRA_ASSETS_AMSUTILS_H
#define LYRA_LYRA_ASSETS_AMSUTILS_H

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/GUID.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/FileIO/VFSTypes.h>
#include <Lyra/Assets/AMSEnums.h>

namespace lyra
{
    using AssetID = lyra::GUID;

    using AssetTypeID = uint;

    /**
     * @brief A basic asset handle containing only a GUID.
     */
    struct RawAssetHandle
    {
        AssetID uuid = 0ull; ///< The globally unique identifier for the asset.

        /**
         * @brief Default constructor.
         */
        RawAssetHandle() = default;

        /**
         * @brief Construct from a raw GUID.
         */
        RawAssetHandle(AssetID id) : uuid(id) {}

        /**
         * @brief Check if the handle points to a valid asset.
         */
        FORCE_INLINE bool valid() const { return uuid != 0; }
    };

    /**
     * @brief A type-safe asset handle that associates a GUID with a specific AssetType.
     * @tparam AssetType The class/struct representing the asset data.
     */
    template <typename AssetType>
    struct AssetHandle : RawAssetHandle
    {
        static constexpr uint type = AssetType::type; ///< The static asset type.

        /**
         * @brief Default constructor.
         */
        AssetHandle() = default;

        /**
         * @brief Construct from a raw GUID.
         */
        AssetHandle(AssetID id) : RawAssetHandle(id) {}
    };

    /**
     * @brief Configuration for importing/cooking assets during development.
     */
    struct AMSImportDescriptor
    {
        OSPath assets_path; ///< Directory where source assets (e.g. .png, .obj) are located.
        OSPath caches_path; ///< Directory where processed/cooked binary files are stored.
    };

    /**
     * @brief Configuration for loading assets at runtime.
     */
    struct AMSLoaderDescriptor
    {
        FileLoader* assets;
        FileLoader* caches;
    };

    /**
     * @brief Main configuration for the AssetServer.
     */
    struct AMSDescriptor
    {
        AMSImportDescriptor importer;
        AMSLoaderDescriptor loader;
        OSPath              registry;

        bool watch   = false; ///< Whether to monitor the asset directory for hot-reloading.
        uint workers = 1;     ///< Number of worker threads for background asset processing.
    };

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_AMSUTILS_H
