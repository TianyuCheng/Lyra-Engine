#pragma once

#ifndef LYRA_LYRA_ASSETS_AMSSERVER_H
#define LYRA_LYRA_ASSETS_AMSSERVER_H

#include <memory>
#include <atomic>
#include <shared_mutex>

#include <BS_thread_pool.hpp>
#include <absl/strings/ascii.h>
#include <Lyra/Common/UUID.h>
#include <Lyra/Common/GUID.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Promise.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/FileIO/VFSEnums.h>
#include <Lyra/FileIO/VFSUtils.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSRegistry.h>
#include <Lyra/Assets/AMSWatcher.h>

namespace lyra
{
    /**
     * @brief Pipeline status and metrics.
     */
    struct AssetPipelineStats
    {
        uint   pending_count   = 0;
        uint   completed_count = 0;
        uint   failed_count    = 0;
        String current_asset   = "";
        bool   watching        = false;
    };

    using AssetReloadCallback      = Function<void(AssetID, AssetTypeID)>;
    using FileSystemChangeCallback = Function<void()>;

    /**
     * @brief The AssetServer manages engine assets with a reference counting policy based on handles.
     */
    struct AssetServer
    {
    public:
        explicit AssetServer(const AMSDescriptor& descriptor);

        virtual ~AssetServer();

        /**
         * @brief Internal detection for cooker support.
         */
        template <typename T, typename = void>
        struct has_cooker : std::false_type
        {
        };

        template <typename T>
        struct has_cooker<T, std::void_t<decltype(T::cooker())>> : std::true_type
        {
        };

        /**
         * @brief Internal detection for saver support.
         */
        template <typename T, typename = void>
        struct has_saver : std::false_type
        {
        };

        template <typename T>
        struct has_saver<T, std::void_t<decltype(T::saver())>> : std::true_type
        {
        };

        /**
         * @brief Purge unloaded assets.
         */
        void purge();

        /**
         * @brief Flush dirty registry to disk.
         */
        void flush();

        /**
         * @brief Register a loader for an asset type.
         * If the asset type also provides a cooker() method, it will be registered automatically.
         */
        template <typename AssetType>
        void register_asset(const JSON& options = {})
        {
            auto info       = std::make_unique<AssetProcessor>();
            info->type_name = AssetType::name;
            info->type_id   = AssetType::type;
            info->loader    = AssetType::loader();
            info->assets    = {};

            if (info->loader.configure) {
                info->loader.configure(this, options);
            }

            uint count = info->loader.get_supported_extensions(nullptr);

            Vector<CString> exts(count);
            info->loader.get_supported_extensions(exts.data());

            auto [it, success]       = processors.emplace(AssetType::type, std::move(info));
            AssetProcessor* proc_ptr = it->second.get();

            for (uint i = 0; i < count; ++i) {
                loader_extensions.emplace(absl::AsciiStrToLower(exts[i]), proc_ptr);
            }

            // automatically register cooker if provided by the AssetType
            if constexpr (has_cooker<AssetType>::value) {
                register_asset<AssetType, AssetType>(options);
            }

            // automatically register saver if provided by the AssetType
            if constexpr (has_saver<AssetType>::value) {
                register_saver<AssetType>(options);
            }
        }

        /**
         * @brief Register a cooker for an asset type from a specific source format.
         */
        template <typename AssetType, typename CookerType>
        void register_asset(const JSON& options = {})
        {
            auto it = processors.find(AssetType::type);
            if (it == processors.end()) {
                spdlog::error("AssetType ({}) has not been registered! Please register the loader first.", AssetType::name);
                return;
            }

            auto& proc = it->second;

            AssetCookerAPI cooker = CookerType::cooker();
            if (cooker.configure) {
                cooker.configure(this, options);
            }

            uint count = cooker.get_supported_extensions(nullptr);

            Vector<CString> exts(count);
            cooker.get_supported_extensions(exts.data());

            // add cooker to processor's list (stable pointers)
            proc->cookers.push_back(cooker);
            AssetCookerAPI* cooker_ptr = &proc->cookers.back();

            for (uint i = 0; i < count; ++i) {
                cooker_extensions.emplace(absl::AsciiStrToLower(exts[i]), cooker_ptr);
            }
        }

