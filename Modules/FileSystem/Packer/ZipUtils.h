#pragma once
#ifndef LYRA_MODULE_FILESYSTEM_ZIP_BUILDER_UTILS_H
#define LYRA_MODULE_FILESYSTEM_ZIP_BUILDER_UTILS_H

#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Collections.h>

using namespace lyra;

namespace lyra::packer::zip
{
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

        bool finalize();

        static auto normalize_path(FSPath vpath) -> String;
    };

    auto get_logger() -> Logger;

} // namespace lyra::packer::zip

#endif // LYRA_MODULE_FILESYSTEM_ZIP_BUILDER_UTILS_H
