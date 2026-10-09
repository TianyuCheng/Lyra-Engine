#pragma once

#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/FileSystem/VFSTypes.h>

#include <physfs.h>

using namespace lyra;

namespace lyra::loader::physfs
{

    // custom deleter for PHYSFS_File
    struct PhysFSFileDeleter
    {
        void operator()(PHYSFS_File* f) const
        {
            if (f) PHYSFS_close(f);
        }
    };

    struct PhysMountPoint
    {
        String vpath;
        Path   root;
        uint   priority = 0;
        uint   mount_id = 0;
    };

    struct PhysFSLoader
    {
        Vector<PhysMountPoint*> mounts;
        std::mutex              mounts_mutex;
    };

    using PhysFSFilePtr = Own<PHYSFS_File, PhysFSFileDeleter>;

} // namespace lyra::loader::physfs
