#pragma once

#ifndef LYRA_LYRA_ASSETS_AMSREGISTRY_H
#define LYRA_LYRA_ASSETS_AMSREGISTRY_H

#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/FileIO/VFSAPI.h>

namespace lyra
{
    /**
     * @brief A single entry in the asset registry.
     */
    struct AssetEntry
    {
        AssetID           guid;         ///< Globally unique identifier for the asset.
        AssetTypeID       type;         ///< Enum-based asset type.
        uint32_t          path;         ///< Index into the string table for the asset path.
        Vector<AssetID>   dependencies; ///< List of GUIDs this asset depends on.
    };

    /**
     * @brief The AssetRegistry maintains a mapping between AssetIDs and paths.
     * It can be serialized to binary or TOML formats.
     */
    struct AssetRegistry
    {
    public:
        AssetRegistry() = default;

        /**
         * @brief Load the registry from a file (detects format by extension: .bin or .toml).
         * @param path Full path to the registry file.
         * @return True if loading was successful.
         */
        bool load(const OSPath& path);

        /**
         * @brief Save the registry to a file (detects format by extension: .bin or .toml).
         * @param path Full path to the target file.
         * @return True if saving was successful.
         */
        bool save(const OSPath& path);

        /**
         * @brief Check if the registry is dirty and save it to the specified path if it is.
         * @param path Full path to the target file.
         * @return True if saving was successful or if the registry was not dirty.
         */
        bool flush(const OSPath& path);

        /**
         * @brief Scan the OS filesystem for existing *.import files to rebuild the registry.
         * @param assets_dir The directory to scan recursively.
         */
        void rebuild(const OSPath& assets_dir);

        /**
         * @brief Update or add an asset entry in the registry.
         */
        void update(AssetID guid, StringView path, AssetTypeID type, const Vector<AssetID>& dependencies = {});

        /**
         * @brief Get the asset path associated with a GUID.
         */
        StringView get_path(AssetID guid) const;

        /**
         * @brief Get the GUID associated with an asset path.
         */
        AssetID get_guid(StringView path) const;

        /**
         * @brief Get the asset type associated with a GUID.
         */
        AssetTypeID get_type(AssetID guid) const;

        /**
         * @brief Get the list of dependencies for an asset.
         */
        const Vector<AssetID>& get_dependencies(AssetID guid) const;

        /**
         * @brief Generate a new random GUID that is guaranteed to be unique within this registry.
         */
        AssetID generate_guid();

        /**
         * @brief Check if the registry has been modified since the last save.
         */
        bool is_dirty() const { return dirty; }

        /**
         * @brief Clear the dirty flag.
         */
        void clear_dirty() { dirty = false; }

    private:
        bool load_binary(const OSPath& path);
        bool save_binary(const OSPath& path);
        bool load_toml(const OSPath& path);
        bool save_toml(const OSPath& path);

        Vector<AssetEntry> entries;
        Deque<String>      string_table;

        HashMap<AssetID, uint32_t>   guid_to_entry_index;
        HashMap<StringView, AssetID> path_to_guid;

        bool dirty = false;

        /**
         * @brief Internal helper to rebuild lookup tables after loading or major changes.
         */
        void build_lookup_tables();
    };

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_AMSREGISTRY_H
