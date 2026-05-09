#pragma once

#ifndef LYRA_LYRA_FILEIO_VFSUTILS_H
#define LYRA_LYRA_FILEIO_VFSUTILS_H

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

#endif // LYRA_LYRA_FILEIO_VFSUTILS_H
