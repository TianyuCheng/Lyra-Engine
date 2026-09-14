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

namespace lyra
{
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
            return AssetHandle<AssetType>{handle.uuid};
        }

        /**
         * @brief Load an asset by its GUID and increment its handle reference count.
         */
        template <typename AssetType>
        auto load_asset(AssetID guid) -> AssetHandle<AssetType>
        {
            auto handle = load_asset(AssetType::type, guid);
            return AssetHandle<AssetType>{handle.uuid};
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
            return save_asset_raw(AssetType::type, asset, path);
        }

        /**
         * @brief Save an in-memory asset to an OS filesystem path.
         */
        template <typename AssetType>
        bool save_asset(const AssetType& asset, OSPath path)
        {
            return save_asset_raw(AssetType::type, &asset, path);
        }

        /**
         * @brief Get the GUID associated with an asset path.
         */
        AssetID get_guid(FSPath path) const;

        /**
         * @brief Preproces asset into engine compatible format.
         */
        Future<AssetID> import_asset(const Path& path);

    private:
        struct AssetRecord
        {
            void*                 data   = nullptr;
            std::atomic<uint32_t> refcnt = 0;
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
        bool save_asset_raw(AssetTypeID type_id, const void* asset, OSPath path);

    private:
        AMSDescriptor                             descriptor;
        BS::thread_pool<>                         pool;
        AssetRegistry                             registry;
        HashMap<AssetTypeID, Own<AssetProcessor>> processors;
        HashMap<String, AssetProcessor*>          saver_extensions;
        HashMap<String, AssetProcessor*>          loader_extensions;
        HashMap<String, AssetCookerAPI*>          cooker_extensions;
    };

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_AMSSERVER_H