        /**
         * @brief Register a saver for an asset type.
         */
        template <typename AssetType>
        void register_saver(const JSON& options = {})
        {
            auto it = processors.find(AssetType::type);
            if (it == processors.end()) {
                spdlog::error("AssetType ({}) has not been registered! Please register the loader first.", AssetType::name);
                return;
            }

            auto& proc = it->second;

            AssetSaverAPI saver = AssetType::saver();
            if (saver.configure) {
                saver.configure(this, options);
            }

            uint count = saver.get_supported_extensions(nullptr);

            Vector<CString> exts(count);
            saver.get_supported_extensions(exts.data());

            proc->saver = saver;

            for (uint i = 0; i < count; ++i) {
                saver_extensions.emplace(absl::AsciiStrToLower(exts[i]), proc.get());
            }
        }

        /**
         * @brief Get the raw asset pointer. Returns nullptr if not loaded.
         */
        template <typename AssetType>
        auto get_asset(AssetHandle<AssetType> handle) -> AssetType*
        {
            return reinterpret_cast<AssetType*>(get_asset(AssetType::type, handle));
        }

        /**
         * @brief Load an asset and increment its handle reference count.
         */
        template <typename AssetType>
        auto load_asset(FSPath path) -> AssetHandle<AssetType>
        {
            auto handle = load_asset(AssetType::type, path);
            return AssetHandle<AssetType>{handle.guid};
        }

        /**
         * @brief Load an asset by its GUID and increment its handle reference count.
         */
        template <typename AssetType>
        auto load_asset(AssetID guid) -> AssetHandle<AssetType>
        {
            auto handle = load_asset(AssetType::type, guid);
            return AssetHandle<AssetType>{handle.guid};
        }

        /**
         * @brief Decrement the reference count for an asset handle.
         */
        template <typename AssetType>
        void unload_asset(AssetHandle<AssetType> handle)
        {
            unload_asset(AssetType::type, handle);
        }

        /**
         * @brief Increment the reference count for an asset handle.
         */
        template <typename AssetType>
        auto clone_asset(AssetHandle<AssetType> handle) -> AssetHandle<AssetType>
        {
            clone_asset(AssetType::type, handle);
            return handle;
        }

        /**
         * @brief Save a loaded asset by its handle to an OS filesystem path.
         */
        template <typename AssetType>
        bool save_asset(AssetHandle<AssetType> handle, OSPath path)
        {
            auto* asset = get_asset(handle);
            if (!asset) {
                spdlog::error("Cannot save asset: handle not loaded or invalid");
                return false;
            }
            return save_asset(AssetType::type, asset, path);
        }

        /**
         * @brief Save an in-memory asset to an OS filesystem path.
         */
        template <typename AssetType>
        bool save_asset(const AssetType& asset, OSPath path)
        {
            return save_asset(AssetType::type, &asset, path);
        }

        /**
         * @brief Get the GUID associated with an asset path.
         */
        AssetID get_guid(FSPath path) const;

        /**
         * @brief Preprocess asset into engine compatible format.
         */
        Future<AssetID> import_asset(const Path& path, bool force = false);

        /**
         * @brief Delete an asset (source file, .import metadata, cache) and unregister it.
         * @param path Asset path (can be relative to assets root or absolute).
         * @return True if deletion was successful.
         */
        bool delete_asset(const Path& path);

        /**
         * @brief Delete an asset by its GUID (source file, .import metadata, cache) and unregister it.
         * @param guid Asset GUID.
         * @return True if deletion was successful.
         */
        bool delete_asset(AssetID guid);

        /**
         * @brief Move or rename an asset (including source file and .import metadata) and update registry.
         * @param source_path Path to source asset (relative to assets root or absolute).
         * @param destination_path Path to destination asset or target directory.
         * @return True if move was successful.
         */
        bool move_asset(const Path& source_path, const Path& destination_path);

        /**
         * @brief Move or rename an asset by GUID (including source file and .import metadata) and update registry.
         * @param guid GUID of the asset to move.
         * @param destination_path Destination path (file or directory).
         * @return True if move was successful.
         */
        bool move_asset(AssetID guid, const Path& destination_path);

        /**
         * @brief Hot-reload an asset that is currently loaded in memory.
         */
        void reload_asset(AssetID guid);

        /**
         * @brief Re-import/cook all recognized assets found in the assets directory.
         */
        void reimport_all(bool force = false);

        /**
         * @brief Enable or disable the active directory watcher.
         */
        void set_watching(bool enable);

        /**
         * @brief Check whether the directory watcher is currently active.
         */
        bool is_watching() const;

        /**
         * @brief Get current pipeline cooking metrics and status.
         */
        AssetPipelineStats get_pipeline_stats() const;

        /**
         * @brief Check if an extension has a registered cooker.
         */
        bool has_cooker_for(const Path& path) const;

        /**
         * @brief Poll and dispatch queued pipeline events on the main thread.
         */
        void poll_events();

