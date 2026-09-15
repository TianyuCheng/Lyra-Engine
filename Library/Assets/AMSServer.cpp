#include <ctime>
#include <mutex>
#include <fstream>
#include <filesystem>

#include <absl/strings/ascii.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Assets/AMSServer.h>

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
    auto sctp = std::chrono::file_clock::to_sys(ftime);
    return std::chrono::system_clock::to_time_t(sctp);
#else
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
#endif
}

AssetServer::AssetServer(const AMSDescriptor& descriptor)
    : descriptor(descriptor), pool(descriptor.workers)
{
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
}

AssetServer::~AssetServer()
{
    if (watcher) {
        watcher->stop();
    }
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

    const auto it2 = processor->assets.find(handle.uuid);
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
        // recursively load dependencies
        const auto& deps = registry.get_dependencies(guid);
        for (auto dep_guid : deps) {
            AssetTypeID dep_type = registry.get_type(dep_guid);
            if (dep_type != 0) {
                load_asset(dep_type, dep_guid);
            }
        }
    }

    if (is_new_entry) {
        String       path   = String(registry.get_path(guid));
        AssetRecord* record = nullptr;
        {
            std::shared_lock alock(*processor_ptr->mutex);
            record = processor_ptr->assets[guid];
        }

        pool.detach_task([this, processor_ptr, path, record]() {
            record->data = processor_ptr->loader.load(descriptor.loader.assets, path.c_str());
        });
    }

    return RawAssetHandle{guid};
}

void AssetServer::unload_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    auto& processor = it->second;

    bool should_unload_deps = false;

    {
        std::unique_lock alock(*processor->mutex);

        auto it2 = processor->assets.find(handle.uuid);
        if (it2 != processor->assets.end()) {
            it2->second->refcnt--;
            if (it2->second->refcnt == 0) {
                should_unload_deps = true;
            }
        }
    }

    if (should_unload_deps) {
        // recursively unload dependencies
        const auto& deps = registry.get_dependencies(handle.uuid);
        for (auto dep_guid : deps) {
            AssetTypeID dep_type = registry.get_type(dep_guid);
            if (dep_type != 0) {
                unload_asset(dep_type, RawAssetHandle{dep_guid});
            }
        }
    }
}

void AssetServer::clone_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    auto& processor = it->second;

    std::unique_lock alock(*processor->mutex);

    auto it2 = processor->assets.find(handle.uuid);
    if (it2 != processor->assets.end()) {
        it2->second->refcnt++;
    }
}

