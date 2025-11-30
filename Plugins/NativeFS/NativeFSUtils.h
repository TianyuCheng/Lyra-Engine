#pragma once

#ifndef LYRA_PLUGIN_NATIVE_FS_UTILS_H
#define LYRA_PLUGIN_NATIVE_FS_UTILS_H

#include <mutex>

#include <Lyra/Common/String.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Plugin/VFS/VFSAPI.h>

using namespace lyra;

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

#endif // LYRA_PLUGIN_NATIVE_FS_UTILS_H
