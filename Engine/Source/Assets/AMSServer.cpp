#include <ctime>
#include <mutex>
#include <fstream>
#include <cassert>
#include <filesystem>

#include <absl/strings/ascii.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/FileSystem/VFSPath.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/AMSPreview.h>
#include <Lyra/Assets/Format/MeshAsset.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

using namespace lyra;

static Path get_metadata_path(Path path)
{
    path += ".import";
    return path;
}

static time_t get_timestamp()
{
    time_t timestamp;
    time(&timestamp);
    return timestamp;
}

static auto load_json_file(const Path& path) -> Optional<JSON>
{
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return std::nullopt;
    }
    std::ifstream sf(path);
    if (!sf.good() || sf.peek() == std::ifstream::traits_type::eof()) {
        return std::nullopt;
    }
    try {
        return JSON::parse(sf);
    } catch (...) {
        return std::nullopt;
    }
}

static AssetID load_guid(const Path& path)
{
    auto json = load_json_file(path);
    if (json && json->contains("guid") && (*json)["guid"].is_number()) {
        return (*json)["guid"].get<AssetID>();
    }
    return 0;
}

static void save_json(const Path& path, const JSON& data, int indent = 2)
{
    std::ofstream f(path, std::ios::out);
    if (!f.good()) {
        spdlog::error("Failed to open file for writing: {}", path.string());
        return;
    }
    f << data.dump(indent);
    f.close();
}

static time_t load_metadata_time(const Path& path)
{
    auto json = load_json_file(path);
    if (json && json->contains("time") && (*json)["time"].is_number()) {
        return (*json)["time"].get<time_t>();
    }
    return 0;
}

static time_t get_file_mtime(const Path& path)
{
    std::error_code ec;
    auto            ftime = fs::last_write_time(path, ec);
    if (ec) return 0;
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    return std::chrono::system_clock::to_time_t(sctp);
#else
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
#endif
}

static OSPath get_assets_path(const AMSDescriptor& desc, StringView scheme)
{
    if (scheme == vfs::SchemeSys) return desc.sys.assets_path;
    if (scheme == vfs::SchemeApp) return desc.app.assets_path;
    return nullptr;
}

static OSPath get_caches_path(const AMSDescriptor& desc, StringView scheme)
{
    if (scheme == vfs::SchemeSys) return desc.sys.caches_path;
    if (scheme == vfs::SchemeApp) return desc.app.caches_path;
    return nullptr;
}

static void strip_import_extension(Path& full_path, Path& rel_path)
{
    if (full_path.extension() == ".import") {
        full_path.replace_extension("");
        if (rel_path.extension() == ".import") {
            rel_path.replace_extension("");
        }
    }
}

static bool has_missing_cooked_paths(const AssetRegistry& registry)
{
    for (const auto& entry : registry.get_entries()) {
        if (entry.cooked_path != 0xFFFFFFFF) continue;
        StringView p = registry.get_path(entry.guid);
        if (p.find('#') != StringView::npos ||
            p.ends_with(".stl") || p.ends_with(".glb") || p.ends_with(".gltf") || p.ends_with(".obj")) {
            return true;
        }
    }
    return false;
}

static void initialize_registry(AssetRegistry& registry, const AMSDescriptor& desc)
{
    AssetRegistry::RebuildDesc rebuild_desc{
        .sys = desc.sys.assets_path,
        .app = desc.app.assets_path,
    };

    if (!desc.registry) {
        registry.rebuild(rebuild_desc);
        return;
    }

    if (!registry.load(desc.registry)) {
        spdlog::info("AssetRegistry {} not found or failed to load. Rebuilding from source...", Path(desc.registry).string());
        registry.rebuild(rebuild_desc);
        registry.save(desc.registry);
        return;
    }

    if (desc.sys.assets_path) {
        registry.scan(desc.sys.assets_path, vfs::SchemeSys);
    }

    if (desc.app.assets_path && has_missing_cooked_paths(registry)) {
        spdlog::info("AssetRegistry {} contains entries without cooked paths. Rebuilding from source...", Path(desc.registry).string());
        registry.rebuild(rebuild_desc);
        registry.save(desc.registry);
    }
}

static void register_cooked_dependencies(
    const JSON&       metadata,
    const Path&       rel_path,
    StringView        scheme,
    AssetRegistry&    registry,
    Vector<AssetID>&  out_dep_guids,
    HashSet<AssetID>& out_current_guids)
{
    if (!metadata.contains("dependencies") || !metadata["dependencies"].is_array()) {
        return;
    }

    for (const auto& item : metadata["dependencies"]) {
        if (item.is_number()) {
            AssetID dep_guid = item.get<AssetID>();
            out_dep_guids.push_back(dep_guid);
            out_current_guids.insert(dep_guid);
            continue;
        }

        if (!item.is_object() || !item.contains("guid")) {
            continue;
        }

        AssetID dep_guid = item["guid"].get<AssetID>();
        out_dep_guids.push_back(dep_guid);
        out_current_guids.insert(dep_guid);

        if (!item.contains("name") || !item.contains("type")) {
            continue;
        }

        String      dep_name = item["name"].get<String>();
        AssetTypeID dep_type{};
        if (item["type"].is_string()) {
            dep_type = parse_uuid(item["type"].get<String>());
        } else if (item["type"].is_number()) {
            dep_type = AssetTypeID(item["type"].get<ulong>());
        }

        String dep_vpath = rel_path.generic_string() + "#" + dep_name;
        String dep_cooked_path;
        if (item.contains("path") && item["path"].is_string()) {
            String raw_dep  = item["path"].get<String>();
            dep_cooked_path = VFSPath::has_protocol(raw_dep) ? raw_dep : VFSPath(scheme, raw_dep).to_string();
        }

        registry.update(dep_guid, dep_vpath, dep_type, {}, dep_cooked_path);
    }
}

