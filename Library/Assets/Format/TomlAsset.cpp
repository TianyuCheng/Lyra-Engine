#include <fstream>
#include <Lyra/FileIO/VFSAPI.h>

#include <Lyra/Assets/Format/TomlAsset.h>

using namespace lyra;

static void* load_toml_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    if (content.empty()) {
        return new TomlAsset{};
    }
    size_t size = content.size();
    if (content.back() == '\0') {
        size--;
    }
    auto view = StringView(content.data(), size);
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

static bool save_toml_asset(const void* raw_asset, OSPath path)
{
    const auto* asset = reinterpret_cast<const TomlAsset*>(raw_asset);
    if (!asset) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << asset->content;
    file.close();
    return !file.fail();
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

AssetSaverAPI TomlAsset::saver()
{
    auto api                     = AssetSaverAPI{};
    api.configure                = nullptr;
    api.save                     = save_toml_asset;
    api.get_supported_extensions = get_toml_extensions;
    return api;
}
