#pragma once

#ifndef LYRA_MODULE_FILESYSTEM_NATIVE_FS_UTILS_H
#define LYRA_MODULE_FILESYSTEM_NATIVE_FS_UTILS_H

#include <mutex>

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/FileSystem/VFSAPI.h>

using namespace lyra;

namespace lyra::loader::native
{

    struct NativeMount
    {
        String vpath;    // virtual mount prefix, e.g. "/textures" (no trailing slash)
        Path   root;     // real OS directory root
        uint   priority; // higher value = searched earlier
    };

    struct NativeFSLoader
    {
        Vector<NativeMount*> mounts;
        std::mutex           mounts_mutex;
    };

} // namespace lyra::loader::native

#endif // LYRA_MODULE_FILESYSTEM_NATIVE_FS_UTILS_H
