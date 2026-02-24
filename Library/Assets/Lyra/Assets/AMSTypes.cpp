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
    JSON data = JSON::parse(f);
    if (data.contains("guid")) {
        guid = data["guid"].get<lyra::GUID>();
    }
    f.close();
    return guid;
}

static void save_json(const Path& path, const JSON& data, int indent = 2)
{
    std::ofstream f(path, std::ios::out);
    assert(f.good());
    f << data.dump(indent);
    f.close();
}

/**
 * @brief Initialize the AssetServer.
 */
AssetServer::AssetServer(const AMSDescriptor& descriptor)
    : descriptor(descriptor)
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
                processor.handler->unload(record->data);
            }
            delete record;
        }
        processor.assets.clear();
    }

    processors.clear();
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
        record->data           = proc_ptr->handler->load(descriptor.loader.assets, json);
        record->refcnt         = 1;
        proc_ptr->assets[guid] = record;
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

    std::unique_lock alock(*proc.mutex);

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
            if (record->refcnt == 0) {
                if (record->data) {
                    proc.handler->unload(record->data);
                }
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
    if (it == extensions.end()) return false;

    auto type_uuid = it->second;
    auto it2       = processors.find(type_uuid);
    if (it2 == processors.end()) return false;

    auto& proc      = it2->second;
    auto  type_name = proc.type;
    auto  handler   = proc.handler;

    guid             = 0ull;
    Path import_path = get_metadata_path(descriptor.importer.metadata_path / path);
    if (std::filesystem::exists(import_path))
        guid = load_guid(import_path);

    if (guid == 0)
        guid = random_guid();

    Path source_path = descriptor.importer.assets_path / path;
    Path target_path = descriptor.importer.generated_path / path;
    JSON metadata;
    metadata["version"] = "1";
    metadata["guid"]    = guid;
    metadata["time"]    = get_timestamp();
    metadata["type"]    = type_name;
    metadata["data"]    = handler->process(this, (OSPath)source_path.c_str(), (OSPath)target_path.c_str());

    save_json(import_path, metadata);
    return true;
}
