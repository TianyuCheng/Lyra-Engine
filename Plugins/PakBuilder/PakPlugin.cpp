// library headers
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>

// plugin headers
#include "PakUtils.h"

using namespace lyra;
using namespace lyra::pakbuilder;

// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

static Vector<FilePackerHandle> g_packers;

// -----------------------------------------------------------------------------
// API functions
// -----------------------------------------------------------------------------

static CString get_api_name() { return "PakBuilder"; }

static bool create_packer(FilePackerHandle& packer, OSPath path)
{
    if (!path) {
        pakbuilder::get_logger()->error("create_packer: input path is null!");
        return false;
    }

    std::error_code ec;

    // convert to C++ path
    Path archive_path(path);

    // create parent directories if they don't exist
    Path parent = archive_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
        if (!std::filesystem::create_directories(parent, ec)) {
            pakbuilder::get_logger()->error("create_packer: failed to create parent directories for {}: {}", fmt::ptr(path), ec.message());
            return false;
        }
    }

    auto* handle_data         = new pakbuilder::PakArchive();
    handle_data->archive_path = archive_path;

    packer.pointer = handle_data;
    g_packers.push_back(packer);
    pakbuilder::get_logger()->info("create_packer: opened PAK archive {} with handle {}", fmt::ptr(path), packer.pointer);
    return true;
}

static void delete_packer(FilePackerHandle packer)
{
    if (!packer.valid()) {
        pakbuilder::get_logger()->error("delete_packer: input archive handle is not valid!");
        return;
    }

    auto* handle_data = static_cast<pakbuilder::PakArchive*>(packer.pointer);
    pakbuilder::get_logger()->info("delete_packer: finalizing and closing PAK archive with handle {}", packer.pointer);

    // finalize the archive before closing
    if (!handle_data->finalize()) {
        pakbuilder::get_logger()->error("delete_packer: failed to finalize PAK archive");
    }

    delete handle_data;

    // remove the packer from the global list
    auto it = std::find_if(g_packers.begin(), g_packers.end(),
        [packer](const FilePackerHandle& p) {
        return p.pointer == packer.pointer;
    });
    if (it != g_packers.end()) {
        g_packers.erase(it);
    }
}

static bool write(FilePackerHandle packer, FSPath path, void* buffer, size_t size)
{
    if (!packer.valid() || !path || !buffer) {
        pakbuilder::get_logger()->error("write: invalid parameters - handle valid: {}, path: {}, buffer: {}",
            packer.valid(), path != nullptr, buffer != nullptr);
        return false;
    }

    auto* archive_data = static_cast<pakbuilder::PakArchive*>(packer.pointer);
    if (archive_data->is_finalized) {
        pakbuilder::get_logger()->error("write: cannot write to finalized archive (handle {})", packer.pointer);
        return false;
    }

    String normalized_path = PakArchive::normalize_path(path);
    if (normalized_path.empty()) {
        pakbuilder::get_logger()->error("write: normalized path is empty for input path {}", path);
        return false;
    }

    // check if file already exists and replace it
    auto existing = std::find_if(archive_data->files.begin(), archive_data->files.end(),
        [&normalized_path](const pakbuilder::PakFileEntry& entry) {
        return entry.filename == normalized_path;
    });

    if (existing != archive_data->files.end()) {
        pakbuilder::get_logger()->warn("write: replacing existing file {} in PAK", normalized_path);
        archive_data->files.erase(existing);
    }

    // add new file entry
    pakbuilder::PakFileEntry entry;
    entry.filename = normalized_path;
    entry.data.resize(size);
    std::memcpy(entry.data.data(), buffer, size);

    archive_data->files.push_back(std::move(entry));

    pakbuilder::get_logger()->trace("write: added {} bytes to {} (total files: {})",
        size, normalized_path, archive_data->files.size());
    return true;
}

namespace lyra::pakbuilder
{

    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_PAKBUILDER_VERBOSITY"));
    }

    void cleanup()
    {
        for (auto& packer : g_packers) {
            auto pointer = static_cast<PakArchive*>(packer.pointer);
            delete pointer;
        }

        g_packers.clear();
    }

    FilePackerAPI create()
    {
        auto api          = FilePackerAPI{};
        api.get_api_name  = get_api_name;
        api.create_packer = create_packer;
        api.delete_packer = delete_packer;
        api.write         = write;
        return api;
    }

} // namespace lyra::pakbuilder
