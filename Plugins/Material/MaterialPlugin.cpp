#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/FileIO/VFSAPI.h>

#include <Lyra/Format/MaterialAsset.h>
#include <Lyra/Format/TextureAsset.h>

using namespace lyra;

static AssetServer* G_ASSET_SERVER = nullptr;

static Logger get_logger()
{
    static Logger logger = create_logger("Material", LogLevel::trace);
    return logger;
}

// helper to map string to gpucullmode
static GPUCullMode string_to_cull_mode(const String& str)
{
    // clang-format off
    if (str == "none")  return GPUCullMode::NONE;
    if (str == "front") return GPUCullMode::FRONT;
    // clang-format on
    return GPUCullMode::BACK;
}

static void configure_material_loader(AssetServer* manager, const JSON&)
{
    G_ASSET_SERVER = manager;
}

static bool load_material_schema(MaterialAsset* asset, const JSON& json, FSPath path)
{
    if (json.contains("schema")) {
        if (json["schema"].is_string()) {
            asset->schema = G_ASSET_SERVER->load_asset<MaterialSchema>(json["schema"].get<String>().c_str());
            if (!asset->schema.valid()) {
                get_logger()->error("failed to load material schema '{}' for material: {}", json["schema"].get<String>(), path);
                return false;
            }
        } else {
            get_logger()->error("'schema' field in material must be a string: {}", path);
            return false;
        }
    } else {
        get_logger()->error("material asset is missing mandatory 'schema' field: {}", path);
        return false;
    }
    return true;
}

static void load_material_textures(MaterialAsset* asset, const JSON& json, FSPath path)
{
    if (json.contains("textures")) {
        if (json["textures"].is_object()) {
            for (auto& [name, tex_path] : json["textures"].items()) {
                if (tex_path.is_string()) {
                    asset->params.textures[name] = G_ASSET_SERVER->load_asset<TextureAsset>(tex_path.get<String>().c_str());
                    if (!asset->params.textures[name].valid()) {
                        get_logger()->warn("failed to load texture '{}' for material: {}", tex_path.get<String>(), path);
                    }
                } else {
                    get_logger()->warn("texture path for '{}' must be a string in material: {}", name, path);
                }
            }
        } else {
            get_logger()->warn("'textures' field must be an object in material: {}", path);
        }
    }
}

static void load_material_constants(MaterialAsset* asset, const JSON& json, FSPath path)
{
    if (json.contains("constants")) {
        if (json["constants"].is_object()) {
            for (auto& [name, value] : json["constants"].items()) {
                if (value.is_array()) {
                    if (value.size() == 4) {
                        asset->params.constants[name] = Vector4(value[0], value[1], value[2], value[3]);
                    } else if (value.size() == 3) {
                        asset->params.constants[name] = Vector4(value[0], value[1], value[2], 1.0f);
                    } else {
                        get_logger()->warn("constant '{}' array must have size 3 or 4 in material: {}", name, path);
                    }
                } else if (value.is_number()) {
                    asset->params.constants[name] = Vector4(value.get<float>(), 0.0f, 0.0f, 0.0f);
                } else {
                    get_logger()->warn("constant '{}' has invalid format (must be array or number) in material: {}", name, path);
                }
            }
        } else {
            get_logger()->warn("'constants' field must be an object in material: {}", path);
        }
    }
}

static void load_material_overrides(MaterialAsset* asset, const JSON& json, FSPath path)
{
    if (json.contains("cull_mode")) {
        if (json["cull_mode"].is_string()) {
            asset->cull_mode = string_to_cull_mode(json["cull_mode"].get<String>());
        } else {
            get_logger()->warn("'cull_mode' must be a string in material: {}", path);
        }
    }

    if (json.contains("depth_write")) {
        if (json["depth_write"].is_boolean()) {
            asset->depth_write = json["depth_write"].get<bool>();
        } else {
            get_logger()->warn("'depth_write' must be a boolean in material: {}", path);
        }
    }

    if (json.contains("depth_test")) {
        if (json["depth_test"].is_boolean()) {
            asset->depth_test = json["depth_test"].get<bool>();
        } else {
            get_logger()->warn("'depth_test' must be a boolean in material: {}", path);
        }
    }
}

static void* load_material_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    if (content.empty()) {
        get_logger()->error("failed to read material file: {}", path);
        return nullptr;
    }

    JSON json;
    try {
        json = JSON::parse(content.begin(), content.end());
    } catch (const JSON::parse_error& e) {
        get_logger()->error("failed to parse material JSON at {}: {}", path, e.what());
        return nullptr;
    }

    if (!json.is_object()) {
        get_logger()->error("material JSON must be an object: {}", path);
        return nullptr;
    }

    if (!G_ASSET_SERVER) {
        get_logger()->error("asset server not configured for material loader!");
        return nullptr;
    }

    auto asset = new MaterialAsset();

    if (!load_material_schema(asset, json, path)) {
        delete asset;
        return nullptr;
    }

    load_material_textures(asset, json, path);
    load_material_constants(asset, json, path);
    load_material_overrides(asset, json, path);

    return asset;
}

static void unload_material_asset(void* asset)
{
    delete reinterpret_cast<MaterialAsset*>(asset);
}

static uint get_material_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".mat";
    }
    return 1;
}

namespace lyra::material::loader
{
    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_MATERIAL_VERBOSITY"));
    }

    void cleanup() {}

    auto create() -> AssetLoaderAPI
    {
        auto api                     = AssetLoaderAPI{};
        api.configure                = configure_material_loader;
        api.load                     = load_material_asset;
        api.unload                   = unload_material_asset;
        api.get_supported_extensions = get_material_extensions;
        return api;
    }
} // namespace lyra::material::loader