static void cleanup_orphaned_dependencies(
    const Optional<JSON>&      prev_meta,
    const HashSet<AssetID>&    current_guids,
    OSPath                     caches_path,
    AssetRegistry&             registry,
    FunctionRef<void(AssetID)> unload_fn)
{
    if (!prev_meta || !prev_meta->contains("dependencies") || !(*prev_meta)["dependencies"].is_array()) {
        return;
    }

    std::error_code ec;
    for (const auto& item : (*prev_meta)["dependencies"]) {
        if (!item.is_object() || !item.contains("guid")) {
            continue;
        }
        AssetID prev_guid = item["guid"].get<AssetID>();
        if (current_guids.contains(prev_guid)) {
            continue;
        }

        if (caches_path && item.contains("path") && item["path"].is_string()) {
            Path old_cache = Path(caches_path) / item["path"].get<String>();
            fs::remove(old_cache, ec);
        }
        unload_fn(prev_guid);
        registry.remove(prev_guid);
    }
}

static void remove_metadata_cache_files(
    const JSON&                meta,
    OSPath                     caches_path,
    AssetRegistry&             registry,
    FunctionRef<void(AssetID)> unload_fn)
{
    if (!caches_path) return;

    std::error_code ec;
    if (meta.contains("thumbnail") && meta["thumbnail"].is_string()) {
        fs::remove(Path(caches_path) / meta["thumbnail"].get<String>(), ec);
    }
    if (meta.contains("path") && meta["path"].is_string()) {
        fs::remove(Path(caches_path) / meta["path"].get<String>(), ec);
    }
    if (!meta.contains("dependencies") || !meta["dependencies"].is_array()) {
        return;
    }

    for (const auto& dep : meta["dependencies"]) {
        if (!dep.is_object() || !dep.contains("guid")) continue;
        AssetID dep_guid = dep["guid"].get<AssetID>();
        if (dep.contains("path") && dep["path"].is_string()) {
            fs::remove(Path(caches_path) / dep["path"].get<String>(), ec);
        }
        unload_fn(dep_guid);
        registry.remove(dep_guid);
    }
}

static auto try_resolve_from_sidecar(
    const VFSPath& vp,
    const String&  raw_path,
    AssetTypeID    type_id,
    AssetID        guid,
    OSPath         assets_path,
    AssetRegistry& registry) -> Optional<String>
{
    if (!assets_path) return std::nullopt;

    auto   hash_pos  = vp.path.find('#');
    String container = (hash_pos != String::npos) ? vp.path.substr(0, hash_pos) : vp.path;
    Path   import_p  = Path(assets_path) / (container + ".import");

    auto meta = load_json_file(import_p);
    if (!meta) return std::nullopt;

    // check container asset cooked path
    if (hash_pos == String::npos && meta->contains("path") && (*meta)["path"].is_string()) {
        String cooked_path      = (*meta)["path"].get<String>();
        String canonical_cooked = VFSPath::has_protocol(cooked_path) ? cooked_path : VFSPath(vp.scheme, cooked_path).to_string();
        registry.update(guid, raw_path, type_id, registry.get_dependencies(guid), canonical_cooked);
        return canonical_cooked;
    }

    // check secondary dependencies
    if (!meta->contains("dependencies") || !(*meta)["dependencies"].is_array()) {
        return std::nullopt;
    }

    String dep_name = (hash_pos != String::npos) ? vp.path.substr(hash_pos + 1) : "";
    for (const auto& item : (*meta)["dependencies"]) {
        if (!item.is_object() || !item.contains("path") || !item["path"].is_string()) continue;
        bool match = (item.contains("guid") && item["guid"].is_number() && item["guid"].get<AssetID>() == guid) ||
                     (!dep_name.empty() && item.contains("name") && item["name"].get<String>() == dep_name);
        if (match) {
            String dep_cooked    = item["path"].get<String>();
            String canonical_dep = VFSPath::has_protocol(dep_cooked) ? dep_cooked : VFSPath(vp.scheme, dep_cooked).to_string();
            registry.update(guid, raw_path, type_id, {}, canonical_dep);
            return canonical_dep;
        }
    }

    return std::nullopt;
}

static auto try_resolve_from_convention(
    const VFSPath& vp,
    const String&  raw_path,
    AssetTypeID    type_id,
    AssetID        guid,
    OSPath         caches_path,
    AssetRegistry& registry) -> Optional<String>
{
    if (!caches_path) return std::nullopt;

    static const HashMap<AssetTypeID, std::pair<CString, CString>> conventions = {
        {ModelAsset::type, {"models", ".model"}},
        {MaterialAsset::type, {"materials", ".material"}},
        {MeshAsset::type, {"meshes", ".mesh"}},
    };

    auto it = conventions.find(type_id);
    if (it == conventions.end()) return std::nullopt;

    String candidate_rel = String(it->second.first) + "/" + std::to_string(guid) + it->second.second;
    if (fs::exists(Path(caches_path) / candidate_rel)) {
        String candidate = VFSPath(vp.scheme, candidate_rel).to_string();
        registry.update(guid, raw_path, type_id, registry.get_dependencies(guid), candidate);
        return candidate;
    }

    return std::nullopt;
}

