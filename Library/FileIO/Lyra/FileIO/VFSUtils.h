#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_VFS_UTILS_H
#define LYRA_LIBRARY_PLUGIN_VFS_UTILS_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/FileIO/VFSEnums.h>

namespace lyra
{
    struct FSFile
    {
        // placeholder type
    };

    struct FSMount
    {
        // placeholder type
    };

    struct FileLoader;
    struct FilePacker;

    using FileHandle = TypedPointerHandle<FSFile>;

    using MountHandle = TypedPointerHandle<FSMount>;

    using FileLoaderHandle = TypedPointerHandle<FileLoader>;

    using FilePackerHandle = TypedPointerHandle<FilePacker>;

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_VFS_UTILS_H
