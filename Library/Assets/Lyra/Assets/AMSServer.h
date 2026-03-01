#pragma once

#ifndef LYRA_LIBRARY_ASSETS_AMS_SERVER_H
#define LYRA_LIBRARY_ASSETS_AMS_SERVER_H

#include <memory>
#include <atomic>
#include <shared_mutex>

#include <BS_thread_pool.hpp>
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
                loader_extensions.emplace(exts[i], proc_ptr);
            }

            // automatically register cooker if provided by the AssetType
            if constexpr (has_cooker<AssetType>::value) {
                register_asset<AssetType, AssetType>(options);
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
                cooker_extensions.emplace(exts[i], cooker_ptr);
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

    private:
        AMSDescriptor                             descriptor;
        BS::thread_pool<>                         pool;
        AssetRegistry                             registry;
        HashMap<AssetTypeID, Own<AssetProcessor>> processors;
        HashMap<String, AssetProcessor*>          loader_extensions;
        HashMap<String, AssetCookerAPI*>          cooker_extensions;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_AMS_SERVER_H
