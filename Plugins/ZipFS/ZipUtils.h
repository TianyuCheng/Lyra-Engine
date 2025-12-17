#pragma once
#ifndef LYRA_PLUGIN_ZIP_UTILS_H
#define LYRA_PLUGIN_ZIP_UTILS_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Collections.h>

using namespace lyra;

struct ZipFileEntry
{
    String          filename;
    Vector<uint8_t> data;
};

struct ZipArchive
{
    Path                 archive_path;
    Vector<ZipFileEntry> files;
    bool                 is_finalized;

    ZipArchive() : is_finalized(false) {}
};

auto get_logger() -> Logger;
auto normalize_zip_path(FSPath vpath) -> String;
bool finalize_zip_archive(ZipArchive& archive_data);

#endif // LYRA_PLUGIN_ZIP_UTILS_H
