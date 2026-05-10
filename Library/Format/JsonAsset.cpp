#include <Lyra/FileIO/VFSAPI.h>

#include <Lyra/Format/JsonAsset.h>

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
