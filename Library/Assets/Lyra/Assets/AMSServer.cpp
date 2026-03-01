#include <ctime>
#include <fstream>
#include <filesystem>
#include <mutex>

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

AssetServer::AssetServer(const AMSDescriptor& descriptor)
    : descriptor(descriptor), pool(descriptor.workers)
{
    // load registry
    if (!registry.load(descriptor.registry)) {
        spdlog::info("AssetRegistry not found or failed to load. Rebuilding from source...");
        registry.rebuild(descriptor.importer.assets_path);
        registry.save(descriptor.registry);
    }
}

AssetServer::~AssetServer()
{
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

    std::unique_lock alock(*processor_ptr->mutex);

    auto it2 = processor_ptr->assets.find(guid);
    if (it2 == processor_ptr->assets.end()) {
        auto record                 = new AssetRecord();
        record->data                = nullptr;
        record->refcnt              = 1;
        processor_ptr->assets[guid] = record;

        String path = String(registry.get_path(guid));

        pool.detach_task([this, processor_ptr, path, record]() {
            record->data = processor_ptr->loader.load(descriptor.loader.assets, path.c_str());
        });
    } else {
        it2->second->refcnt++;
    }
    return RawAssetHandle{guid};
}

void AssetServer::unload_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    auto& processor = it->second;

    std::shared_lock alock(*processor->mutex);

    auto it2 = processor->assets.find(handle.uuid);
    if (it2 != processor->assets.end()) {
        it2->second->refcnt--;
    }
}

void AssetServer::clone_asset(AssetTypeID type_id, RawAssetHandle handle)
{
    auto it = processors.find(type_id);
    if (it == processors.end()) return;
    auto& processor = it->second;

    std::shared_lock alock(*processor->mutex);

    auto it2 = processor->assets.find(handle.uuid);
    if (it2 != processor->assets.end()) {
        it2->second->refcnt++;
    }
}

bool AssetServer::import_asset(const Path& path, AssetID& guid)
{
    auto ext = path.extension().string();
    auto it  = cooker_extensions.find(ext);
    if (it == cooker_extensions.end()) {
        spdlog::error("No cooker found for extension: {}", ext);
        return false;
    }

    auto cooker = it->second;

    Path source_path = Path(descriptor.importer.assets_path) / path;
    Path target_path = Path(descriptor.importer.caches_path) / path;
    Path import_path = get_metadata_path(source_path);

    guid = registry.get_guid(path.string());

    if (guid == 0 && fs::exists(import_path))
        guid = load_guid(import_path);

    if (guid == 0)
        guid = registry.generate_guid();

    // we use a separate task for cooking
    pool.detach_task([this, cooker, source_path, target_path, import_path, path, guid]() {
        JSON data = cooker->process((OSPath)source_path.c_str(), (OSPath)target_path.c_str());

        // find processor to get type_id and type_name
        AssetTypeID type_id   = 0;
        String      type_name = "";
        for (auto& [tid, proc] : processors) {
            for (auto& c : proc->cookers) {
                if (&c == cooker) {
                    type_id   = tid;
                    type_name = proc->type_name;
                    break;
                }
            }
            if (type_id != 0) break;
        }

        JSON metadata;
        metadata["version"] = "1";
        metadata["guid"]    = guid;
        metadata["type"]    = type_name;
        metadata["time"]    = get_timestamp();
        metadata["data"]    = data;

        save_json(import_path, metadata);

        // for simplicity, let's assume registry is not thread-safe and use a mutex
        static std::mutex registry_mutex;
        std::lock_guard   lock(registry_mutex);
        registry.update(guid, path.string(), type_id);
    });

    return true;
}
