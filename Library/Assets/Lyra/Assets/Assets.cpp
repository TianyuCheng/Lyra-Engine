#include <Lyra/Assets/Assets.h>

using namespace lyra;

/**
 * @brief Default asset processing implementation.
 * Simply records the source path in the metadata.
 */
template <typename AssetType>
JSON process_asset(AssetServer* manager, OSPath source_path, OSPath target_path)
{
    MAYBE_UNUSED(manager);
    MAYBE_UNUSED(target_path);

    JSON metadata    = {};
    metadata["path"] = source_path;
    return metadata;
}

/**
 * @brief Default asset unloading implementation.
 */
template <typename AssetType>
void unload_asset(AssetServer* manager, void* asset)
{
    MAYBE_UNUSED(manager);

    auto typed_asset = reinterpret_cast<AssetType*>(asset);
    delete typed_asset;
}

/**
 * @brief Built-in loader for JSON assets.
 */
static void* load_json_asset(AssetServer* manager, FileLoader* loader, const JSON& metadata)
{
    MAYBE_UNUSED(manager);

    auto path    = metadata["path"].template get<String>();
    auto content = loader->read<char>(path.c_str());
    return new JsonAsset{JSON::parse(content.begin(), content.end())};
}

/**
 * @brief Built-in loader for text assets.
 */
static void* load_text_asset(AssetServer* manager, FileLoader* loader, const JSON& metadata)
{
    MAYBE_UNUSED(manager);

    auto path    = metadata["path"].template get<String>();
    auto content = loader->read<char>(path.c_str());
    return new TextAsset{String(content.begin(), content.end())};
}

/**
 * @brief Built-in loader for TOML assets.
 */
static void* load_toml_asset(AssetServer* manager, FileLoader* loader, const JSON& metadata)
{
    MAYBE_UNUSED(manager);

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
    using TexturePlugin = Plugin<AssetHandlerAPI>;
    static Own<TexturePlugin> TEXTURE_PLUGIN;

    if (!TEXTURE_PLUGIN)
        TEXTURE_PLUGIN = std::make_unique<TexturePlugin>("lyra-texture");

    return *TEXTURE_PLUGIN->get_api();
}
