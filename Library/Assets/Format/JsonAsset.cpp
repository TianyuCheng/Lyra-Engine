#include <fstream>
#include <iomanip>
#include <Lyra/FileIO/VFSAPI.h>

#include <Lyra/Assets/Format/JsonAsset.h>

using namespace lyra;

static void* load_json_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    return new JsonAsset{JSON::parse(content.begin(), content.end())};
}

static uint get_json_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".json";
    }
    return 1;
}

static bool json_process(JSON& metadata, OSPath source_path, OSPath)
{
    metadata["path"] = reinterpret_cast<const char*>(source_path);
    return true;
}

static bool save_json_asset(const void* raw_asset, OSPath path)
{
    const auto* asset = reinterpret_cast<const JsonAsset*>(raw_asset);
    if (!asset) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << std::setw(4) << asset->content << '\n';
    file.close();
    return !file.fail();
}

AssetLoaderAPI JsonAsset::loader()
{
    auto api                     = AssetLoaderAPI{};
    api.configure                = nullptr;
    api.load                     = load_json_asset;
    api.unload                   = [](void* asset) { delete reinterpret_cast<JsonAsset*>(asset); };
    api.get_supported_extensions = get_json_extensions;
    return api;
}

AssetCookerAPI JsonAsset::cooker()
{
    auto api                     = AssetCookerAPI{};
    api.configure                = nullptr;
    api.process                  = json_process;
    api.get_supported_extensions = get_json_extensions;
    return api;
}

AssetSaverAPI JsonAsset::saver()
{
    auto api                     = AssetSaverAPI{};
    api.configure                = nullptr;
    api.save                     = save_json_asset;
    api.get_supported_extensions = get_json_extensions;
    return api;
}
