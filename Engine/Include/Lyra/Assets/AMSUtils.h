#pragma once

#ifndef LYRA_ENGINE_ASSETS_AMSUTILS_H
#define LYRA_ENGINE_ASSETS_AMSUTILS_H

#include <Lyra/Utilities/UUID.h>
#include <Lyra/Utilities/GUID.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/FileSystem/VFSTypes.h>
#include <Lyra/Assets/AMSEnums.h>

namespace lyra
{
    using AssetID = lyra::GUID;

    using AssetTypeID = lyra::UUID;

    /**
     * @brief A basic asset handle containing only a GUID.
     */
    struct RawAssetHandle
    {
        AssetID guid = 0ull; ///< globally unique identifier for the asset

        RawAssetHandle() : guid(0ull) {}
        RawAssetHandle(AssetID id) : guid(id) {}
        RawAssetHandle(const RawAssetHandle& other) : guid(other.guid) {}
        RawAssetHandle& operator=(const RawAssetHandle& other)
        {
            guid = other.guid;
            return *this;
        }

        FORCE_INLINE bool valid() const { return guid != 0; }
    };

    /**
     * @brief A type-safe asset handle that associates a GUID with a specific AssetType.
     * @tparam AssetType The class/struct representing the asset data.
     */
    template <typename AssetType>
    struct AssetHandle : RawAssetHandle
    {
        static constexpr AssetTypeID type = AssetType::type; ///< The static asset type.

        AssetHandle() = default;
        AssetHandle(AssetID id) : RawAssetHandle(id) {}
    };

    /**
     * @brief A record of an asset dependency within the asset pipeline.
     */
    struct AssetDependencyEntry
    {
        String      name;     ///< Local identifier / tag within the container (empty for external refs)
        AssetID     guid = 0; ///< 64-bit unique identifier
        AssetTypeID type = 0; ///< 128-bit UUID identifying the asset class
        String      path;     ///< Relative cache path (e.g. "meshes/123.mesh")
    };

    /**
     * @brief Generic tracker for asset dependencies during cooking.
     *        Resolves stable GUIDs across re-imports and writes structured dependency metadata.
     */
    struct AssetDependencyScope
    {
        JSON&                        metadata;
        Path                         source_path;
        Path                         caches_root;
        HashMap<String, AssetID>     prev_deps;
        HashSet<AssetID>             allocated_guids;
        Vector<AssetDependencyEntry> new_deps;

        AssetDependencyScope(JSON& metadata, const Path& source_path = {}, const Path& caches_root = {});
        ~AssetDependencyScope() = default;

        auto resolve(AssetID parent_guid, StringView dep_name, AssetTypeID type, StringView cache_rel_path = "") -> AssetID;
        void set_path(AssetID id, StringView path);
        void commit();
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

#endif // LYRA_ENGINE_ASSETS_AMSUTILS_H
