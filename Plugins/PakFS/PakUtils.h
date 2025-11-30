#pragma once

#ifndef LYRA_PLUGIN_PAK_UTILS_H
#define LYRA_PLUGIN_PAK_UTILS_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Collections.h>

using namespace lyra;

// PAK file format structures (little-endian)
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
};

auto get_logger() -> Logger;
auto normalize_pak_path(FSPath vpath) -> String;
void write_le32(std::ostream& os, uint value);
bool finalize_pak_archive(PakArchive& archive_data);

#endif // LYRA_PLUGIN_PAK_UTILS_H