        /**
         * @brief Generate a preview/thumbnail for an asset from a preview scene and write to cache.
         * @return Future<Path> resolving to relative thumbnail path, or empty path on failure.
         */
        auto preview(const PreviewScene& scene, JSON& metadata) -> Future<Path>;

        /**
         * @brief Set callback for hot-reloaded assets.
         */
        void set_on_asset_reloaded(AssetReloadCallback callback);

        /**
         * @brief Set callback for external filesystem modifications.
         */
        void set_on_filesystem_changed(FileSystemChangeCallback callback);

    private:
        struct AssetRecord
        {
            void*              data   = nullptr;
            std::atomic<uint>  refcnt = 0;
        };

        struct AssetProcessor
        {
            String                         type_name;
            AssetTypeID                    type_id;
            AssetLoaderAPI                 loader;
            Optional<AssetSaverAPI>        saver;
            List<AssetCookerAPI>           cookers; // storage for cookers (stable pointers)
            Own<std::shared_mutex>         mutex;
            HashMap<AssetID, AssetRecord*> assets;

            AssetProcessor() : mutex(std::make_unique<std::shared_mutex>()) {}
            AssetProcessor(const AssetProcessor&)                      = delete;
            AssetProcessor(AssetProcessor&& other) noexcept            = default;
            AssetProcessor& operator=(const AssetProcessor&)           = delete;
            AssetProcessor& operator=(AssetProcessor&& other) noexcept = default;
        };

        auto get_asset(AssetTypeID type_id, RawAssetHandle handle) -> void*;
        auto load_asset(AssetTypeID type_id, FSPath path) -> RawAssetHandle;
        auto load_asset(AssetTypeID type_id, AssetID guid) -> RawAssetHandle;
        void unload_asset(AssetTypeID type_id, RawAssetHandle handle);
        void clone_asset(AssetTypeID type_id, RawAssetHandle handle);
        bool save_asset(AssetTypeID type_id, const void* asset, OSPath path);
        void handle_watch_events(const Vector<AssetWatchEvent>& events);
        void handle_watch_rename(const AssetWatchEvent& evt);

        // cooking & import helpers
        auto find_asset_type_for_cooker(const AssetCookerAPI* cooker) const -> AssetTypeID;
        bool is_cook_up_to_date(const Path& source_path, const Path& import_path) const;
        auto resolve_or_create_guid(const Path& rel_path, const Path& import_path) -> AssetID;
        bool execute_cooker(AssetCookerAPI* cooker, const Path& source_path, JSON& metadata);
        void commit_cooked_asset(const Path& import_path, const Path& rel_path, AssetID guid, AssetTypeID type_id, const JSON& metadata);
        auto cook_asset_task(AssetCookerAPI* cooker, const Path& source_path, const Path& import_path, const Path& rel_path, AssetID guid) -> AssetID;

        // dependency helpers
        void load_dependencies(AssetID guid);
        void unload_dependencies(AssetID guid);

        // deletion helpers
        auto resolve_asset_path(const Path& path) const -> std::pair<Path, Path>;
        void unload_record(AssetID guid);
        void delete_metadata_and_caches(const Path& import_path, AssetID guid);
        bool delete_directory_assets(const Path& dir_path);
        bool delete_single_asset(const Path& full_path, const Path& rel_path);

        // move helpers
        auto resolve_destination_path(const Path& src_full, const Path& destination) const -> std::pair<Path, Path>;
        bool move_directory_assets(const Path& src_full, const Path& src_rel, const Path& dst_full, const Path& dst_rel);
        bool move_single_asset(const Path& src_full, const Path& src_rel, const Path& dst_full, const Path& dst_rel);

    private:
        AMSDescriptor                             descriptor;
        BS::thread_pool<>                         pool;
        AssetRegistry                             registry;
        HashMap<AssetTypeID, Own<AssetProcessor>> processors;
        HashMap<String, AssetProcessor*>          saver_extensions;
        HashMap<String, AssetProcessor*>          loader_extensions;
        HashMap<String, AssetCookerAPI*>          cooker_extensions;

        Own<AssetWatcher>                       watcher;
        std::atomic<uint>                       pending_cooks{0};
        std::atomic<uint>                       completed_cooks{0};
        std::atomic<uint>                       failed_cooks{0};
        mutable std::mutex                      pipeline_mutex;
        String                                  current_cooking_asset;
        Deque<AssetWatchEvent>                  queued_fs_events;
        Vector<std::pair<AssetID, AssetTypeID>> queued_reloaded_assets;
        AssetReloadCallback                     on_asset_reloaded;
        FileSystemChangeCallback                on_fs_changed;
    };

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_AMSSERVER_H
