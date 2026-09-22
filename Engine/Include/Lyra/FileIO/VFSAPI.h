#pragma once

#ifndef LYRA_LYRA_FILEIO_VFSAPI_H
#define LYRA_LYRA_FILEIO_VFSAPI_H

#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/FileIO/VFSEnums.h>
#include <Lyra/FileIO/VFSUtils.h>

namespace lyra
{
    struct FileLoaderAPI
    {
        // api name
        CString (*get_api_name)();

        // create a loader and return handle
        bool (*create_loader)(FileLoaderHandle& loader);
        bool (*delete_loader)(FileLoaderHandle loader);

        // file query operations
        auto (*sizeof_file)(FileLoaderHandle loader, FSPath path) -> size_t;
        bool (*exists_file)(FileLoaderHandle loader, FSPath path);

        // file open/close operations
        bool (*open_file)(FileLoaderHandle loader, FileHandle& handle, FSPath path);
        void (*close_file)(FileLoaderHandle, FileHandle handle);

        // file read operations
        bool (*read_file)(FileLoaderHandle loader, FileHandle handle, void* buffer, size_t size, size_t& bytes_read);
        bool (*seek_file)(FileLoaderHandle loader, FileHandle handle, int64_t offset);
        bool (*read_whole_file)(FileLoaderHandle loader, FSPath path, void* data);

        // mount operations
        bool (*mount)(FileLoaderHandle loader, MountHandle& handle, FSPath vpath, OSPath path, uint priority);
        bool (*unmount)(FileLoaderHandle loader, MountHandle handle);
    };

    struct FilePackerAPI
    {
        // api name
        CString (*get_api_name)();

        // initialize with target archive path
        bool (*create_packer)(FilePackerHandle& packer, OSPath path);
        void (*delete_packer)(FilePackerHandle packer);

        // write binary to virtual file system within the archive
        bool (*write)(FilePackerHandle packer, FSPath path, void* buffer, size_t size);
    };

} // namespace lyra

#endif // LYRA_LYRA_FILEIO_VFSAPI_H
