#pragma once

#ifndef LYRA_ENGINE_FILESYSTEM_VFSUTILS_H
#define LYRA_ENGINE_FILESYSTEM_VFSUTILS_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/FileSystem/VFSEnums.h>

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

#endif // LYRA_ENGINE_FILESYSTEM_VFSUTILS_H
