// system headers
#include <cstdint>
#include <fstream>
#include <algorithm>

// library headers
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/FileSystem/VFSAPI.h>

// plugin headers
#include "NativeFSUtils.h"

using namespace lyra;
using namespace lyra::loader::native;

namespace fs = std::filesystem;

static Logger get_logger()
{
    static Logger logger = create_logger("Loader", LogLevel::trace);
    return logger;
}

static Vector<FileLoaderHandle> g_loaders;

static bool sort_mount_points(NativeMount* a, NativeMount* b)
{
    // sort based on priority
    if (a->priority != b->priority)
        return a->priority > b->priority;

    // longer subpath first as tiebreaker
    return a->vprefix.path.size() > b->vprefix.path.size();
}

// resolve a VFS path to a list of real OS paths in search order (read side).
static Vector<fs::path> resolve_read_paths(NativeFSLoader* loader, FSPath cpath)
{
    Vector<fs::path> out;
    if (!cpath || !loader) return out;

    StringView raw(cpath);
    if (raw.empty()) return out;

    VFSPath target = VFSPath::parse(raw);
    if (!target.is_valid()) {
        get_logger()->error("resolve_read_paths: invalid VFS path '{}'. Scheme is strictly required (e.g. 'sys://...' or 'app://...')", raw);
        return out;
    }

    LockGuard            lk(loader->mounts_mutex);
    Vector<NativeMount*> mounts = loader->mounts;
    std::sort(mounts.begin(), mounts.end(), sort_mount_points);

    for (const auto& m : mounts) {
        if (m->vprefix.scheme != target.scheme) {
            continue; // different protocol domain (e.g. sys vs app)
        }

        String sub = target.path;
        if (!m->vprefix.path.empty()) {
            if (sub == m->vprefix.path) {
                sub = "";
            } else if (sub.size() > m->vprefix.path.size() &&
                       sub.compare(0, m->vprefix.path.size(), m->vprefix.path) == 0 &&
                       sub[m->vprefix.path.size()] == '/') {
                sub.erase(0, m->vprefix.path.size() + 1);
            } else {
                continue; // does not match mount subpath
            }
        }

        fs::path real = m->root;
        if (!sub.empty())
            real /= fs::path(sub);
        out.push_back(real);
    }

    return out;
}

// -----------------------------------------------------------------------------
// api functions
// -----------------------------------------------------------------------------

static CString get_api_name()
{
    return "NativeFS";
}

static bool create_loader(FileLoaderHandle& loader)
{
    auto ptr       = new NativeFSLoader();
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
    auto pointer = loader.as_type<NativeFSLoader>();
    delete pointer;
    return true;
}

static usize sizeof_file(FileLoaderHandle loader, FSPath path)
{
    if (!path) {
        get_logger()->error("sizeof_file: input path is null!");
        return 0;
    }

    auto candidates = resolve_read_paths(loader.as_type<NativeFSLoader>(), path);
    for (const auto& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
            auto sz = fs::file_size(p, ec);
            if (!ec) {
                get_logger()->trace("sizeof_file: {} -> {} bytes", path, static_cast<unsigned long long>(sz));
                return sz;
            }
        }
    }
    return 0;
}

static bool exists_file(FileLoaderHandle loader, FSPath path)
{
    if (!path) {
        get_logger()->error("exists_file: input path is null!");
        return false;
    }

    auto candidates = resolve_read_paths(loader.as_type<NativeFSLoader>(), path);
    for (const auto& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec) && fs::is_regular_file(p, ec))
            return true;
    }
    return false;
}

static bool open_file(FileLoaderHandle loader, FileHandle& out_handle, FSPath path)
{
    if (!path) {
        get_logger()->error("open_file: input path is null!");
        return false;
    }

    auto pointer = loader.as_type<NativeFSLoader>();

    // find and open the corresponding file
    auto candidates = resolve_read_paths(pointer, path);
    for (const auto& real : candidates) {
        auto h = new std::ifstream(real, std::ios::binary);
        if (h->is_open()) {
            out_handle.pointer = h;
            get_logger()->trace("open_file: {}", real.string());
            return true;
        }
    }
    get_logger()->error("open_file: not found: {}", path);
    return false;
}

