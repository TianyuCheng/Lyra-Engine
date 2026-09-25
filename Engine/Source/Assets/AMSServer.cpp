#include <ctime>
#include <mutex>
#include <fstream>
#include <cassert>
#include <filesystem>

#include <absl/strings/ascii.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/AMSPreview.h>

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

static AssetID load_guid(const Path& path)
{
    AssetID guid = 0;

    std::ifstream f(path, std::ios::in);
    if (!f.good()) return 0;

    // check if file is empty to avoid json parse error
    if (f.peek() == std::ifstream::traits_type::eof())
        return 0;

    try {
        JSON data = JSON::parse(f);
        if (data.contains("guid")) {
            guid = data["guid"].get<AssetID>();
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse metadata GUID from {}: {}", path.string(), e.what());
    }

    f.close();
    return guid;
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
    std::ifstream f(path, std::ios::in);
    if (!f.good()) return 0;
    if (f.peek() == std::ifstream::traits_type::eof()) return 0;
    try {
        JSON data = JSON::parse(f);
        if (data.contains("time")) {
            return data["time"].get<time_t>();
        }
    } catch (...) {
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

AssetServer::AssetServer(const AMSDescriptor& descriptor)
    : descriptor(descriptor)
{
    assert(JobScheduler::is_initialized() && "JobScheduler must be initialized before creating AssetServer!");

    // load registry
    if (descriptor.registry) {
        if (!registry.load(descriptor.registry)) {
            spdlog::info("AssetRegistry {} not found or failed to load. Rebuilding from source...", Path(descriptor.registry).string());
            if (descriptor.importer.assets_path) {
                registry.rebuild(descriptor.importer.assets_path);
            }
            registry.save(descriptor.registry);
        }
    }

    if (descriptor.watch && descriptor.importer.assets_path) {
        set_watching(true);
    }

    // configure preview generator
    JSON preview_opts;
    if (descriptor.importer.caches_path) {
        preview_opts["caches_root"] = Path(descriptor.importer.caches_path).string();
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

        std::unique_lock alock(*processor->mutex);
        for (auto& kv : processor->assets) {
            auto record = kv.second;
            if (record->data) {
                processor->loader.unload(record->data);
            }
            delete record;
        }
        processor->assets.clear();
    }

    processors.clear();
}

void AssetServer::purge()
{
    for (auto& kv_processor : processors) {
        auto& processor = kv_processor.second;

        std::unique_lock alock(*processor->mutex);
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
    }
}

void AssetServer::flush()
{
    registry.flush(descriptor.registry);
}

AssetID AssetServer::get_guid(FSPath path) const
{
    return registry.get_guid(path);
}

void* AssetServer::get_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return nullptr;
    const auto& processor = it->second;

    std::shared_lock alock(*processor->mutex);

    const auto it2 = processor->assets.find(handle.guid);
    if (it2 == processor->assets.end()) return nullptr;
    return it2->second->data;
}

RawAssetHandle AssetServer::load_asset(AssetTypeID type_id, FSPath path)
{
    AssetID guid = registry.get_guid(path);
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

    {
        std::unique_lock alock(*processor_ptr->mutex);

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
    }

    if (should_load_deps) {
        load_dependencies(guid);
    }

    if (is_new_entry) {
        String       path   = String(registry.get_path(guid));
        AssetRecord* record = nullptr;
        {
            std::shared_lock alock(*processor_ptr->mutex);
            record = processor_ptr->assets[guid];
        }

        JobScheduler::schedule(JobPriority::BACKGROUND, [this, processor_ptr, path, record]() {
            record->data = processor_ptr->loader.load(descriptor.loader.assets, path.c_str());
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

    {
        std::unique_lock alock(*processor->mutex);

        auto it2 = processor->assets.find(handle.guid);
        if (it2 != processor->assets.end()) {
            it2->second->refcnt--;
            if (it2->second->refcnt == 0) {
                should_unload_deps = true;
            }
        }
    }

    if (should_unload_deps) {
        unload_dependencies(handle.guid);
    }
}

void AssetServer::clone_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    auto& processor = it->second;

    std::unique_lock alock(*processor->mutex);

    auto it2 = processor->assets.find(handle.guid);
    if (it2 != processor->assets.end()) {
        it2->second->refcnt++;
    }
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
    AssetID guid = registry.get_guid(rel_path.string());
    if (guid == 0 && fs::exists(import_path)) {
        guid = load_guid(import_path);
    }
    if (guid == 0) {
        guid = registry.generate_guid();
    }
    return guid;
}

bool AssetServer::execute_cooker(AssetCookerAPI* cooker, const Path& source_path, JSON& metadata)
{
    try {
        return cooker->process(metadata, (OSPath)source_path.c_str(), descriptor.importer.caches_path);
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
    std::error_code ec;
    JSON            prev_meta;
    if (fs::exists(import_path, ec)) {
        try {
            std::ifstream sf(import_path);
            if (sf.good() && sf.peek() != std::ifstream::traits_type::eof()) {
                prev_meta = JSON::parse(sf);
            }
        } catch (...) {
        }
    }

    save_json(import_path, metadata);

    HashSet<AssetID> current_dep_guids;
    Vector<AssetID>  dependency_guids;

    static std::mutex registry_mutex;
    std::lock_guard   lock(registry_mutex);

    if (metadata.contains("dependencies") && metadata["dependencies"].is_array()) {
        for (const auto& item : metadata["dependencies"]) {
            if (item.is_number()) {
                AssetID dep_guid = item.get<AssetID>();
                dependency_guids.push_back(dep_guid);
                current_dep_guids.insert(dep_guid);
            } else if (item.is_object() && item.contains("guid")) {
                AssetID dep_guid = item["guid"].get<AssetID>();
                dependency_guids.push_back(dep_guid);
                current_dep_guids.insert(dep_guid);
                if (item.contains("name") && item.contains("type")) {
                    String      dep_name = item["name"].get<String>();
                    AssetTypeID dep_type{};
                    if (item["type"].is_string()) {
                        dep_type = parse_uuid(item["type"].get<String>());
                    } else if (item["type"].is_number()) {
                        dep_type = AssetTypeID(item["type"].get<ulong>());
                    }
                    String dep_vpath = rel_path.generic_string() + "#" + dep_name;
                    registry.update(dep_guid, dep_vpath, dep_type, {});
                }
            }
        }
    }

    // orphan cleanup: if prev_meta had dependencies that are no longer present
    if (prev_meta.contains("dependencies") && prev_meta["dependencies"].is_array()) {
        for (const auto& item : prev_meta["dependencies"]) {
            if (item.is_object() && item.contains("guid")) {
                AssetID prev_guid = item["guid"].get<AssetID>();
                if (current_dep_guids.find(prev_guid) == current_dep_guids.end()) {
                    if (descriptor.importer.caches_path && item.contains("path") && item["path"].is_string()) {
                        Path old_cache = Path(descriptor.importer.caches_path) / item["path"].get<String>();
                        fs::remove(old_cache, ec);
                    }
                    unload_record(prev_guid);
                    registry.remove(prev_guid);
                }
            }
        }
    }

    registry.update(guid, rel_path.generic_string(), type_id, dependency_guids);

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
        std::error_code ec;
        if (fs::exists(import_path, ec)) {
            try {
                std::ifstream sf(import_path);
                if (sf.good() && sf.peek() != std::ifstream::traits_type::eof()) {
                    JSON prev_meta = JSON::parse(sf);
                    if (prev_meta.contains("dependencies")) {
                        metadata["dependencies"] = prev_meta["dependencies"];
                    }
                }
            } catch (...) {
            }
        }

        if (!execute_cooker(cooker, source_path, metadata)) {
            spdlog::error("Failed to cook asset: {}", rel_path.string());
            if (fs::exists(import_path, ec)) {
                fs::remove(import_path, ec);
            }
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

    Path    source_path = Path(descriptor.importer.assets_path) / path;
    Path    import_path = get_metadata_path(source_path);
    AssetID guid        = resolve_or_create_guid(path, import_path);

    if (!force && is_cook_up_to_date(source_path, import_path)) {
        Promise<AssetID> p;
        p.set_value(guid);
        return p.get_future();
    }

    pending_cooks++;

    auto promise = std::make_shared<Promise<AssetID>>();
    auto future  = promise->get_future();

    JobScheduler::schedule(JobPriority::BACKGROUND, [this, cooker = it->second, source_path, import_path, path, guid, promise]() {
        try {
            AssetID result = cook_asset_task(cooker, source_path, import_path, path, guid);
            promise->set_value(result);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });

    return future;
}

auto AssetServer::resolve_asset_path(const Path& path) const -> std::pair<Path, Path>
{
    Path full_path;
    Path rel_path;

    if (path.is_absolute()) {
        full_path = path;
        if (descriptor.importer.assets_path) {
            std::error_code ec;
            rel_path = fs::relative(full_path, Path(descriptor.importer.assets_path), ec);
            if (ec) rel_path = full_path;
        } else {
            rel_path = full_path;
        }
    } else {
        if (descriptor.importer.assets_path) {
            std::error_code ec;

            Path base = Path(descriptor.importer.assets_path);
            auto rel  = fs::relative(path, base, ec);
            if (!ec && !rel.empty() && *rel.begin() != "..") {
                full_path = path;
                rel_path  = rel;
            } else {
                full_path = base / path;
                rel_path  = path;
            }
        } else {
            full_path = path;
            rel_path  = path;
        }
    }

    if (full_path.extension() == ".import") {
        full_path.replace_extension("");
        if (rel_path.extension() == ".import") {
            rel_path.replace_extension("");
        }
    }

    return {full_path, rel_path};
}

void AssetServer::unload_record(AssetID guid)
{
    if (guid == 0) return;

    for (auto& [type_id, processor] : processors) {
        std::unique_lock alock(*processor->mutex);
        auto             it = processor->assets.find(guid);
        if (it != processor->assets.end()) {
            auto record = it->second;
            if (record->data) {
                processor->loader.unload(record->data);
            }
            delete record;
            processor->assets.erase(it);
        }
    }
}

void AssetServer::delete_metadata_and_caches(const Path& import_path, AssetID guid)
{
    std::error_code ec;

    if (guid == 0 && fs::exists(import_path, ec)) {
        guid = load_guid(import_path);
    }

    if (guid != 0) {
        unload_record(guid);
        static std::mutex registry_mutex;
        std::lock_guard   lock(registry_mutex);
        registry.remove(guid);
    }

    if (fs::exists(import_path, ec)) {
        try {
            std::ifstream sf(import_path);
            if (sf.good() && sf.peek() != std::ifstream::traits_type::eof()) {
                JSON meta = JSON::parse(sf);
                if (descriptor.importer.caches_path) {
                    if (meta.contains("thumbnail") && meta["thumbnail"].is_string()) {
                        Path thumb = Path(descriptor.importer.caches_path) / meta["thumbnail"].get<String>();
                        fs::remove(thumb, ec);
                    }
                    if (meta.contains("path") && meta["path"].is_string()) {
                        Path cache = Path(descriptor.importer.caches_path) / meta["path"].get<String>();
                        fs::remove(cache, ec);
                    }
                    if (meta.contains("dependencies") && meta["dependencies"].is_array()) {
                        for (const auto& dep : meta["dependencies"]) {
                            if (dep.is_object() && dep.contains("guid")) {
                                AssetID dep_guid = dep["guid"].get<AssetID>();
                                if (dep.contains("path") && dep["path"].is_string()) {
                                    Path dep_cache = Path(descriptor.importer.caches_path) / dep["path"].get<String>();
                                    fs::remove(dep_cache, ec);
                                }
                                unload_record(dep_guid);
                                static std::mutex registry_mutex;
                                std::lock_guard   lock(registry_mutex);
                                registry.remove(dep_guid);
                            }
                        }
                    }
                }
            }
        } catch (...) {
        }

        fs::remove(import_path, ec);
    }
}

bool AssetServer::delete_directory_assets(const Path& dir_path)
{
    std::error_code ec;

    Vector<Path> import_files;
    for (const auto& entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".import") {
            import_files.push_back(entry.path());
        }
    }

    for (const auto& import_file : import_files) {
        delete_metadata_and_caches(import_file, 0);
    }

    fs::remove_all(dir_path, ec);

    Path dir_import = get_metadata_path(dir_path);
    if (fs::exists(dir_import, ec)) {
        fs::remove(dir_import, ec);
    }

    return !ec;
}

bool AssetServer::delete_single_asset(const Path& full_path, const Path& rel_path)
{
    std::error_code ec;

    Path import_path = get_metadata_path(full_path);

    AssetID guid = 0;
    if (fs::exists(import_path, ec)) {
        guid = load_guid(import_path);
    }
    if (guid == 0) {
        guid = registry.get_guid(rel_path.string());
    }
    if (guid == 0) {
        guid = registry.get_guid(rel_path.generic_string());
    }

    delete_metadata_and_caches(import_path, guid);

    if (guid == 0) {
        static std::mutex registry_mutex;
        std::lock_guard   lock(registry_mutex);
        registry.remove(rel_path.string());
        registry.remove(rel_path.generic_string());
    }

    if (fs::exists(full_path, ec)) {
        fs::remove(full_path, ec);
    }

    return true;
}

bool AssetServer::delete_asset(const Path& path)
{
    auto [full_path, rel_path] = resolve_asset_path(path);

    std::error_code ec;

    bool success = false;
    if (fs::is_directory(full_path, ec)) {
        success = delete_directory_assets(full_path);
    } else {
        success = delete_single_asset(full_path, rel_path);
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
    std::lock_guard   lock(registry_mutex);
    registry.remove(guid);

    flush();
    return true;
}

auto AssetServer::resolve_destination_path(const Path& src_full, const Path& destination) const -> std::pair<Path, Path>
{
    Path dst_full;
    Path dst_rel;

    if (destination.is_absolute()) {
        dst_full = destination;
    } else {
        if (descriptor.importer.assets_path) {
            Path            assets_base = Path(descriptor.importer.assets_path);
            std::error_code ec;
            auto            rel = fs::relative(destination, assets_base, ec);
            if (!ec && !rel.empty() && *rel.begin() != "..") {
                dst_full = destination;
            } else {
                dst_full = assets_base / destination;
            }
        } else {
            dst_full = destination;
        }
    }

    std::error_code ec;
    if (fs::is_directory(dst_full, ec)) {
        dst_full /= src_full.filename();
    }

    if (dst_full.extension() == ".import") {
        dst_full.replace_extension("");
    }

    if (descriptor.importer.assets_path) {
        dst_rel = fs::relative(dst_full, Path(descriptor.importer.assets_path), ec);
        if (ec) dst_rel = dst_full;
    } else {
        dst_rel = dst_full;
    }

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

    for (const auto& entry : fs::recursive_directory_iterator(src_full, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".import") {
            AssetID guid = load_guid(entry.path());
            if (guid != 0) {
                Path asset_path = entry.path();
                asset_path.replace_extension("");
                Path rel_to_src = fs::relative(asset_path, src_full, ec);
                if (!ec) {
                    Path new_rel = dst_rel / rel_to_src;
                    moved_entries.push_back({guid, new_rel.string()});
                }
            }
        }
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
    std::lock_guard   lock(reg_mut);
    for (const auto& item : moved_entries) {
        registry.update(item.guid, item.new_rel_path, registry.get_type(item.guid), registry.get_dependencies(item.guid));
    }

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
        guid = registry.get_guid(src_rel.string());
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
        std::lock_guard   lock(reg_mut);
        registry.update(guid, dst_rel.string(), registry.get_type(guid), registry.get_dependencies(guid));
    }

    return true;
}

bool AssetServer::move_asset(const Path& source_path, const Path& destination_path)
{
    auto [src_full, src_rel] = resolve_asset_path(source_path);

    std::error_code ec;
    if (!fs::exists(src_full, ec)) {
        spdlog::error("AssetServer: Cannot move asset {}: source does not exist", src_full.string());
        return false;
    }

    auto [dst_full, dst_rel] = resolve_destination_path(src_full, destination_path);

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

    AssetRecord* record = nullptr;
    {
        std::shared_lock alock(*proc_ptr->mutex);
        auto             it2 = proc_ptr->assets.find(guid);
        if (it2 != proc_ptr->assets.end() && it2->second->data != nullptr && it2->second->refcnt > 0) {
            record = it2->second;
        }
    }

    if (!record) return;

    String path = String(registry.get_path(guid));

    JobScheduler::schedule(JobPriority::BACKGROUND, [this, proc_ptr, path, record, guid, type_id]() {
        try {
            void* new_data = proc_ptr->loader.load(descriptor.loader.assets, path.c_str());
            if (new_data) {
                void* old_data = nullptr;
                {
                    std::unique_lock alock(*proc_ptr->mutex);
                    old_data     = record->data;
                    record->data = new_data;
                }
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

void AssetServer::reimport_all(bool force)
{
    if (!descriptor.importer.assets_path) return;
    Path assets_dir(descriptor.importer.assets_path);
    if (!fs::exists(assets_dir)) return;

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(assets_dir, ec)) {
        if (entry.is_regular_file()) {
            Path rel_path = fs::relative(entry.path(), assets_dir);
            if (has_cooker_for(rel_path)) {
                import_asset(rel_path, force);
            }
        }
    }
}

void AssetServer::set_watching(bool enable)
{
    if (!descriptor.importer.assets_path) return;

    if (enable) {
        if (!watcher) {
            watcher = std::make_unique<AssetWatcher>(Path(descriptor.importer.assets_path));
            watcher->start([this](const Vector<AssetWatchEvent>& evts) {
                handle_watch_events(evts);
            });
            spdlog::info("AssetServer: Active asset directory watching started on: {}", Path(descriptor.importer.assets_path).string());
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

    AssetID guid = registry.get_guid(evt.old_path.string());
    if (guid == 0) return;

    static std::mutex reg_mut;
    std::lock_guard   lock(reg_mut);
    registry.update(guid, evt.path.string(), registry.get_type(guid), registry.get_dependencies(guid));

    // rename .import sidecar if it exists
    Path old_import = get_metadata_path(Path(descriptor.importer.assets_path) / evt.old_path);
    Path new_import = get_metadata_path(Path(descriptor.importer.assets_path) / evt.path);

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
                spdlog::info("AssetServer: Watcher detected {} file {}, queuing cook...",
                    evt.action == AssetWatchAction::Added ? "added" : "modified",
                    evt.path.string());
                import_asset(evt.path, false);
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