AssetServer::AssetServer(const AMSDescriptor& descriptor) : descriptor(descriptor)
{
    assert(JobScheduler::is_initialized() && "JobScheduler must be initialized before creating AssetServer!");

    initialize_registry(registry, descriptor);

    if (descriptor.watch && descriptor.app.assets_path) {
        set_watching(true);
    }

    // configure preview generator
    JSON preview_opts;
    if (descriptor.app.caches_path) {
        preview_opts["caches_root"] = Path(descriptor.app.caches_path).string();
    } else if (descriptor.sys.caches_path) {
        preview_opts["caches_root"] = Path(descriptor.sys.caches_path).string();
    }
    preview_opts["width"]  = 128;
    preview_opts["height"] = 128;
    if (preview_api().configure) {
        preview_api().configure(this, preview_opts);
    }
}

AssetServer::~AssetServer()
{
    if (watcher) {
        watcher->stop();
    }
    JobScheduler::wait_idle();
    // unload all existing assets regardless of ref count
    for (auto& kv_processor : processors) {
        auto& processor = kv_processor.second;

        with_lock(*processor->mutex, [&] {
            for (auto& kv : processor->assets) {
                auto record = kv.second;
                if (record->data) {
                    processor->loader.unload(record->data);
                }
                delete record;
            }
            processor->assets.clear();
        });
    }

    processors.clear();
}

void AssetServer::purge()
{
    for (auto& kv_processor : processors) {
        auto& processor = kv_processor.second;

        with_lock(*processor->mutex, [&] {
            for (auto it = processor->assets.begin(); it != processor->assets.end();) {
                auto record = it->second;
                // only purge if refcnt is 0 and it's not currently loading (data != nullptr)
                if (record->refcnt == 0 && record->data != nullptr) {
                    processor->loader.unload(record->data);
                    delete record;
                    processor->assets.erase(it++);
                } else {
                    ++it;
                }
            }
        });
    }
}

void AssetServer::flush()
{
    registry.flush(descriptor.registry);
}

AssetID AssetServer::get_guid(FSPath path) const
{
    if (!path || path[0] == '\0') return 0;

    AssetID guid = registry.get_guid(path);
    if (guid != 0) return guid;

    VFSPath vp = VFSPath::parse(path);
    if (vp.is_valid()) {
        return registry.get_guid(vp.to_string());
    }
    return 0;
}

StringView AssetServer::get_path(AssetID guid) const
{
    return registry.get_path(guid);
}

void AssetServer::register_asset_entry(AssetID guid, const Path& path, AssetTypeID type_id, const Vector<AssetID>& dependencies, const Path& cooked_path)
{
    VFSPath vp = VFSPath::parse(path.generic_string());
    if (!vp.is_valid()) return;

    static std::mutex reg_mut;
    with_lock(reg_mut, [&] {
        registry.update(guid, vp.to_string(), type_id, dependencies, cooked_path.generic_string());
    });
}

void* AssetServer::get_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return nullptr;
    const auto& processor = it->second;

    return with_shared_lock(*processor->mutex, [&]() -> void* {
        const auto it2 = processor->assets.find(handle.guid);
        if (it2 == processor->assets.end()) return nullptr;
        return it2->second->data;
    });
}

RawAssetHandle AssetServer::load_asset(AssetTypeID type_id, FSPath path)
{
    if (!path || path[0] == '\0') return RawAssetHandle();
    AssetID guid = get_guid(path);
    if (guid == 0) return RawAssetHandle();

    return load_asset(type_id, guid);
}

RawAssetHandle AssetServer::load_asset(AssetTypeID type_id, AssetID guid)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return RawAssetHandle();
    AssetProcessor* processor_ptr = it->second.get();

    bool should_load_deps = false;
    bool is_new_entry     = false;

    with_lock(*processor_ptr->mutex, [&] {
        auto it2 = processor_ptr->assets.find(guid);
        if (it2 == processor_ptr->assets.end()) {
            auto record                 = new AssetRecord();
            record->data                = nullptr;
            record->refcnt              = 1;
            processor_ptr->assets[guid] = record;
            should_load_deps            = true;
            is_new_entry                = true;
        } else {
            if (it2->second->refcnt == 0) should_load_deps = true;
            it2->second->refcnt++;
        }
    });

    if (should_load_deps) {
        load_dependencies(guid);
    }

    if (is_new_entry) {
        String       path   = resolve_load_path(type_id, guid);
        AssetRecord* record = with_shared_lock(*processor_ptr->mutex, [&] {
            return processor_ptr->assets[guid];
        });

        JobScheduler::schedule(JobPriority::BACKGROUND, [this, processor_ptr, path, record]() {
            record->data = processor_ptr->loader.load(descriptor.loader, path.c_str());
        });
    }

    return RawAssetHandle{guid};
}

void AssetServer::load_dependencies(AssetID guid)
{
    const auto& deps = registry.get_dependencies(guid);
    for (auto dep_guid : deps) {
        AssetTypeID dep_type = registry.get_type(dep_guid);
        if (dep_type != 0) {
            load_asset(dep_type, dep_guid);
        }
    }
}

