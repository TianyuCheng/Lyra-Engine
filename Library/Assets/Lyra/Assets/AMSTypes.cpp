#include <ctime>
#include <fstream>

#include <Lyra/Assets/AMSTypes.h>

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

static lyra::GUID load_guid(const Path& path)
{
    lyra::GUID guid = 0;

    std::ifstream f(path, std::ios::in);
    if (!f.good()) return 0;

    // check if file is empty to avoid json parse error
    if (f.peek() == std::ifstream::traits_type::eof())
        return 0;

    try {
        JSON data = JSON::parse(f);
        if (data.contains("guid")) {
            guid = data["guid"].get<lyra::GUID>();
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

static JSON default_process(AssetServer*, OSPath source_path, OSPath)
{
    JSON metadata;
    metadata["path"] = Path(source_path).string();
    return metadata;
}

/**
 * @brief Initialize the AssetServer.
 */
AssetServer::AssetServer(const AMSDescriptor& descriptor)
    : descriptor(descriptor), pool(descriptor.workers)
{
    // do nothing
}

/**
 * @brief Destroy the AssetServer and unload all managed assets.
 */
AssetServer::~AssetServer()
{
    // unload all existing assets regardless of ref count
    for (auto& kv_processor : processors) {
        auto&            processor = kv_processor.second;
        std::unique_lock alock(*processor.mutex);
        for (auto& kv : processor.assets) {
            auto record = kv.second;
            if (record->data) {
                processor.handler->unload(this, record->data);
            }
            delete record;
        }
        processor.assets.clear();
    }

    processors.clear();
}

/**
 * @brief Register a new asset type with early validation.
 */
void AssetServer::register_processor(UUID uuid, AssetProcessor&& proc, const InitList<CString>& extensions, const JSON& options)
{
    // load and unload must exist
    assert(proc.handler->load != nullptr);
    assert(proc.handler->unload != nullptr);

    // patch dummy process if missing
    if (proc.handler->process == nullptr) {
        proc.handler->process = default_process;
    }

    // configure asset processor
    if (proc.handler->configure) {
        proc.handler->configure(options);
    }

    auto type_uuid = uuid;
    processors.emplace(type_uuid, std::move(proc));

    for (const auto& extension : extensions) {
        this->extensions.emplace(extension, type_uuid);
    }
}

/**
 * @brief Internal helper to safely retrieve an asset pointer.
 */
void* AssetServer::get_asset(UUID type_uuid, RawAssetHandle handle)
{
    auto it = processors.find(type_uuid);
    if (it == processors.end()) return nullptr;
    const auto& proc = it->second;

    std::shared_lock alock(*proc.mutex);

    const auto it2 = proc.assets.find(handle.guid);
    if (it2 == proc.assets.end()) return nullptr;
    return it2->second->data;
}

/**
 * @brief Load an asset and increment its handle reference count.
 */
RawAssetHandle AssetServer::load_asset(UUID type_uuid, FSPath path)
{
    auto it = processors.find(type_uuid);
    if (it == processors.end()) return RawAssetHandle();

    AssetProcessor* proc_ptr = &it->second;

    auto  metadata_file   = get_metadata_path(Path(path));
    auto  metadata_vfs    = metadata_file.string();
    auto& metadata_loader = descriptor.loader.metadata;

    if (!metadata_loader->exists(metadata_vfs.c_str())) return RawAssetHandle();

    auto data = metadata_loader->read<char>(metadata_vfs.c_str());
    auto json = JSON::parse(data.begin(), data.end());
    auto guid = json["guid"].template get<lyra::GUID>();

    std::unique_lock alock(*proc_ptr->mutex);

    auto it2 = proc_ptr->assets.find(guid);
    if (it2 == proc_ptr->assets.end()) {
        auto record            = new AssetRecord();
        record->data           = nullptr;
        record->refcnt         = 1;
        proc_ptr->assets[guid] = record;
        pool.detach_task([this, proc_ptr, json, record]() {
            record->data = proc_ptr->handler->load(this, descriptor.loader.assets, json);
        });
    } else {
        it2->second->refcnt++;
    }
    return RawAssetHandle{guid};
}

/**
 * @brief Decrement reference count for an asset handle.
 */
void AssetServer::unload_asset(UUID type_uuid, RawAssetHandle handle)
{
    auto it = processors.find(type_uuid);
    if (it == processors.end()) return;
    auto& proc = it->second;

    std::shared_lock alock(*proc.mutex);

    auto it2 = proc.assets.find(handle.guid);
    if (it2 != proc.assets.end()) {
        it2->second->refcnt--;
    }
}

/**
 * @brief Remove assets with zero references from memory.
 */
void AssetServer::purge()
{
    for (auto& kv_processor : processors) {
        auto& proc = kv_processor.second;

        std::unique_lock alock(*proc.mutex);
        for (auto it = proc.assets.begin(); it != proc.assets.end();) {
            auto record = it->second;
            // only purge if refcnt is 0 and it's not currently loading (data != nullptr)
            if (record->refcnt == 0 && record->data != nullptr) {
                proc.handler->unload(this, record->data);
                delete record;
                proc.assets.erase(it++);
            } else {
                ++it;
            }
        }
    }
}

/**
 * @brief Cook and import a raw source asset into the engine's generated format.
 */
bool AssetServer::import_asset(const Path& path, lyra::GUID& guid)
{
    auto ext = path.extension().string();
    auto it  = extensions.find(ext);
    if (it == extensions.end()) {
        spdlog::error("No processor found for extension: {}", ext);
        return false;
    }

    auto type_uuid = it->second;
    auto it2       = processors.find(type_uuid);
    if (it2 == processors.end()) {
        spdlog::error("Processor for type {} not found!", to_string(type_uuid));
        return false;
    }

    auto& proc      = it2->second;
    auto  type_name = proc.type;
    auto  handler   = proc.handler;

    Path source_path = Path(descriptor.importer.assets_path) / path;
    Path target_path = Path(descriptor.importer.caches_path) / path;
    Path import_path = get_metadata_path(source_path);

    guid = 0ull;

    if (std::filesystem::exists(import_path))
        guid = load_guid(import_path);

    if (guid == 0)
        guid = random_guid();

    JSON metadata;
    metadata["version"] = "1";
    metadata["guid"]    = guid;
    metadata["time"]    = get_timestamp();
    metadata["type"]    = type_name;
    metadata["data"]    = handler->process(this, (OSPath)source_path.c_str(), (OSPath)target_path.c_str());

    save_json(import_path, metadata);
    return true;
}
