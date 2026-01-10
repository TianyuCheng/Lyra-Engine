// library headers
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Plugin/VFS/VFSAPI.h>

// plugin headers
#include "ZipUtils.h"

// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

static Vector<FilePackerHandle> g_packers;

// -----------------------------------------------------------------------------
// API functions
// -----------------------------------------------------------------------------

static CString get_api_name() { return "ZipFS"; }

static bool create_packer(FilePackerHandle& packer, OSPath path)
{
    if (!path) {
        get_logger()->error("create_packer: input path is null!");
        return false;
    }

    std::error_code ec;

    // convert to C++ path
    Path archive_path(path);

    // create parent directories if they don't exist
    Path parent = archive_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
        if (!std::filesystem::create_directories(parent, ec)) {
            get_logger()->error("create_packer: failed to create parent directories for {}: {}", fmt::ptr(path), ec.message());
            return false;
        }
    }

    auto* handle_data         = new ZipArchive();
    handle_data->archive_path = archive_path;

    packer.pointer = handle_data;
    g_packers.push_back(packer);
    get_logger()->info("create_packer: opened zip archive {} with handle {}", fmt::ptr(path), packer.pointer);
    return true;
}

static void delete_packer(FilePackerHandle packer)
{
    if (!packer.valid()) {
        get_logger()->error("delete_packer: input archive handle is not valid!");
        return;
    }

    auto* handle_data = static_cast<ZipArchive*>(packer.pointer);
    get_logger()->info("delete_packer: finalizing and closing zip archive with handle {}", packer.pointer);

    // finalize the archive before closing
    if (!finalize_zip_archive(*handle_data)) {
        get_logger()->error("delete_packer: failed to finalize zip archive");
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
        get_logger()->error("write: invalid parameters - handle valid: {}, path: {}, buffer: {}",
            packer.valid(), path != nullptr, buffer != nullptr);
        return false;
    }

    auto* archive_data = static_cast<ZipArchive*>(packer.pointer);
    if (archive_data->is_finalized) {
        get_logger()->error("write: cannot write to finalized archive (handle {})", packer.pointer);
        return false;
    }

    String normalized_path = normalize_zip_path(path);
    if (normalized_path.empty()) {
        get_logger()->error("write: normalized path is empty for input path {}", path);
        return false;
    }

    // check if file already exists and replace it
    auto existing = std::find_if(archive_data->files.begin(), archive_data->files.end(),
        [&normalized_path](const ZipFileEntry& entry) {
        return entry.filename == normalized_path;
    });

    if (existing != archive_data->files.end()) {
        get_logger()->warn("write: replacing existing file {} in ZIP", normalized_path);
        archive_data->files.erase(existing);
    }

    // add new file entry
    ZipFileEntry entry;
    entry.filename = normalized_path;
    entry.data.resize(size);
    std::memcpy(entry.data.data(), buffer, size);

    archive_data->files.push_back(std::move(entry));

    get_logger()->trace("write: added {} bytes to {} (total files: {})",
        size, normalized_path, archive_data->files.size());
    return true;
}

// -----------------------------------------------------------------------------
// plugin exports
// -----------------------------------------------------------------------------

LYRA_EXPORT auto prepare() -> void
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_ZIPFS_VERBOSITY"));
}

LYRA_EXPORT auto cleanup() -> void
{
    for (auto& packer : g_packers)
        delete_packer(packer);

    g_packers.clear();
}

LYRA_EXPORT auto create() -> FilePackerAPI
{
    auto api          = FilePackerAPI{};
    api.get_api_name  = get_api_name;
    api.create_packer = create_packer;
    api.delete_packer = delete_packer;
    api.write         = write;
    return api;
}
