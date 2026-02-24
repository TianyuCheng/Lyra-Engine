#pragma once

#ifndef LYRA_LIBRARY_ASSETS_AMS_TYPES_H
#define LYRA_LIBRARY_ASSETS_AMS_TYPES_H

#include <memory>
#include <shared_mutex>

#include <Lyra/Common/UUID.h>
#include <Lyra/Common/GUID.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/FileIO/VFSEnums.h>
#include <Lyra/FileIO/VFSUtils.h>
#include <Lyra/Assets/AMSAPI.h>

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
         * @brief Register a new asset type.
         */
        template <typename AssetType>
        void register_asset(const JSON& options = {})
        {
            AssetProcessor info = {};
            info.type           = AssetType::name;
            info.handler        = AssetType::handler();
            info.assets         = {};

            if (info.handler->configure)
                info.handler->configure(options);

            processors.emplace(AssetType::uuid, std::move(info));

            for (const auto& extension : AssetType::extensions)
                extensions.emplace(extension, AssetType::uuid);
        }

        /**
         * @brief Configure an existing asset-type handler.
         */
        template <typename AssetType>
        void configure_asset(const JSON& options)
        {
            auto it = processors.find(AssetType::uuid);
            if (it == processors.end()) {
                spdlog::error("AssetType ({}) has not been registered!", AssetType::name);
                return;
            }

            auto& proc = it->second;
            if (proc.handler->configure) {
                proc.handler->configure(options);
            }
        }

        /**
         * @brief Get the raw asset pointer. Returns nullptr if not loaded.
         */
        template <typename AssetType>
        auto get_asset(AssetHandle<AssetType> handle) -> AssetType*
        {
            return reinterpret_cast<AssetType*>(get_asset(AssetType::uuid, handle));
        }

        /**
         * @brief Load an asset and increment its handle reference count.
         */
        template <typename AssetType>
        auto load_asset(FSPath path) -> AssetHandle<AssetType>
        {
            auto handle = load_asset(AssetType::uuid, path);
            return AssetHandle<AssetType>{handle.guid};
        }

        /**
         * @brief Decrement the reference count for an asset handle.
         */
        template <typename AssetType>
        void unload_asset(AssetHandle<AssetType> handle)
        {
            unload_asset(AssetType::uuid, handle);
        }

        /**
         * @brief Preproces asset into engine compatible format.
         */
        bool import_asset(const Path& path, GUID& guid);

        /**
         * @brief Purge assets with zero handle references.
         */
        void purge();

    private:
        struct AssetRecord
        {
            void* data   = nullptr;
            uint  refcnt = 0;
        };

        struct AssetProcessor
        {
            String                      type;
            AssetHandlerAPI*            handler;
            Own<std::shared_mutex>      mutex;
            HashMap<GUID, AssetRecord*> assets;

            AssetProcessor() : mutex(std::make_unique<std::shared_mutex>()) {}
            AssetProcessor(const AssetProcessor&)                      = delete;
            AssetProcessor(AssetProcessor&& other) noexcept            = default;
            AssetProcessor& operator=(const AssetProcessor&)           = delete;
            AssetProcessor& operator=(AssetProcessor&& other) noexcept = default;
        };

        auto get_asset(UUID type_uuid, RawAssetHandle handle) -> void*;
        auto load_asset(UUID type_uuid, FSPath path) -> RawAssetHandle;
        void unload_asset(UUID type_uuid, RawAssetHandle handle);

    private:
        AMSDescriptor                 descriptor;
        HashMap<String, UUID>         extensions;
        HashMap<UUID, AssetProcessor> processors;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_AMS_TYPES_H
