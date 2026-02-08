#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_AMS_TYPES_H
#define LYRA_LIBRARY_PLUGIN_AMS_TYPES_H

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
    struct AssetServer
    {
    public:
        explicit AssetServer(const AMSDescriptor& descriptor);

        virtual ~AssetServer();

        // AssetType needs to define the following:
        // 1. static constexpr CString name
        // 2. static constexpr UUID uuid
        // 3. static constexpr List<CString> extensions
        // 4. static AssetHandlerAPI handler();
        template <typename AssetType>
        void register_asset(const JSON& options = {})
        {
            AssetProcessor info = {};
            info.type           = AssetType::name;
            info.handler        = AssetType::handler();
            info.assets         = {};

            // configure handler
            if (info.handler->configure)
                info.handler->configure(options);

            // save asset-type specific processor
            processors.emplace(AssetType::uuid, info);

            // maintain mapping from file extension to asset-type uuid
            for (const auto& extension : AssetType::extensions)
                extensions.emplace(extension, AssetType::uuid);
        }

        // configure asset-type handler
        template <typename AssetType>
        void configure_asset(const JSON& options)
        {
            auto it = processors.find(AssetType::uuid);
            if (it == processors.end()) {
                spdlog::error("AssetType ({}) has not been registered!", to_string(it->second));
                return;
            }

            auto& proc = it->second;
            if (proc.handler->configure) {
                proc.handler->configure(options);
            }
        }

        // get the actual asset pointer, returns nullptr if asset is not ready
        template <typename AssetType>
        auto get_asset(AssetHandle<AssetType> handle) -> AssetType*
        {
            return reinterpret_cast<AssetType*>(get_asset(AssetType::uuid, handle));
        }

        // load the asset via asset path in virtual file system,
        // returns a typed asset handle (with uuid)
        template <typename AssetType>
        auto load_asset(FSPath path) -> AssetHandle<AssetType>
        {
            auto handle = load_asset(AssetType::uuid, path);
            return AssetHandle<AssetType>{handle.guid};
        }

        // unload the asset (if necessary)
        template <typename AssetType>
        void unload_asset(AssetHandle<AssetType> handle)
        {
            unload_asset(AssetType::uuid, handle);
        }

        // import the asset via asset path in actual file system,
        // returns a boolean indicating import status,
        // along with a typed asset handle (guid)
        bool import_asset(const Path& path, GUID& guid);

    private:
        auto get_asset(UUID type_uuid, RawAssetHandle handle) -> void*;
        auto load_asset(UUID type_uuid, FSPath path) -> RawAssetHandle;
        void unload_asset(UUID type_uuid, RawAssetHandle handle);

    private:
        struct AssetProcessor
        {
            String               type;
            AssetHandlerAPI*     handler;
            HashMap<GUID, void*> assets;
        };

        // descriptor
        AMSDescriptor descriptor;

        // mapping from extensions to asset type uuid
        HashMap<String, UUID> extensions;

        // mapping from asset type uuids to asset processor
        HashMap<UUID, AssetProcessor> processors;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_AMS_TYPES_H
