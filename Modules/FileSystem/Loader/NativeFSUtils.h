#pragma once

#ifndef LYRA_MODULE_FILESYSTEM_NATIVE_FS_UTILS_H
#define LYRA_MODULE_FILESYSTEM_NATIVE_FS_UTILS_H

#include <Lyra/Utilities/Sync.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/FileSystem/VFSAPI.h>
#include <Lyra/FileSystem/VFSPath.h>

using namespace lyra;

namespace lyra::loader::native
{

    struct NativeMount
    {
        VFSPath vprefix;  // parsed virtual mount prefix, e.g. sys:// or app://
        Path    root;     // real OS directory root
        uint    priority; // higher value = searched earlier
    };

    struct NativeFSLoader
    {
        Vector<NativeMount*> mounts;
        Mutex                mounts_mutex;
    };

} // namespace lyra::loader::native

#endif // LYRA_MODULE_FILESYSTEM_NATIVE_FS_UTILS_H
