// system headers
#include <mutex>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <filesystem>

// library headers
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/FileSystem/VFSAPI.h>
#include <Lyra/FileSystem/VFSTypes.h>

// plugin headers
#include "PhysFSUtils.h"

using namespace lyra;
using namespace lyra::loader::physfs;

namespace fs = std::filesystem;

static Logger get_logger()
{
    static Logger logger = create_logger("Loader", LogLevel::trace);
    return logger;
}

static Vector<FileLoaderHandle> g_loaders;

static bool sort_mount_points(PhysMountPoint* a, PhysMountPoint* b)
{
    if (a->priority != b->priority)
        return a->priority > b->priority;
    return a->vprefix.path.size() > b->vprefix.path.size(); // longer subpath first as tiebreaker
}

static String to_physfs_path(FSPath vpath)
{
    if (!vpath) return "";
    VFSPath p = VFSPath::parse(vpath);
    if (!p.is_valid()) {
        get_logger()->error("to_physfs_path: invalid VFS path '{}'. Scheme is strictly required (e.g. 'sys://...' or 'app://...')", vpath);
        return "";
    }
    if (p.path.empty()) {
        return p.scheme;
    }
    return p.scheme + "/" + p.path;
}

static String to_physfs_mount_point(const VFSPath& vprefix)
{
    if (vprefix.path.empty()) {
        return vprefix.scheme;
    }
    return vprefix.scheme + "/" + vprefix.path;
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

    LockGuard lock(loader->mounts_mutex);

    // sort mounts by priority descending (higher priority first)
    Vector<PhysMountPoint*> sorted = loader->mounts;
    std::sort(sorted.begin(), sorted.end(), sort_mount_points);

    // mount each in order, appending to the end (higher prio earlier in search order)
    for (const auto& m : sorted) {
        String mount_point = to_physfs_mount_point(m->vprefix);
        if (!PHYSFS_mount(m->root.string().c_str(), mount_point.c_str(), 1)) {
            get_logger()->error("rebuild_mounts: failed to mount {} -> {}: {}", m->vprefix.to_string(), m->root.string(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        } else {
            get_logger()->trace("rebuild_mounts: mounted {} -> {}", m->vprefix.to_string(), m->root.string());
        }
    }
}

// -----------------------------------------------------------------------------
// api functions
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
    auto pointer = loader.as_type<PhysFSLoader>();
    delete pointer;
    return true;
}

static usize sizeof_file(FileLoaderHandle loader, FSPath path)
{
    if (!path) {
        get_logger()->error("sizeof_file: input path is null!");
        return 0;
    }

    String v = to_physfs_path(path);

    PHYSFS_Stat stat;
    if (PHYSFS_stat(v.c_str(), &stat) && stat.filetype == PHYSFS_FILETYPE_REGULAR) {
        get_logger()->trace("sizeof_file: {} -> {} bytes", path, static_cast<unsigned long long>(stat.filesize));
        return static_cast<usize>(stat.filesize);
    }
    return 0;
}

static bool exists_file(FileLoaderHandle loader, FSPath path)
{
    if (!path) {
        get_logger()->error("exists_file: input path is null!");
        return false;
    }

    String v = to_physfs_path(path);

    PHYSFS_Stat stat;
    return PHYSFS_stat(v.c_str(), &stat) && stat.filetype == PHYSFS_FILETYPE_REGULAR;
}

static bool open_file(FileLoaderHandle loader, FileHandle& out_handle, FSPath path)
{
    if (!path) {
        get_logger()->error("open_file: input path is null!");
        return false;
    }

    String v = to_physfs_path(path);

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

    auto pf = handle.as_type<PHYSFS_File>();
    PHYSFS_close(pf);
}

static bool read_file(FileLoaderHandle loader, FileHandle handle, void* buffer, usize size, usize& bytes_read)
{
    bytes_read = 0;

    if (!handle.valid() || !buffer || size == 0) {
        get_logger()->error("read_file: input file handle / buffer / size is invalid!");
        return false;
    }

    auto pf = handle.as_type<PHYSFS_File>();

    PHYSFS_sint64 len = PHYSFS_readBytes(pf, buffer, static_cast<PHYSFS_uint64>(size));
    if (len < 0) {
        get_logger()->error("read_file: PhysicsFS error: {}", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return false;
    }
    bytes_read = static_cast<usize>(len);
    return true;
}

static bool seek_file(FileLoaderHandle loader, FileHandle handle, int64_t offset)
{
    if (!handle.valid()) {
        get_logger()->error("seek_file: input file handle is invalid!");
        return false;
    }

    auto pf = handle.as_type<PHYSFS_File>();

    return PHYSFS_seek(pf, static_cast<PHYSFS_uint64>(offset)) != 0;
}

static bool read_whole_file(FileLoaderHandle loader, FSPath path, void* data)
{
    if (!path || !data) {
        get_logger()->error("read_whole_file: input path or data is invalid!");
        return false;
    }

    usize sz = sizeof_file(loader, path);
    if (sz == 0) return false; // not found or empty

    String       v  = to_physfs_path(path);
    PHYSFS_File* pf = PHYSFS_openRead(v.c_str());
    if (!pf) return false;

    PhysFSFilePtr guard(pf); // auto close
    PHYSFS_sint64 len = PHYSFS_readBytes(pf, data, static_cast<PHYSFS_uint64>(sz));
    if (len < 0 || static_cast<usize>(len) != sz) return false;
    return true;
}

static bool mount(FileLoaderHandle loader, MountHandle& out_handle, FSPath vpath, OSPath path, uint priority)
{
    if (!path) {
        get_logger()->error("mount: input path is invalid!");
        return false;
    }

    auto pointer = loader.as_type<PhysFSLoader>();

    std::error_code ec;

    auto vprefix = VFSPath::parse(vpath ? vpath : "");
    if (!vprefix.is_valid()) {
        get_logger()->error("mount: invalid VFS path '{}'. Scheme is strictly required (e.g. 'sys://' or 'app://')", vpath ? vpath : "");
        return false;
    }

    fs::path root(path);
    root = fs::absolute(root, ec);
    if (ec) {
        get_logger()->error("mount {} -> {}: {}", vprefix.to_string(), root.string(), ec.message());
        return false;
    }

    if (!fs::exists(root, ec)) {
        get_logger()->error("mount: root does not exist: {}", root.string());
        return false;
    }

    auto m      = new PhysMountPoint{};
    m->vprefix  = vprefix;
    m->root     = root;
    m->priority = priority;

    // save to global mount points
    {
        LockGuard lk(pointer->mounts_mutex);
        pointer->mounts.push_back(m);
    }
    rebuild_mounts(pointer);

    out_handle.pointer = m;
    get_logger()->info("mount: {} -> {} (prio {})", vprefix.to_string(), root.string(), priority);
    return true;
}

static bool unmount(FileLoaderHandle loader, MountHandle handle)
{
    auto pointer = loader.as_type<PhysFSLoader>();
    auto pmount  = handle.as_type<PhysMountPoint>();
    get_logger()->info("unmount path={} from vpath={}", pmount->root.string(), pmount->vprefix.to_string());

    // lock while unmount
    LockGuard lk(pointer->mounts_mutex);

    auto before = pointer->mounts.size();
    pointer->mounts.erase(
        std::remove_if(pointer->mounts.begin(), pointer->mounts.end(),
            [&](PhysMountPoint* m) {
                return m == handle.pointer;
            }),
        pointer->mounts.end());
    auto after = pointer->mounts.size();

    rebuild_mounts(pointer);
    return before != after;
}

namespace lyra::loader::physfs
{

    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_LOADER_VERBOSITY"));

        if (!PHYSFS_init(nullptr)) {
            get_logger()->error("prepare: PHYSFS_init failed: {}", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        }
    }

    void cleanup()
    {
        for (auto& loader : g_loaders) {
            auto pointer = loader.as_type<PhysFSLoader>();
            delete pointer;
        }

        g_loaders.clear();

        PHYSFS_deinit();
    }

    auto create() -> FileLoaderAPI
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

} // namespace lyra::loader::physfs
