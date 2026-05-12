#pragma once

#include <Lyra/Common/Path.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/FileIO/VFSTypes.h>

#include <physfs.h>

using namespace lyra;

namespace lyra::file_loader::physfs
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

} // namespace lyra::file_loader::physfs