void AssetServer::unload_dependencies(AssetID guid)
{
    const auto& deps = registry.get_dependencies(guid);
    for (auto dep_guid : deps) {
        AssetTypeID dep_type = registry.get_type(dep_guid);
        if (dep_type != 0) {
            unload_asset(dep_type, RawAssetHandle{dep_guid});
        }
    }
}

void AssetServer::unload_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    auto& processor = it->second;

    bool should_unload_deps = false;

    with_lock(*processor->mutex, [&] {
        auto it2 = processor->assets.find(handle.guid);
        if (it2 != processor->assets.end()) {
            it2->second->refcnt--;
            if (it2->second->refcnt == 0) {
                should_unload_deps = true;
            }
        }
    });

    if (should_unload_deps) {
        unload_dependencies(handle.guid);
    }
}

void AssetServer::clone_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    auto& processor = it->second;

    with_lock(*processor->mutex, [&] {
        auto it2 = processor->assets.find(handle.guid);
        if (it2 != processor->assets.end()) {
            it2->second->refcnt++;
        }
    });
}

auto AssetServer::find_asset_type_for_cooker(const AssetCookerAPI* cooker) const -> AssetTypeID
{
    for (const auto& [tid, proc] : processors) {
        for (const auto& c : proc->cookers) {
            if (&c == cooker) {
                return tid;
            }
        }
    }
    return 0;
}

bool AssetServer::is_cook_up_to_date(const Path& source_path, const Path& import_path) const
{
    if (!fs::exists(import_path) || !fs::exists(source_path)) {
        return false;
    }
    time_t meta_time = load_metadata_time(import_path);
    time_t file_time = get_file_mtime(source_path);
    return meta_time > 0 && meta_time >= file_time;
}

auto AssetServer::resolve_or_create_guid(const Path& rel_path, const Path& import_path) -> AssetID
{
    AssetID guid = registry.get_guid(rel_path.generic_string());
    if (guid == 0 && fs::exists(import_path)) {
        guid = load_guid(import_path);
    }
    if (guid == 0) {
        guid = registry.generate_guid();
    }
    return guid;
}