static void close_file(FileLoaderHandle loader, FileHandle handle)
{
    if (!handle.valid()) {
        get_logger()->error("close_file: input file handle is invalid!");
        return;
    }

    auto h = handle.as_type<std::ifstream>();
    h->close();
    delete h;
}

static bool read_file(FileLoaderHandle loader, FileHandle handle, void* buffer, usize size, usize& bytes_read)
{
    bytes_read = 0;

    if (!handle.valid() || !buffer || size == 0) {
        get_logger()->error("read_file: input file handle / buffer / size is invalid!");
        return false;
    }

    auto h = handle.as_type<std::ifstream>();
    if (!h->is_open()) {
        get_logger()->error("read_file: file handle is not opened for read");
        return false;
    }

    // read bytes
    h->read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
    bytes_read = static_cast<usize>(h->gcount());
    return bytes_read > 0 || h->eof();
}

static bool seek_file(FileLoaderHandle loader, FileHandle handle, int64_t offset)
{
    if (!handle.valid()) {
        get_logger()->error("seek_file: input file handle is invalid!");
        return false;
    }

    auto h = handle.as_type<std::ifstream>();
    if (!h->is_open()) {
        get_logger()->error("read_file: file handle is not opened for read");
        return false;
    }

    // seek position
    h->seekg(offset, std::ios::beg);
    if (h->fail()) return false;
    return true;
}

static bool read_whole_file(FileLoaderHandle loader, FSPath path, void* data)
{
    if (!path || !data) {
        get_logger()->error("read_whole_file: input path or data is invalid!");
        return false;
    }

    auto pointer    = loader.as_type<NativeFSLoader>();
    auto candidates = resolve_read_paths(pointer, path);
    for (const auto& real : candidates) {
        std::ifstream in(real, std::ios::binary);
        if (!in.is_open()) continue;
        in.seekg(0, std::ios::end);
        std::streamsize sz = in.tellg();
        in.seekg(0, std::ios::beg);
        if (sz <= 0) continue;
        in.read(static_cast<char*>(data), sz);
        return in.good() || in.eof();
    }
    return false;
}

static bool mount(FileLoaderHandle loader, MountHandle& handle, FSPath vpath, OSPath path, uint priority)
{
    if (!path) {
        get_logger()->error("mount: input path is invalid!");
        return false;
    }

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

    if (!fs::exists(root) || !fs::is_directory(root)) {
        get_logger()->error("mount: root not a directory: {}", root.string());
        return false;
    }

    auto pointer = loader.as_type<NativeFSLoader>();

    // prepare mount point
    auto m      = new NativeMount{};
    m->vprefix  = vprefix;
    m->root     = root;
    m->priority = priority;

    // save this mount point, and sort according to priority
    LockGuard lk(pointer->mounts_mutex);
    pointer->mounts.push_back(m);
    std::sort(pointer->mounts.begin(), pointer->mounts.end(), sort_mount_points);

    // assign pointer to handle
    handle.pointer = m;
    get_logger()->info("mount: {} -> {} (prio {})", vprefix.to_string(), root.string(), priority);
    return true;
}

static bool unmount(FileLoaderHandle loader, MountHandle handle)
{
    auto pointer = loader.as_type<NativeFSLoader>();
    auto pmount  = handle.as_type<NativeMount>();
    get_logger()->info("unmount path={} from vpath={}", pmount->root.string(), pmount->vprefix.to_string());

    // lock while unmount
    LockGuard lk(pointer->mounts_mutex);

    // remove mount point
    auto before = pointer->mounts.size();
    pointer->mounts.erase(
        std::remove_if(pointer->mounts.begin(), pointer->mounts.end(),
            [&](NativeMount* m) {
                return m == handle.pointer;
            }),
        pointer->mounts.end());
    auto after = pointer->mounts.size();
    return before != after;
}

namespace lyra::loader::native
{

    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_LOADER_VERBOSITY"));
    }

    void cleanup()
    {
        for (auto& loader : g_loaders) {
            auto pointer = loader.as_type<NativeFSLoader>();
            delete pointer;
        }

        g_loaders.clear();
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

} // namespace lyra::loader::native
