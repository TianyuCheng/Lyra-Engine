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

    struct RawAssetHandle
    {
        GUID guid;

        FORCE_INLINE bool valid() const { return guid != 0ull; }
    };

    template <typename AssetType>
    struct AssetHandle : RawAssetHandle
    {
        static constexpr UUID type_uuid = AssetType::uuid;
    };

    struct AMSImportDescriptor
    {
        // directory where user places assets
        OSPath assets_path;

        // directory where import metadata are generated
        OSPath metadata_path;

        // directory where asset caches are generated
        OSPath generated_path;
    };

    struct AMSLoaderDescriptor
    {
        // file loader for loading actual assets,
        // include both editor source files and generated caches,
        // generated caches should have higher priority while loading.
        FileLoader* assets;

        // file loader for loading metadata,
        // user could use the same loader as assets,
        // if a separate imported directory is mount on the asset loader.
        FileLoader* metadata;
    };

    struct AMSDescriptor
    {
        // development only
        AMSImportDescriptor importer;

        // both development + runtime
        AMSLoaderDescriptor loader;

        // watch asset directory changes
        bool watch = false;

        // number of threads for asset processing/loading
        uint workers = 1;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_AMS_UTILS_H
