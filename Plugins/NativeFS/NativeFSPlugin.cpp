// system headers
#include <cstdint>
#include <fstream>
#include <algorithm>

// library headers
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>

// plugin headers
#include "NativeFSUtils.h"

using namespace lyra;
using namespace lyra::nativefs;

namespace fs = std::filesystem;

static Logger logger = create_logger("NativeFS", LogLevel::trace);

static inline Logger get_logger() { return logger; }

static Vector<FileLoaderHandle> g_loaders;

// strip a prefix from path if present. Returns true if stripped.
static bool strip_prefix(String& path, const String& prefix)
{
    // path and prefix are normalized with '/'
    if (path.size() < prefix.size())
        return false;

    if (path.compare(0, prefix.size(), prefix) != 0)
        return false;

    // either exact match or next char is '/'
    if (path.size() > prefix.size() && path[prefix.size()] != '/')
        return false;

    // strip prefix and optional '/'
    if (path.size() == prefix.size()) {
        path = ""; // requesting the mount root
    } else {
        path.erase(0, prefix.size());
        if (!path.empty() && path.front() == '/')
            path.erase(0, 1);
    }
    return true;
}

// normalize virtual path: ensure leading '/', strip trailing '/'
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

static bool sort_mount_points(NativeMount* a, NativeMount* b)
{
    // sort based on priority
    if (a->priority != b->priority)
        return a->priority > b->priority;

    // longer vpath first as tiebreaker
    return a->vpath.size() > b->vpath.size();
}

// resolve a VFS path to a list of real OS paths in search order (read side).
// if the incoming path is absolute on the OS, return it directly.
static Vector<fs::path> resolve_read_paths(NativeFSLoader* loader, FSPath cpath)
{
    Vector<fs::path> out;
    if (!cpath) return out;

    String path(cpath);
    if (path.empty()) return out;

    // normalize slashes
    std::replace(path.begin(), path.end(), '\\', '/');

    // absolute OS path? Let caller use it directly.
    fs::path probe(path);
    if (probe.is_absolute()) {
        out.push_back(probe);
        return out;
    }

    // sort mounts by priority (desc) every time only if needed; cheap for small N
    Vector<NativeMount*> mounts = loader->mounts;
    std::sort(mounts.begin(), mounts.end(), sort_mount_points);

    // check file path validity
    for (const auto& m : mounts) {
        String sub = path;
        if (!m->vpath.empty() && m->vpath != "/")
            if (!strip_prefix(sub, m->vpath))
                continue; // not under this mount

        fs::path real = m->root;
        if (!sub.empty())
            real /= fs::path(sub);
        out.push_back(real);
    }
    return out;
}

// -----------------------------------------------------------------------------
// API functions
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

static size_t sizeof_file(FileLoaderHandle loader, FSPath path)
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

static bool read_file(FileLoaderHandle loader, FileHandle handle, void* buffer, size_t size, size_t& bytes_read)
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
    bytes_read = static_cast<size_t>(h->gcount());
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

    auto     vp = normalize_vpath(vpath);
    fs::path root(path);
    root = fs::absolute(root, ec);
    if (ec) {
        get_logger()->error("mount {} -> {}: {}", vp, root.string(), ec.message());
        return false;
    }

    if (!fs::exists(root) || !fs::is_directory(root)) {
        get_logger()->error("mount: root not a directory: {}", root.string());
        return false;
    }

    auto pointer = loader.as_type<NativeFSLoader>();

    // prepare mount point
    auto m      = new NativeMount{};
    m->vpath    = vp;
    m->root     = root;
    m->priority = priority;

    // save this mount point, and sort according to priority
    std::lock_guard<std::mutex> lk(pointer->mounts_mutex);
    pointer->mounts.push_back(m);
    std::sort(pointer->mounts.begin(), pointer->mounts.end(), sort_mount_points);

    // assign pointer to handle
    handle.pointer = m;
    get_logger()->info("mount: {} -> {} (prio {})", vp, root.string(), priority);
    return true;
}

static bool unmount(FileLoaderHandle loader, MountHandle handle)
{
    auto pointer = loader.as_type<NativeFSLoader>();
    auto pmount  = handle.as_type<NativeMount>();
    get_logger()->info("unmount path={} from vpath={} (removed {} mounts)", pmount->root.string(), pmount->vpath);

    // lock while unmount
    std::lock_guard<std::mutex> lk(pointer->mounts_mutex);

    // remove mount point
    auto before = pointer->mounts.size();
    pointer->mounts.erase(
        std::remove_if(pointer->mounts.begin(), pointer->mounts.end(),
            [&](NativeMount* m) { return m == handle.pointer; }),
        pointer->mounts.end());
    auto after = pointer->mounts.size();
    return before != after;
}

namespace lyra::nativefs
{

    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_NATIVEFS_VERBOSITY"));
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

} // namespace lyra::nativefs
