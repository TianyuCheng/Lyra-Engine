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
    assert(f.good());
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

AssetServer::AssetServer(const AMSDescriptor& descriptor) : descriptor(descriptor)
{
    // do nothing
}

AssetServer::~AssetServer()
{
    // unload all existing assets
    for (auto& kv_processor : processors) {
        auto& processor = kv_processor.second;
        for (auto& kv : processor.assets)
            processor.handler->unload(kv.second);
    }

    // remove all asset processors
    processors.clear();
}

void* AssetServer::get_asset(UUID type_uuid, RawAssetHandle handle)
{
    // find asset processor
    auto it = processors.find(type_uuid);
    if (it == processors.end()) {
        spdlog::error("AssetType ({}) has not been registered!", to_string(type_uuid));
        return nullptr;
    }

    // find asset from collection
    const auto& proc = it->second;
    const auto  it2  = proc.assets.find(handle.guid);
    if (it2 == proc.assets.end())
        return nullptr;

    return it2->second;
}

RawAssetHandle AssetServer::load_asset(UUID type_uuid, FSPath path)
{
    // sanity check if given asset has been registered
    auto it = processors.find(type_uuid);
    if (it == processors.end()) {
        spdlog::error("AssetType ({}) has not been registered!", to_string(type_uuid));
        return RawAssetHandle();
    }

    // prepare metadata path
    auto  metadata_file   = get_metadata_path(Path(path));
    auto  metadata_vfs    = metadata_file.string();
    auto& metadata_loader = descriptor.loader.metadata;

    // check if file exists
    if (metadata_loader->exists(metadata_vfs.c_str())) {
        spdlog::error("Failed to find metadata for {}", path);
        throw std::runtime_error("Failed to find asset metadata!");
    }

    // load asset guid
    auto data = metadata_loader->read<char>(metadata_vfs.c_str());
    auto json = JSON::parse(data.begin(), data.end());
    auto guid = json["guid"].template get<lyra::GUID>();

    // issue asset load (if necessary)
    // TODO: move asset loading to worker threads
    // TODO: check asset timestamp, check if reload is needed
    auto& proc = it->second;
    auto  it2  = proc.assets.find(guid);
    if (it2 == proc.assets.end()) {
        proc.assets[guid] = proc.handler->load(descriptor.loader.assets, json);
    }

    return RawAssetHandle{guid};
}

void AssetServer::unload_asset(UUID type_uuid, RawAssetHandle handle)
{
    // sanity check if given asset has been registered
    auto it = processors.find(type_uuid);
    if (it == processors.end()) {
        spdlog::error("AssetType ({}) has not been registered!", to_string(type_uuid));
        return;
    }

    // unload the asset and remove from collections
    auto& proc = it->second;
    auto  it2  = proc.assets.find(handle.guid);
    if (it2 == proc.assets.end()) {
        proc.handler->unload(it2->second);
        proc.assets.erase(it2);
    }
}

bool AssetServer::import_asset(const Path& path, lyra::GUID& guid)
{
    // sanity check if extensions has been registered
    auto ext = path.extension().string();
    auto it  = extensions.find(ext);
    if (it == extensions.end()) {
        spdlog::error("AssetServer cannot handle files with extension: {}", ext);
        return false;
    }

    // sanity check if given asset has been registered
    auto it2 = processors.find(it->second);
    if (it2 == processors.end()) {
        spdlog::error("AssetType ({}) has not been registered!", to_string(it->second));
        return false;
    }

    // asset processor
    auto& proc = it2->second;

    // asset GUID
    guid = 0ull;

    // check if metadata exists, reuse uuid if possible
    Path import_path = get_metadata_path(descriptor.importer.metadata_path / path);
    if (std::filesystem::exists(import_path))
        guid = load_guid(import_path);

    // choose a random uuid if no uuid has ever been assigned
    if (guid != 0)
        guid = random_guid();

    // compose metadata
    Path source_path = descriptor.importer.assets_path / path;
    Path target_path = descriptor.importer.generated_path / path;
    JSON metadata;
    metadata["version"] = "1";
    metadata["guid"]    = guid;
    metadata["time"]    = get_timestamp();
    metadata["type"]    = proc.type;
    metadata["data"]    = proc.handler->process(this, source_path.c_str(), target_path.c_str());

    // write metadata side by side with the imported asset
    save_json(import_path, metadata);
    return true;
}
