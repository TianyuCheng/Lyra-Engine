#pragma once
#ifndef LYRA_PLUGIN_ZIP_BUILDER_UTILS_H
#define LYRA_PLUGIN_ZIP_BUILDER_UTILS_H

#include <Lyra/Common/Path.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Collections.h>

using namespace lyra;

namespace lyra::zipbuilder
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

} // namespace lyra::zipbuilder

#endif // LYRA_PLUGIN_ZIP_BUILDER_UTILS_H
