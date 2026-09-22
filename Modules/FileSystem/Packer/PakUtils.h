#pragma once

#ifndef LYRA_MODULE_FILESYSTEM_PAK_BUILDER_UTILS_H
#define LYRA_MODULE_FILESYSTEM_PAK_BUILDER_UTILS_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Collections.h>

using namespace lyra;

namespace lyra::packer::pak
{

// pak file format structures (little-endian)
#pragma pack(push, 1)
    struct PakHeader
    {
        char signature[4]; // "PACK"
        uint dir_offset;   // offset to directory
        uint dir_size;     // size of directory
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct PakDirEntry
    {
        char filename[56]; // null-terminated filename (max 55 chars + null)
        uint offset;       // offset to file data
        uint size;         // size of file data
    };
#pragma pack(pop)

    struct PakFileEntry
    {
        String          filename;
        Vector<uint8_t> data;
    };

    struct PakArchive
    {
        Path                 archive_path;
        Vector<PakFileEntry> files;
        bool                 is_finalized;

        PakArchive() : is_finalized(false) {}

        bool finalize();

        static auto normalize_path(FSPath vpath) -> String;
        static void write_le32(std::ostream& os, uint value);
    };

    auto get_logger() -> Logger;

} // namespace lyra::packer::pak

#endif // LYRA_MODULE_FILESYSTEM_PAK_BUILDER_UTILS_H