bool AssetServer::execute_cooker(AssetCookerAPI* cooker, const Path& source_path, OSPath caches_path, JSON& metadata)
{
    try {
        return cooker->process(metadata, (OSPath)source_path.c_str(), caches_path);
    } catch (const std::exception& e) {
        spdlog::error("Exception in cooker processing {}: {}", source_path.string(), e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown exception in cooker processing {}", source_path.string());
        return false;
    }
}

void AssetServer::commit_cooked_asset(const Path& import_path, const Path& rel_path, AssetID guid, AssetTypeID type_id, const JSON& metadata)
{
    auto prev_meta = load_json_file(import_path);
    save_json(import_path, metadata);

    HashSet<AssetID> current_dep_guids;
    Vector<AssetID>  dependency_guids;

    static std::mutex registry_mutex;
    with_lock(registry_mutex, [&] {
        VFSPath vp          = VFSPath::parse(rel_path.generic_string());
        OSPath  caches_path = get_caches_path(descriptor, vp.scheme);

        register_cooked_dependencies(metadata, rel_path, vp.scheme, registry, dependency_guids, current_dep_guids);
        cleanup_orphaned_dependencies(prev_meta, current_dep_guids, caches_path, registry, [this](AssetID id) {
            unload_record(id);
        });

        String cooked_path;
        if (metadata.contains("path") && metadata["path"].is_string()) {
            String raw_cooked = metadata["path"].get<String>();
            cooked_path       = VFSPath::has_protocol(raw_cooked) ? raw_cooked : VFSPath(vp.scheme, raw_cooked).to_string();
        }

        registry.update(guid, rel_path.generic_string(), type_id, dependency_guids, cooked_path);
    });

    reload_asset(guid);
}

auto AssetServer::cook_asset_task(AssetCookerAPI* cooker, const Path& source_path, const Path& import_path, const Path& rel_path, AssetID guid) -> AssetID
{
    with_lock(pipeline_mutex, [&] {
        current_cooking_asset = rel_path.string();
    });

    auto finish_task = [this, &rel_path](bool success) {
        with_lock(pipeline_mutex, [&] {
            if (current_cooking_asset == rel_path.string()) {
                current_cooking_asset.clear();
            }
        });
        if (success) {
            completed_cooks++;
        } else {
            failed_cooks++;
        }
        pending_cooks--;
    };

    try {
        AssetTypeID type_id = find_asset_type_for_cooker(cooker);
        if (type_id == 0) {
            spdlog::error("No processor registered for cooker cooking {}", rel_path.string());
            finish_task(false);
            return 0;
        }

        JSON metadata;
        metadata["guid"]    = guid;
        metadata["version"] = "1";
        metadata["type"]    = to_string(type_id);
        metadata["time"]    = get_timestamp();

        // if import_path already exists, read existing dependencies into metadata so cooker can reuse them
        auto prev_meta = load_json_file(import_path);
        if (prev_meta && prev_meta->contains("dependencies")) {
            metadata["dependencies"] = (*prev_meta)["dependencies"];
        }

        VFSPath vp          = VFSPath::parse(rel_path.generic_string());
        OSPath  caches_path = get_caches_path(descriptor, vp.scheme);
        if (!execute_cooker(cooker, source_path, caches_path, metadata)) {
            spdlog::error("Failed to cook asset: {}", rel_path.string());
            std::error_code ec;
            fs::remove(import_path, ec);
            finish_task(false);
            return 0;
        }

        commit_cooked_asset(import_path, rel_path, guid, type_id, metadata);
        finish_task(true);
        return guid;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected exception while importing {}: {}", rel_path.string(), e.what());
        finish_task(false);
        return 0;
    } catch (...) {
        spdlog::error("Unknown fatal error while importing {}", rel_path.string());
        finish_task(false);
        return 0;
    }
}

Future<AssetID> AssetServer::import_asset(const Path& path, bool force)
{
    auto ext = absl::AsciiStrToLower(path.extension().string());
    auto it  = cooker_extensions.find(ext);
    if (it == cooker_extensions.end()) {
        spdlog::error("No cooker found for extension: {}", ext);
        Promise<AssetID> p;
        p.set_value(0);
        return p.get_future();
    }

    auto [source_path, rel_path] = resolve_asset_path(path);
    if (source_path.empty() || rel_path.empty()) {
        spdlog::error("Cannot import asset with invalid path: {}", path.string());
        Promise<AssetID> p;
        p.set_value(0);
        return p.get_future();
    }

    Path    import_path = get_metadata_path(source_path);
    AssetID guid        = resolve_or_create_guid(rel_path, import_path);

    if (!force && is_cook_up_to_date(source_path, import_path)) {
        Promise<AssetID> p;
        p.set_value(guid);
        return p.get_future();
    }

    pending_cooks++;

    auto promise = std::make_shared<Promise<AssetID>>();
    auto future  = promise->get_future();

    JobScheduler::schedule(JobPriority::BACKGROUND, [this, cooker = it->second, source_path, import_path, rel_path, guid, promise]() {
        try {
            AssetID result = cook_asset_task(cooker, source_path, import_path, rel_path, guid);
            promise->set_value(result);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });

    return future;
}

auto AssetServer::resolve_asset_path(const Path& path) const -> std::pair<Path, Path>
{
    std::error_code ec;

    String path_str = path.generic_string();

    // 1. check if path is already a valid VFS path with scheme (e.g. "app://..." or "sys://...")
    if (VFSPath::has_protocol(path_str)) {
        VFSPath vp = VFSPath::parse(path_str);
        if (vp.is_valid()) {
            OSPath base = get_assets_path(descriptor, vp.scheme);
            if (base) {
                Path full_path = Path(base) / vp.path;
                Path rel_path  = Path(vp.to_string());
                strip_import_extension(full_path, rel_path);
                return {full_path, rel_path};
            }
        }
    }

    // 2. check if path is an absolute OS path
    Path norm = path.lexically_normal();
    if (norm.is_absolute()) {
        if (descriptor.app.assets_path) {
            Path base = Path(descriptor.app.assets_path).lexically_normal();
            Path rel  = fs::relative(norm, base, ec);
            if (!ec && !rel.empty() && *rel.begin() != "..") {
                Path   full_path = norm;
                String sub       = (rel == ".") ? "" : rel.generic_string();
                Path   rel_path  = Path(VFSPath(vfs::SchemeApp, sub).to_string());
                strip_import_extension(full_path, rel_path);
                return {full_path, rel_path};
            }
        }
        if (descriptor.sys.assets_path) {
            Path base = Path(descriptor.sys.assets_path).lexically_normal();
            Path rel  = fs::relative(norm, base, ec);
            if (!ec && !rel.empty() && *rel.begin() != "..") {
                Path   full_path = norm;
                String sub       = (rel == ".") ? "" : rel.generic_string();
                Path   rel_path  = Path(VFSPath(vfs::SchemeSys, sub).to_string());
                strip_import_extension(full_path, rel_path);
                return {full_path, rel_path};
            }
        }
    }

    // 3. bare relative path without scheme: default to app:// scheme
    if (descriptor.app.assets_path) {
        Path full_path = Path(descriptor.app.assets_path) / path;
        Path rel_path  = Path(VFSPath(vfs::SchemeApp, path.generic_string()).to_string());
        strip_import_extension(full_path, rel_path);
        return {full_path, rel_path};
    }

    return {};
}

void AssetServer::unload_record(AssetID guid)
{
    if (guid == 0) return;

    for (auto& [type_id, processor] : processors) {
        with_lock(*processor->mutex, [&] {
            auto it = processor->assets.find(guid);
            if (it != processor->assets.end()) {
                auto record = it->second;
                if (record->data) {
                    processor->loader.unload(record->data);
                }
                delete record;
                processor->assets.erase(it);
            }
        });
    }
}

void AssetServer::delete_metadata_and_caches(const Path& import_path, OSPath caches_path, AssetID guid)
{
    std::error_code ec;

    if (guid == 0 && fs::exists(import_path, ec)) {
        guid = load_guid(import_path);
    }

    if (guid != 0) {
        unload_record(guid);
        static std::mutex registry_mutex;
        with_lock(registry_mutex, [&] {
            registry.remove(guid);
        });
    }

    auto meta = load_json_file(import_path);
    if (meta) {
        static std::mutex registry_mutex;
        with_lock(registry_mutex, [&] {
            remove_metadata_cache_files(*meta, caches_path, registry, [this](AssetID id) {
                unload_record(id);
            });
        });

        fs::remove(import_path, ec);
    }
}

bool AssetServer::delete_directory_assets(const Path& dir_path, OSPath caches_path)
{
    std::error_code ec;

    Vector<Path> import_files;
    for (const auto& entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".import") {
            import_files.push_back(entry.path());
        }
    }

    for (const auto& import_file : import_files) {
        delete_metadata_and_caches(import_file, caches_path, 0);
    }

    fs::remove_all(dir_path, ec);

    Path dir_import = get_metadata_path(dir_path);
    if (fs::exists(dir_import, ec)) {
        fs::remove(dir_import, ec);
    }

    return !ec;
}

bool AssetServer::delete_single_asset(const Path& full_path, const Path& rel_path, OSPath caches_path)
{
    std::error_code ec;

    AssetID guid = 0;

    Path import_path = get_metadata_path(full_path);
    if (fs::exists(import_path, ec)) {
        guid = load_guid(import_path);
    }
    if (guid == 0) {
        guid = registry.get_guid(rel_path.generic_string());
    }

    delete_metadata_and_caches(import_path, caches_path, guid);

    if (guid == 0) {
        static std::mutex registry_mutex;
        with_lock(registry_mutex, [&] {
            registry.remove(rel_path.generic_string());
        });
    }

    if (fs::exists(full_path, ec)) {
        fs::remove(full_path, ec);
    }

    return true;
}

bool AssetServer::delete_asset(const Path& path)
{
    auto [full_path, rel_path] = resolve_asset_path(path);
    if (full_path.empty() || rel_path.empty()) {
        spdlog::error("AssetServer: Cannot delete asset with invalid path: {}", path.string());
        return false;
    }

    std::error_code ec;

    VFSPath vp          = VFSPath::parse(rel_path.generic_string());
    OSPath  caches_path = get_caches_path(descriptor, vp.scheme);

    bool success = false;
    if (fs::is_directory(full_path, ec)) {
        success = delete_directory_assets(full_path, caches_path);
    } else {
        success = delete_single_asset(full_path, rel_path, caches_path);
    }

    flush();
    return success;
}

bool AssetServer::delete_asset(AssetID guid)
{
    StringView p = registry.get_path(guid);
    if (!p.empty()) {
        return delete_asset(Path(String(p)));
    }

    unload_record(guid);

    static std::mutex registry_mutex;
    with_lock(registry_mutex, [&] {
        registry.remove(guid);
    });

    flush();
    return true;
}

auto AssetServer::resolve_destination_path(const Path& src_full, const Path& destination) const -> std::pair<Path, Path>
{
    auto [dst_full, dst_rel] = resolve_asset_path(destination);
    if (dst_full.empty() || dst_rel.empty()) {
        return {};
    }

    std::error_code ec;
    if (fs::is_directory(dst_full, ec)) {
        dst_full /= src_full.filename();
        VFSPath dst_vp = VFSPath::parse(dst_rel.generic_string());
        String  sub    = dst_vp.path.empty() ? src_full.filename().generic_string() : (dst_vp.path + "/" + src_full.filename().generic_string());
        dst_rel        = Path(VFSPath(dst_vp.scheme, sub).to_string());
    }

    strip_import_extension(dst_full, dst_rel);
    return {dst_full, dst_rel};
}

bool AssetServer::move_directory_assets(const Path& src_full, const Path& src_rel, const Path& dst_full, const Path& dst_rel)
{
    std::error_code ec;

    struct MovedEntry
    {
        AssetID guid;
        String  new_rel_path;
    };
    Vector<MovedEntry> moved_entries;

    VFSPath dst_vp = VFSPath::parse(dst_rel.generic_string());

    for (const auto& entry : fs::recursive_directory_iterator(src_full, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".import") continue;
        AssetID guid = load_guid(entry.path());
        if (guid == 0) continue;

        Path asset_path = entry.path();
        asset_path.replace_extension("");
        Path rel_to_src = fs::relative(asset_path, src_full, ec);
        if (ec) continue;

        String sub = dst_vp.path.empty() ? rel_to_src.generic_string() : (dst_vp.path + "/" + rel_to_src.generic_string());
        moved_entries.push_back({guid, VFSPath(dst_vp.scheme, sub).to_string()});
    }

    fs::rename(src_full, dst_full, ec);
    if (ec) {
        spdlog::error("AssetServer: Failed to move directory {} to {}: {}", src_full.string(), dst_full.string(), ec.message());
        return false;
    }

    Path src_dir_import = get_metadata_path(src_full);
    Path dst_dir_import = get_metadata_path(dst_full);
    if (fs::exists(src_dir_import, ec)) {
        fs::rename(src_dir_import, dst_dir_import, ec);
    }

    static std::mutex reg_mut;
    with_lock(reg_mut, [&] {
        for (const auto& item : moved_entries) {
            registry.update(item.guid, item.new_rel_path, registry.get_type(item.guid), registry.get_dependencies(item.guid));
        }
    });

    return true;
}

bool AssetServer::move_single_asset(const Path& src_full, const Path& src_rel, const Path& dst_full, const Path& dst_rel)
{
    std::error_code ec;

    Path src_import = get_metadata_path(src_full);
    Path dst_import = get_metadata_path(dst_full);

    AssetID guid = 0;
    if (fs::exists(src_import, ec)) {
        guid = load_guid(src_import);
    }
    if (guid == 0) {
        guid = registry.get_guid(src_rel.generic_string());
    }

    fs::rename(src_full, dst_full, ec);
    if (ec) {
        spdlog::error("AssetServer: Failed to move file {} to {}: {}", src_full.string(), dst_full.string(), ec.message());
        return false;
    }

    if (fs::exists(src_import, ec)) {
        fs::rename(src_import, dst_import, ec);
    }

    if (guid != 0) {
        static std::mutex reg_mut;
        with_lock(reg_mut, [&] {
            registry.update(guid, dst_rel.generic_string(), registry.get_type(guid), registry.get_dependencies(guid));
        });
    }

    return true;
}

bool AssetServer::move_asset(const Path& source_path, const Path& destination_path)
{
    auto [src_full, src_rel] = resolve_asset_path(source_path);
    if (src_full.empty() || src_rel.empty()) {
        spdlog::error("AssetServer: Cannot move asset with invalid source path: {}", source_path.string());
        return false;
    }

    std::error_code ec;
    if (!fs::exists(src_full, ec)) {
        spdlog::error("AssetServer: Cannot move asset {}: source does not exist", src_full.string());
        return false;
    }

    auto [dst_full, dst_rel] = resolve_destination_path(src_full, destination_path);
    if (dst_full.empty() || dst_rel.empty()) {
        spdlog::error("AssetServer: Cannot move asset to invalid destination path: {}", destination_path.string());
        return false;
    }

    if (src_full == dst_full) {
        return true;
    }

    if (fs::exists(dst_full, ec)) {
        spdlog::error("AssetServer: Cannot move asset to {}: destination already exists", dst_full.string());
        return false;
    }

    if (dst_full.has_parent_path()) {
        fs::create_directories(dst_full.parent_path(), ec);
    }

    bool success = false;
    if (fs::is_directory(src_full, ec)) {
        success = move_directory_assets(src_full, src_rel, dst_full, dst_rel);
    } else {
        success = move_single_asset(src_full, src_rel, dst_full, dst_rel);
    }

    if (success) {
        flush();
    }
    return success;
}

bool AssetServer::move_asset(AssetID guid, const Path& destination_path)
{
    StringView p = registry.get_path(guid);
    if (p.empty()) {
        spdlog::error("AssetServer: Cannot move asset with GUID {:#x}: not found in registry", guid);
        return false;
    }
    return move_asset(Path(String(p)), destination_path);
}

void AssetServer::reload_asset(AssetID guid)
{
    AssetTypeID type_id = registry.get_type(guid);
    if (type_id == 0) return;

    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    AssetProcessor* proc_ptr = it->second.get();

    AssetRecord* record = with_shared_lock(*proc_ptr->mutex, [&]() -> AssetRecord* {
        auto it2 = proc_ptr->assets.find(guid);
        if (it2 != proc_ptr->assets.end() && it2->second->data != nullptr && it2->second->refcnt > 0) {
            return it2->second;
        }
        return nullptr;
    });

    if (!record) return;

    String path = resolve_load_path(type_id, guid);
    JobScheduler::schedule(JobPriority::BACKGROUND, [this, proc_ptr, path, record, guid, type_id]() {
        try {
            void* new_data = proc_ptr->loader.load(descriptor.loader, path.c_str());
            if (new_data) {
                void* old_data = with_lock(*proc_ptr->mutex, [&]() -> void* {
                    void* old    = record->data;
                    record->data = new_data;
                    return old;
                });
                if (old_data) {
                    proc_ptr->loader.unload(old_data);
                }
                spdlog::info("AssetServer: Hot-reloaded asset {} (GUID: {:#x})", path, guid);

                with_lock(pipeline_mutex, [&] {
                    queued_reloaded_assets.emplace_back(guid, type_id);
                });
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to hot-reload asset {}: {}", path, e.what());
        } catch (...) {
            spdlog::error("Unknown error during hot-reload of asset {}", path);
        }
    });
}

String AssetServer::resolve_load_path(AssetTypeID type_id, AssetID guid)
{
    StringView cooked = registry.get_cooked_path(guid);
    if (!cooked.empty()) {
        return String(cooked);
    }

    StringView raw_path_sv = registry.get_path(guid);
    if (raw_path_sv.empty()) {
        return "";
    }
    String raw_path(raw_path_sv);

    VFSPath vp          = VFSPath::parse(raw_path);
    OSPath  assets_path = get_assets_path(descriptor, vp.scheme);
    OSPath  caches_path = get_caches_path(descriptor, vp.scheme);

    if (auto resolved = try_resolve_from_sidecar(vp, raw_path, type_id, guid, assets_path, registry)) {
        return *resolved;
    }

    if (auto resolved = try_resolve_from_convention(vp, raw_path, type_id, guid, caches_path, registry)) {
        return *resolved;
    }

    return raw_path;
}

void AssetServer::reimport_all(bool force)
{
    if (!descriptor.app.assets_path) return;
    Path assets_dir(descriptor.app.assets_path);
    if (!fs::exists(assets_dir)) return;

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(assets_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        Path rel_to_app = fs::relative(entry.path(), assets_dir, ec);
        if (ec || rel_to_app.empty()) continue;
        if (has_cooker_for(entry.path())) {
            import_asset(Path(VFSPath(vfs::SchemeApp, rel_to_app.generic_string()).to_string()), force);
        }
    }
}

void AssetServer::set_watching(bool enable)
{
    if (!descriptor.app.assets_path) return;

    if (enable) {
        if (!watcher) {
            watcher = std::make_unique<AssetWatcher>(Path(descriptor.app.assets_path));
            watcher->start([this](const Vector<AssetWatchEvent>& evts) {
                handle_watch_events(evts);
            });
            spdlog::info("AssetServer: Active asset directory watching started on: {}", Path(descriptor.app.assets_path).string());
        } else if (watcher->is_paused()) {
            watcher->resume();
            spdlog::info("AssetServer: Active asset directory watching resumed");
        }
    } else {
        if (watcher) {
            watcher->pause();
            spdlog::info("AssetServer: Active asset directory watching paused");
        }
    }
}

bool AssetServer::is_watching() const
{
    return watcher && watcher->is_running() && !watcher->is_paused();
}

AssetPipelineStats AssetServer::get_pipeline_stats() const
{
    AssetPipelineStats stats;
    stats.pending_count   = pending_cooks.load();
    stats.completed_count = completed_cooks.load();
    stats.failed_count    = failed_cooks.load();
    stats.watching        = is_watching();
    stats.current_asset   = with_lock(pipeline_mutex, [&] {
        return current_cooking_asset;
    });
    return stats;
}

bool AssetServer::has_cooker_for(const Path& path) const
{
    auto ext = absl::AsciiStrToLower(path.extension().string());
    return cooker_extensions.find(ext) != cooker_extensions.end();
}

void AssetServer::poll_events()
{
    Vector<std::pair<AssetID, AssetTypeID>> reloads;

    bool has_fs_changes = false;
    with_lock(pipeline_mutex, [&] {
        if (!queued_reloaded_assets.empty()) {
            reloads = std::move(queued_reloaded_assets);
            queued_reloaded_assets.clear();
        }
        if (!queued_fs_events.empty()) {
            has_fs_changes = true;
            queued_fs_events.clear();
        }
    });

    for (const auto& [guid, type_id] : reloads) {
        if (on_asset_reloaded) {
            on_asset_reloaded(guid, type_id);
        }
    }

    if (has_fs_changes && on_fs_changed) {
        on_fs_changed();
    }
}

auto AssetServer::preview(const PreviewScene& scene, JSON& metadata) -> Future<Path>
{
    return preview_api().generate_thumbnail(scene, metadata);
}

void AssetServer::set_on_asset_reloaded(AssetReloadCallback callback)
{
    with_lock(pipeline_mutex, [&] {
        on_asset_reloaded = std::move(callback);
    });
}

void AssetServer::set_on_filesystem_changed(FileSystemChangeCallback callback)
{
    with_lock(pipeline_mutex, [&] {
        on_fs_changed = std::move(callback);
    });
}

void AssetServer::handle_watch_rename(const AssetWatchEvent& evt)
{
    spdlog::info("AssetServer: Watcher detected rename from {} to {}",
        evt.old_path.string(), evt.path.string());

    if (evt.old_path.empty()) return;

    String old_vpath = VFSPath(vfs::SchemeApp, evt.old_path.generic_string()).to_string();
    String new_vpath = VFSPath(vfs::SchemeApp, evt.path.generic_string()).to_string();

    AssetID guid = registry.get_guid(old_vpath);
    if (guid == 0) return;

    static std::mutex reg_mut;
    with_lock(reg_mut, [&] {
        registry.update(guid, new_vpath, registry.get_type(guid), registry.get_dependencies(guid));
    });

    // rename .import sidecar if it exists
    Path old_import = get_metadata_path(Path(descriptor.app.assets_path) / evt.old_path);
    Path new_import = get_metadata_path(Path(descriptor.app.assets_path) / evt.path);

    std::error_code ec;
    if (fs::exists(old_import)) {
        fs::rename(old_import, new_import, ec);
    }
}

void AssetServer::handle_watch_events(const Vector<AssetWatchEvent>& events)
{
    bool fs_changed = false;

    for (const auto& evt : events) {
        if (evt.action == AssetWatchAction::Added || evt.action == AssetWatchAction::Modified) {
            if (has_cooker_for(evt.path)) {
                String vpath = VFSPath(vfs::SchemeApp, evt.path.generic_string()).to_string();
                spdlog::info("AssetServer: Watcher detected {} file {}, queuing cook...",
                    evt.action == AssetWatchAction::Added ? "added" : "modified",
                    vpath);
                import_asset(Path(vpath), false);
            }
            fs_changed = true;
        } else if (evt.action == AssetWatchAction::Renamed) {
            handle_watch_rename(evt);
            fs_changed = true;
        } else if (evt.action == AssetWatchAction::Removed) {
            spdlog::info("AssetServer: Watcher detected file removed: {}", evt.path.string());
            fs_changed = true;
        }
    }

    if (fs_changed) {
        with_lock(pipeline_mutex, [&] {
            for (const auto& e : events) {
                queued_fs_events.push_back(e);
            }
        });
    }
}

bool AssetServer::save_asset(AssetTypeID type_id, const void* asset, OSPath path)
{
    auto it = processors.find(type_id);
    if (it == processors.end() || !it->second->saver.has_value()) {
        spdlog::error("No saver registered for asset type ID: {}", to_string(type_id));
        return false;
    }

    const auto& saver = it->second->saver.value();
    if (!saver.save) {
        spdlog::error("Saver for asset type ID {} has no save function defined", to_string(type_id));
        return false;
    }

    return saver.save(asset, path);
}
