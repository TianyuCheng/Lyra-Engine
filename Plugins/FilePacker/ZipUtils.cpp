#include <algorithm>
#include <miniz.h>

#include "ZipUtils.h"

using namespace lyra;
using namespace lyra::file_packer::zip;

static Logger get_shared_logger()
{
    static Logger logger = create_logger("FilePacker", LogLevel::trace);
    return logger;
}

auto lyra::file_packer::zip::get_logger() -> Logger
{
    return get_shared_logger();
}

auto ZipArchive::normalize_path(FSPath vpath) -> String
{
    if (!vpath) return String("");

    String s(vpath);
    if (s.empty()) return String("");

    // replace backslashes with forward slashes to be consistent
    std::replace(s.begin(), s.end(), '\\', '/');

    // remove leading '/' for zip entries
    while (!s.empty() && s.front() == '/') {
        s.erase(0, 1);
    }

    // remove trailing '/'
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }

    return s;
}

bool ZipArchive::finalize()
{
    if (is_finalized) {
        return true; // already finalized
    }

    // initialize miniz zip writer
    mz_zip_archive zip_archive;
    std::memset(&zip_archive, 0, sizeof(zip_archive));

    // create the zip file
    if (!mz_zip_writer_init_file(&zip_archive, archive_path.string().c_str(), 0)) {
        get_logger()->error("finalize_zip_archive: failed to initialize zip writer for {}: {}",
            archive_path.string(), mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
        return false;
    }

    // add all files to the archive
    bool success = true;
    for (const auto& entry : files) {
        if (!mz_zip_writer_add_mem(&zip_archive, entry.filename.c_str(),
                entry.data.data(), entry.data.size(), MZ_BEST_COMPRESSION)) {
            get_logger()->error("finalize_zip_archive: failed to add file '{}' to archive: {}",
                entry.filename, mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
            success = false;
            break;
        }

        get_logger()->trace("finalize_zip_archive: added file '{}' ({} bytes)",
            entry.filename, entry.data.size());
    }

    // finalize the archive
    if (success) {
        if (!mz_zip_writer_finalize_archive(&zip_archive)) {
            get_logger()->error("finalize_zip_archive: failed to finalize archive: {}",
                mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
            success = false;
        }
    }

    // clean up
    if (!mz_zip_writer_end(&zip_archive)) {
        get_logger()->error("finalize_zip_archive: failed to end zip writer: {}",
            mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
        success = false;
    }

    if (success) {
        is_finalized = true;
        get_logger()->info("finalize_zip_archive: successfully wrote ZIP file {} with {} files",
            archive_path.string(), files.size());
    }

    return success;
}
