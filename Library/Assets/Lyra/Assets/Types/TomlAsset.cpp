#include <Lyra/FileIO/VFSAPI.h>

#include "TomlAsset.h"

using namespace lyra;

static void* load_toml_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    auto view    = StringView(content.data(), content.size() - 1);
    return new TomlAsset{toml::parse(view)};
}

static uint get_toml_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".toml";
    }
    return 1;
}

static bool toml_process(JSON& metadata, OSPath source_path, OSPath)
{
    metadata["path"] = reinterpret_cast<const char*>(source_path);
    return true;
}

AssetLoaderAPI TomlAsset::loader()
{
    auto api                     = AssetLoaderAPI{};
    api.configure                = nullptr;
    api.load                     = load_toml_asset;
    api.unload                   = [](void* asset) { delete reinterpret_cast<TomlAsset*>(asset); };
    api.get_supported_extensions = get_toml_extensions;
    return api;
}

AssetCookerAPI TomlAsset::cooker()
{
    auto api                     = AssetCookerAPI{};
    api.configure                = nullptr;
    api.process                  = toml_process;
    api.get_supported_extensions = get_toml_extensions;
    return api;
}
