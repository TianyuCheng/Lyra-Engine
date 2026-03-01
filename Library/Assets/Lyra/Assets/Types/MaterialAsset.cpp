#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Assets/AMSServer.h>

#include "MaterialAsset.h"
#include "TextureAsset.h"

using namespace lyra;

static AssetServer* G_ASSET_SERVER = nullptr;

static void configure_material_loader(AssetServer* manager, const JSON&)
{
    G_ASSET_SERVER = manager;
}

static void* load_material_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    auto json    = JSON::parse(content.begin(), content.end());

    auto asset = new MaterialAsset();

    if (json.contains("shader_id")) {
        asset->shader_id = json["shader_id"].get<String>();
    }

    if (json.contains("textures") && G_ASSET_SERVER) {
        for (auto& [name, tex_path] : json["textures"].items()) {
            asset->textures[name] = G_ASSET_SERVER->load_asset<TextureAsset>(tex_path.get<String>().c_str());
        }
    }

    if (json.contains("constants")) {
        for (auto& [name, value] : json["constants"].items()) {
            if (value.is_array() && value.size() == 4) {
                asset->constants[name] = Vector4(value[0], value[1], value[2], value[3]);
            } else if (value.is_number()) {
                asset->constants[name] = Vector4(value.get<float>(), 0.0f, 0.0f, 0.0f);
            }
        }
    }

    if (json.contains("blend_mode")) {
        auto mode = json["blend_mode"].get<String>();
        if (mode == "OPAQUE")
            asset->blend_mode = MaterialAsset::BlendMode::OPAQUE;
        else if (mode == "MASK")
            asset->blend_mode = MaterialAsset::BlendMode::MASK;
        else if (mode == "BLEND")
            asset->blend_mode = MaterialAsset::BlendMode::BLEND;
    }

    if (json.contains("cull_mode")) {
        auto mode = json["cull_mode"].get<String>();
        if (mode == "NONE")
            asset->cull_mode = GPUCullMode::NONE;
        else if (mode == "FRONT")
            asset->cull_mode = GPUCullMode::FRONT;
        else if (mode == "BACK")
            asset->cull_mode = GPUCullMode::BACK;
    }

    if (json.contains("depth_write")) asset->depth_write = json["depth_write"].get<bool>();
    if (json.contains("depth_test")) asset->depth_test = json["depth_test"].get<bool>();

    return asset;
}

static uint get_material_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".mat";
    }
    return 1;
}

AssetLoaderAPI MaterialAsset::loader()
{
    auto api                     = AssetLoaderAPI{};
    api.configure                = configure_material_loader;
    api.load                     = load_material_asset;
    api.unload                   = [](void* asset) { delete reinterpret_cast<MaterialAsset*>(asset); };
    api.get_supported_extensions = get_material_extensions;
    return api;
}
