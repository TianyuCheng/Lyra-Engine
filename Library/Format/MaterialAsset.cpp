#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Format/TextureAsset.h>
#include <Lyra/Format/MaterialAsset.h>

using namespace lyra;

static AssetServer* G_ASSET_SERVER = nullptr;

// Helper to map string to GPUCullMode
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

// --- MaterialAsset Loader ---

static void* load_material_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    auto json    = JSON::parse(content.begin(), content.end());

    auto asset = new MaterialAsset();

    if (json.contains("schema") && G_ASSET_SERVER) {
        asset->schema = G_ASSET_SERVER->load_asset<MaterialSchema>(json["schema"].get<String>().c_str());
    }

    if (json.contains("textures") && G_ASSET_SERVER) {
        for (auto& [name, tex_path] : json["textures"].items()) {
            asset->params.textures[name] = G_ASSET_SERVER->load_asset<TextureAsset>(tex_path.get<String>().c_str());
        }
    }

    if (json.contains("constants")) {
        for (auto& [name, value] : json["constants"].items()) {
            if (value.is_array() && value.size() == 4) {
                asset->params.constants[name] = Vector4(value[0], value[1], value[2], value[3]);
            } else if (value.is_number()) {
                asset->params.constants[name] = Vector4(value.get<float>(), 0.0f, 0.0f, 0.0f);
            }
        }
    }

    if (json.contains("cull_mode")) {
        asset->cull_mode = string_to_cull_mode(json["cull_mode"].get<String>());
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
