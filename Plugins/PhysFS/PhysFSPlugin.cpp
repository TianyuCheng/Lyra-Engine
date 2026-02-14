// system headers
#include <mutex>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <filesystem>

// library headers
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/FileIO/VFSTypes.h>

// plugin headers
#include "PhysFSUtils.h"

namespace fs = std::filesystem;

static Logger logger = create_logger("PhysFS", LogLevel::trace);

static inline Logger get_logger() { return logger; }

static Vector<FileLoaderHandle> g_loaders;

static bool sort_mount_points(PhysMountPoint* a, PhysMountPoint* b)
{
    if (a->priority != b->priority)
        return a->priority > b->priority;
    return a->vpath.size() > b->vpath.size(); // longer vpath first as tiebreaker
}

static String normalize_vpath(FSPath vpath)
{
    if (!vpath) return String("/");

    String s(vpath);
    if (s.empty()) return String("/");

    // replace backslashes with forward slashes to be consistent
    std::replace(s.begin(), s.end(), '\\', '/');
    if (s.front() != '/')
        s.insert(s.begin(), '/');

    // remove trailing '/'
    while (s.size() > 1 && s.back() == '/')
        s.pop_back();

    return s;
}

// rebuild the PhysicsFS search path based on current mounts
static void rebuild_mounts(PhysFSLoader* loader)
{
    // remove all existing entries from the search path
    char** search_path = PHYSFS_getSearchPath();
    if (search_path) {
        for (char** i = search_path; *i; ++i)
            PHYSFS_unmount(*i);
        PHYSFS_freeList(search_path);
    }

    std::lock_guard<std::mutex> lock(loader->mounts_mutex);

    // sort mounts by priority descending (higher priority first)
    Vector<PhysMountPoint*> sorted = loader->mounts;
    std::sort(sorted.begin(), sorted.end(), sort_mount_points);

    // mount each in order, appending to the end (higher prio earlier in search order)
    for (const auto& m : sorted) {
        if (!PHYSFS_mount(m->root.string().c_str(), m->vpath.c_str(), 1)) {
            get_logger()->error("rebuild_mounts: failed to mount {} -> {}: {}", m->vpath, m->root.string(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        } else {
            get_logger()->trace("rebuild_mounts: mounted {} -> {}", m->vpath, m->root.string());
        }
    }
}

// -----------------------------------------------------------------------------
// API functions
// -----------------------------------------------------------------------------

static CString get_api_name() { return "PhysFS"; }

static bool create_loader(FileLoaderHandle& loader)
{
    auto ptr       = new PhysFSLoader();
    loader.pointer = ptr;
    if (loader.valid()) {
        g_loaders.push_back(loader);
    }
    return loader.valid();
}

static bool delete_loader(FileLoaderHandle loader)
{
    if (!loader.valid())
        return false;

    // remove from loaders
    g_loaders.erase(
        std::remove(g_loaders.begin(), g_loaders.end(), loader),
        g_loaders.end());

    // delete pointer
    auto pointer = loader.astype<PhysFSLoader>();
    delete pointer;
    return true;
}

static size_t sizeof_file(FileLoaderHandle loader, FSPath path)
{
    if (!path) {
        get_logger()->error("sizeof_file: input path is null!");
        return false;
    }

    String v = normalize_vpath(path);

    PHYSFS_Stat stat;
    if (PHYSFS_stat(v.c_str(), &stat) && stat.filetype == PHYSFS_FILETYPE_REGULAR) {
        get_logger()->trace("sizeof_file: {} -> {} bytes", path, static_cast<unsigned long long>(stat.filesize));
        return static_cast<size_t>(stat.filesize);
    }
    return 0;
}

static bool exists_file(FileLoaderHandle loader, FSPath path)
{
    if (!path) {
        get_logger()->error("exists_file: input path is null!");
        return false;
    }

    String v = normalize_vpath(path);

    PHYSFS_Stat stat;
    return PHYSFS_stat(v.c_str(), &stat) && stat.filetype == PHYSFS_FILETYPE_REGULAR;
}

static bool open_file(FileLoaderHandle loader, FileHandle& out_handle, FSPath path)
{
    if (!path) {
        get_logger()->error("open_file: input path is null!");
        return false;
    }

    String v = normalize_vpath(path);

    PHYSFS_File* pf = PHYSFS_openRead(v.c_str());
    if (!pf) {
        get_logger()->error("open_file: failed to open {}: {}", v, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return false;
    }

    out_handle.pointer = pf;
    get_logger()->trace("open_file: {}", path);
    return true;
}

static void close_file(FileLoaderHandle loader, FileHandle handle)
{
    if (!handle.valid()) {
        get_logger()->error("close_file: input file handle is not valid!");
        return;
    }

    auto pf = handle.astype<PHYSFS_File>();
    PHYSFS_close(pf);
}

static bool read_file(FileLoaderHandle loader, FileHandle handle, void* buffer, size_t size, size_t& bytes_read)
{
    bytes_read = 0;

    if (!handle.valid() || !buffer || size == 0) {
        get_logger()->error("read_file: input file handle / buffer / size is invalid!");
        return false;
    }

    auto pf = handle.astype<PHYSFS_File>();

    PHYSFS_sint64 len = PHYSFS_readBytes(pf, buffer, static_cast<PHYSFS_uint64>(size));
    if (len < 0) {
        get_logger()->error("read_file: PhysicsFS error: {}", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return false;
    }
    bytes_read = static_cast<size_t>(len);
    return true;
}

static bool seek_file(FileLoaderHandle loader, FileHandle handle, int64_t offset)
{
    if (!handle.valid()) {
        get_logger()->error("seek_file: input file handle is invalid!");
        return false;
    }

    auto pf = handle.astype<PHYSFS_File>();

    return PHYSFS_seek(pf, static_cast<PHYSFS_uint64>(offset)) != 0;
}

static bool read_whole_file(FileLoaderHandle loader, FSPath path, void* data)
{
    if (!path || !data) {
        get_logger()->error("read_whole_file: input path or data is invalid!");
        return false;
    }

    size_t sz = sizeof_file(loader, path);
    if (sz == 0) return false; // not found or empty

    String       v  = normalize_vpath(path);
    PHYSFS_File* pf = PHYSFS_openRead(v.c_str());
    if (!pf) return false;

    PhysFSFilePtr guard(pf); // auto close
    PHYSFS_sint64 len = PHYSFS_readBytes(pf, data, static_cast<PHYSFS_uint64>(sz));
    if (len < 0 || static_cast<size_t>(len) != sz) return false;
    return true;
}

static bool mount(FileLoaderHandle loader, MountHandle& out_handle, FSPath vpath, OSPath path, uint priority)
{
    if (!path) {
        get_logger()->error("mount: input path is invalid!");
        return false;
    }

    auto pointer = loader.astype<PhysFSLoader>();

    std::error_code ec;

    auto     vp = normalize_vpath(vpath);
    fs::path root(path);
    root = fs::absolute(root, ec);
    if (ec) {
        get_logger()->error("mount {} -> {}: {}", vp, root.string(), ec.message());
        return false;
    }

    if (!fs::exists(root, ec)) {
        get_logger()->error("mount: root does not exist: {}", root.string());
        return false;
    }

    auto m      = new PhysMountPoint{};
    m->vpath    = vp;
    m->root     = root;
    m->priority = priority;

    // save to global mount points
    {
        std::lock_guard<std::mutex> lk(pointer->mounts_mutex);
        pointer->mounts.push_back(m);
    }
    rebuild_mounts(pointer);

    out_handle.pointer = m;
    get_logger()->info("mount: {} -> {} (prio {})", vp, root.string(), priority);
    return true;
}

static bool unmount(FileLoaderHandle loader, MountHandle handle)
{
    auto pointer = loader.astype<PhysFSLoader>();
    auto pmount  = handle.astype<PhysMountPoint>();
    get_logger()->info("unmount path={} from vpath={} (removed {} mounts)", pmount->root.string(), pmount->vpath);

    // lock while unmount
    std::lock_guard<std::mutex> lk(pointer->mounts_mutex);

    auto before = pointer->mounts.size();
    pointer->mounts.erase(
        std::remove_if(pointer->mounts.begin(), pointer->mounts.end(),
            [&](PhysMountPoint* m) { return m == handle.pointer; }),
        pointer->mounts.end());
    auto after = pointer->mounts.size();

    rebuild_mounts(pointer);
    return before != after;
}

// -----------------------------------------------------------------------------
// plugin exports
// -----------------------------------------------------------------------------

LYRA_EXPORT auto prepare() -> void
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_PHYSFS_VERBOSITY"));

    if (!PHYSFS_init(nullptr)) {
        get_logger()->error("prepare: PHYSFS_init failed: {}", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
}

LYRA_EXPORT auto cleanup() -> void
{
    for (auto& loader : g_loaders)
        delete_loader(loader);

    g_loaders.clear();

    PHYSFS_deinit();
}

LYRA_EXPORT auto create() -> FileLoaderAPI
{
    auto api            = FileLoaderAPI{};
    api.get_api_name    = get_api_name;
    api.create_loader   = create_loader;
    api.delete_loader   = delete_loader;
    api.sizeof_file     = sizeof_file;
    api.exists_file     = exists_file;
    api.open_file       = open_file;
    api.close_file      = close_file;
    api.read_file       = read_file;
    api.seek_file       = seek_file;
    api.read_whole_file = read_whole_file;
    api.mount           = mount;
    api.unmount         = unmount;
    return api;
}
