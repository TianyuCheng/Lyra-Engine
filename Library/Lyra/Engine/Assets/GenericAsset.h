#pragma once

#ifndef LYRA_LIBRARY_ENGINE_ASSETS_GENERIC_ASSET_H
#define LYRA_LIBRARY_ENGINE_ASSETS_GENERIC_ASSET_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/Config.h>

namespace lyra
{
    struct AssetServer;

    // Dummy asset process implementation,
    // which does absolutely nothing except for saving the asset path in metadata.
    // This is suitable for simple assets that do not need an importing process.
    template <typename AssetType>
    JSON process_asset(AssetServer* manager, OSPath source_path, OSPath target_path)
    {
        MAYBE_UNUSED(manager);
        MAYBE_UNUSED(target_path);

        JSON metadata    = {};
        metadata["path"] = source_path;
        return metadata;
    }

    // Simple asset unload implementation, which only uses C++ pointer deletion.
    template <typename AssetType>
    void unload_asset(void* asset)
    {
        auto typed_asset = reinterpret_cast<AssetType*>(asset);
        delete typed_asset;
    }
} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_ASSETS_GENERIC_ASSET_H
