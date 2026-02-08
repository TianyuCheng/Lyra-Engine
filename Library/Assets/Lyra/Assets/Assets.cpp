#include <Lyra/Assets/Assets.h>

using namespace lyra;

using TexturePlugin = Plugin<AssetHandlerAPI>;

static Own<TexturePlugin> TEXTURE_PLUGIN;

// Dummy asset process implementation,
// which does absolutely nothing except for saving the asset path in metadata.
// This is suitable for simple assets that do not need an importing process.
template <typename AssetType>
JSON process_asset(AssetServer* manager, OSPath source_path, OSPath target_path)
{
    MAYBE_UNUSED(manager);
    MAYBE_UNUSED(target_path);

    JSON metadata    = {};
    metadata["path"] = source_path;
    return metadata;
}

// Simple asset unload implementation, which only uses C++ pointer deletion.
template <typename AssetType>
void unload_asset(void* asset)
{
    auto typed_asset = reinterpret_cast<AssetType*>(asset);
    delete typed_asset;
}

static void* load_json_asset(FileLoader* loader, const JSON& metadata)
{
    auto path    = metadata["path"].template get<String>();
    auto content = loader->read<char>(path.c_str());
    return new JsonAsset{JSON::parse(content.begin(), content.end())};
}

static void* load_text_asset(FileLoader* loader, const JSON& metadata)
{
    auto path    = metadata["path"].template get<String>();
    auto content = loader->read<char>(path.c_str());
    return new TextAsset{String(content.begin(), content.end())};
}

static void* load_toml_asset(FileLoader* loader, const JSON& metadata)
{
    auto path    = metadata["path"].template get<String>();
    auto content = loader->read<char>(path.c_str());
    auto view    = StringView(content.data(), content.size() - 1);
    return new TomlAsset{toml::parse(view)};
}

AssetHandlerAPI TextAsset::handler()
{
    auto api    = AssetHandlerAPI{};
    api.load    = load_text_asset;
    api.unload  = unload_asset<TextAsset>;
    api.process = process_asset<TextAsset>;
    return api;
}

AssetHandlerAPI JsonAsset::handler()
{
    auto api    = AssetHandlerAPI{};
    api.load    = load_json_asset;
    api.unload  = unload_asset<JsonAsset>;
    api.process = process_asset<JsonAsset>;
    return api;
}

AssetHandlerAPI TomlAsset::handler()
{
    auto api    = AssetHandlerAPI{};
    api.load    = load_toml_asset;
    api.unload  = unload_asset<TomlAsset>;
    api.process = process_asset<TomlAsset>;
    return api;
}

AssetHandlerAPI TextureAsset::handler()
{
    if (!TEXTURE_PLUGIN)
        TEXTURE_PLUGIN = std::make_unique<TexturePlugin>("lyra-ktx");

    return *TEXTURE_PLUGIN->get_api();
}