Future<AssetID> AssetServer::import_asset(const Path& path, bool force)
{
    auto ext = absl::AsciiStrToLower(path.extension().string());

    auto it = cooker_extensions.find(ext);
    if (it == cooker_extensions.end()) {
        spdlog::error("No cooker found for extension: {}", ext);
        Promise<AssetID> p;
        p.set_value(0);
        return p.get_future();
    }

    auto cooker = it->second;

    Path source_path = Path(descriptor.importer.assets_path) / path;
    Path import_path = get_metadata_path(source_path);

    AssetID guid = registry.get_guid(path.string());

    if (guid == 0 && fs::exists(import_path))
        guid = load_guid(import_path);

    if (guid == 0)
        guid = registry.generate_guid();

    // Incremental cooking check
    if (!force && fs::exists(import_path) && fs::exists(source_path)) {
        time_t meta_time = load_metadata_time(import_path);
        time_t file_time = get_file_mtime(source_path);
        if (meta_time > 0 && meta_time >= file_time) {
            Promise<AssetID> p;
            p.set_value(guid);
            return p.get_future();
        }
    }

    pending_cooks++;

    // we use a separate task for cooking
    return pool.submit_task([this, cooker, source_path, import_path, path, guid]() -> AssetID {
        {
            std::lock_guard lock(pipeline_mutex);
            current_cooking_asset = path.string();
        }

        auto finish_task = [this, &path](bool success) {
            std::lock_guard lock(pipeline_mutex);
            if (current_cooking_asset == path.string()) {
                current_cooking_asset.clear();
            }
            if (success) {
                completed_cooks++;
            } else {
                failed_cooks++;
            }
            pending_cooks--;
        };

        try {
            // find processor to get type_id and type_name
            AssetTypeID type_id = lyra::execute([&]() {
                for (auto& [tid, proc] : processors)
                    for (auto& c : proc->cookers)
                        if (&c == cooker)
                            return tid;
                return AssetTypeID(0);
            });

            if (type_id == 0) {
                spdlog::error("No processor registered for cooker cooking {}", path.string());
                finish_task(false);
                return AssetID(0);
            }

            // populate metadata and import (preprocess) the asset
            JSON metadata;
            metadata["guid"]    = guid;
            metadata["version"] = "1";
            metadata["type"]    = type_id;
            metadata["time"]    = get_timestamp();

            bool cook_success = false;
            try {
                cook_success = cooker->process(metadata, (OSPath)source_path.c_str(), descriptor.importer.caches_path);
            } catch (const std::exception& e) {
                spdlog::error("Exception in cooker processing {}: {}", path.string(), e.what());
                cook_success = false;
            } catch (...) {
                spdlog::error("Unknown exception in cooker processing {}", path.string());
                cook_success = false;
            }

            if (!cook_success) {
                spdlog::error("Failed to cook asset: {}", path.string());
                // Remove incomplete .import file if it exists so corrupt metadata is not loaded
                std::error_code ec;
                if (fs::exists(import_path, ec)) {
                    fs::remove(import_path, ec);
                }
                finish_task(false);
                return AssetID(0);
            }

            save_json(import_path, metadata);

            Vector<AssetID> dependencies;
            if (metadata.contains("dependencies") && metadata["dependencies"].is_array()) {
                dependencies = metadata["dependencies"].get<Vector<AssetID>>();
            }

            // for simplicity, let's assume registry is not thread-safe and use a mutex
            static std::mutex registry_mutex;
            {
                std::lock_guard lock(registry_mutex);
                registry.update(guid, path.string(), type_id, dependencies);
            }

            finish_task(true);

            // Trigger hot-reload if the asset is currently loaded in memory
            reload_asset(guid);

            return guid;
        } catch (const std::exception& e) {
            spdlog::error("Unexpected exception while importing {}: {}", path.string(), e.what());
            finish_task(false);
            return AssetID(0);
        } catch (...) {
            spdlog::error("Unknown fatal error while importing {}", path.string());
            finish_task(false);
            return AssetID(0);
        }
    });
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

    pool.detach_task([this, proc_ptr, path, record, guid, type_id]() {
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

                std::lock_guard lock(pipeline_mutex);
                queued_reloaded_assets.emplace_back(guid, type_id);
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
    {
        std::lock_guard lock(pipeline_mutex);
        stats.current_asset = current_cooking_asset;
    }
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
    bool                                    has_fs_changes = false;

    {
        std::lock_guard lock(pipeline_mutex);
        if (!queued_reloaded_assets.empty()) {
            reloads = std::move(queued_reloaded_assets);
            queued_reloaded_assets.clear();
        }
        if (!queued_fs_events.empty()) {
            has_fs_changes = true;
            queued_fs_events.clear();
        }
    }

    for (const auto& [guid, type_id] : reloads) {
        if (on_asset_reloaded) {
            on_asset_reloaded(guid, type_id);
        }
    }

    if (has_fs_changes && on_fs_changed) {
        on_fs_changed();
    }
}

void AssetServer::set_on_asset_reloaded(AssetReloadCallback callback)
{
    std::lock_guard lock(pipeline_mutex);
    on_asset_reloaded = std::move(callback);
}

void AssetServer::set_on_filesystem_changed(FileSystemChangeCallback callback)
{
    std::lock_guard lock(pipeline_mutex);
    on_fs_changed = std::move(callback);
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
            spdlog::info("AssetServer: Watcher detected rename from {} to {}",
                evt.old_path.string(), evt.path.string());

            if (!evt.old_path.empty()) {
                AssetID guid = registry.get_guid(evt.old_path.string());
                if (guid != 0) {
                    static std::mutex reg_mut;
                    std::lock_guard   lock(reg_mut);
                    registry.update(guid, evt.path.string(), registry.get_type(guid), registry.get_dependencies(guid));

                    // Rename .import sidecar if it exists
                    Path            old_import = get_metadata_path(Path(descriptor.importer.assets_path) / evt.old_path);
                    Path            new_import = get_metadata_path(Path(descriptor.importer.assets_path) / evt.path);
                    std::error_code ec;
                    if (fs::exists(old_import)) {
                        fs::rename(old_import, new_import, ec);
                    }
                }
            }
            fs_changed = true;
        } else if (evt.action == AssetWatchAction::Removed) {
            spdlog::info("AssetServer: Watcher detected file removed: {}", evt.path.string());
            fs_changed = true;
        }
    }

    if (fs_changed) {
        std::lock_guard lock(pipeline_mutex);
        for (const auto& e : events) {
            queued_fs_events.push_back(e);
        }
    }
}

bool AssetServer::save_asset(AssetTypeID type_id, const void* asset, OSPath path)
{
    auto it = processors.find(type_id);
    if (it == processors.end() || !it->second->saver.has_value()) {
        spdlog::error("No saver registered for asset type ID: {:#x}", type_id);
        return false;
    }

    const auto& saver = it->second->saver.value();
    if (!saver.save) {
        spdlog::error("Saver for asset type ID {:#x} has no save function defined", type_id);
        return false;
    }

    return saver.save(asset, path);
}
