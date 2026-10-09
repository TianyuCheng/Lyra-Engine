#include <fstream>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/FileSystem/VFSAPI.h>
#include <Lyra/Assets/Format/MaterialAsset.h>
#include <Lyra/Assets/Format/TextureAsset.h>

// tinyusdz — only in .cpp, never in public headers
#include <tinyusdz.hh>
#include <usda-reader.hh>
#include <stream-reader.hh>

using namespace lyra;

static AssetServer* G_ASSET_SERVER = nullptr;

static Logger get_logger()
{
    static Logger logger = create_logger("Material", LogLevel::trace);
    return logger;
}

static void configure_material_loader(AssetServer* manager, const JSON&)
{
    G_ASSET_SERVER = manager;
}

static void* load_material_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    if (content.empty()) {
        get_logger()->error("failed to read material file: {}", path);
        return nullptr;
    }

    tinyusdz::StreamReader sr(
        reinterpret_cast<const uint8_t*>(content.data()),
        content.size(),
        /* swap_endian = */ false);

    tinyusdz::usda::USDAReader reader(&sr);
    if (!reader.read() || !reader.reconstruct_stage()) {
        get_logger()->error("failed to parse .material USDA at {}: {}", path, reader.get_error());
        return nullptr;
    }

    const tinyusdz::Stage& stage = reader.get_stage();
    auto                   asset = new MaterialAsset();

    for (const auto& root : stage.root_prims()) {
        const tinyusdz::Material* mat = root.as<tinyusdz::Material>();
        if (!mat) continue;

        for (const auto& [prop_name, prop] : mat->props) {
            if (!prop.is_attribute()) continue;
            const auto& attr = prop.get_attribute();

            if (prop_name == "lyra:blendMode") {
                std::string s;
                if (attr.get_value(&s)) {
                    if (s == "mask")
                        asset->blend_mode = MaterialBlendMode::MASK;
                    else if (s == "blend")
                        asset->blend_mode = MaterialBlendMode::BLEND;
                    else
                        asset->blend_mode = MaterialBlendMode::OPAQUE;
                }
            } else if (prop_name == "lyra:cullMode") {
                std::string s;
                if (attr.get_value(&s)) {
                    if (s == "none")
                        asset->cull_mode = GPUCullMode::NONE;
                    else if (s == "front")
                        asset->cull_mode = GPUCullMode::FRONT;
                    else
                        asset->cull_mode = GPUCullMode::BACK;
                }
            } else if (prop_name == "lyra:depthWrite") {
                bool b = true;
                if (attr.get_value(&b)) asset->depth_write = b;
            } else if (prop_name == "lyra:depthTest") {
                bool b = true;
                if (attr.get_value(&b)) asset->depth_test = b;
            }
        }

        for (const auto& child : root.children()) {
            const tinyusdz::Shader* shader = child.as<tinyusdz::Shader>();
            if (!shader) continue;

            auto process_props = [&](const std::map<std::string, tinyusdz::Property>& props) {
                for (const auto& [prop_name, prop] : props) {
                    if (!prop.is_attribute()) continue;
                    const auto& attr = prop.get_attribute();

                    if (prop_name == "inputs:diffuseColor") {
                        tinyusdz::value::color3f col;
                        if (attr.get_value(&col)) {
                            asset->base_color_factor.x = col[0];
                            asset->base_color_factor.y = col[1];
                            asset->base_color_factor.z = col[2];
                        }
                    } else if (prop_name == "inputs:metallic") {
                        float f = 0.0f;
                        if (attr.get_value(&f)) asset->metallic_factor = f;
                    } else if (prop_name == "inputs:roughness") {
                        float f = 1.0f;
                        if (attr.get_value(&f)) asset->roughness_factor = f;
                    } else if (prop_name == "inputs:emissiveColor") {
                        tinyusdz::value::color3f col;
                        if (attr.get_value(&col)) {
                            asset->emissive_factor.x = col[0];
                            asset->emissive_factor.y = col[1];
                            asset->emissive_factor.z = col[2];
                        }
                    } else if (prop_name == "inputs:opacity") {
                        float f = 1.0f;
                        if (attr.get_value(&f)) asset->base_color_factor.w = f;
                    } else if (prop_name == "inputs:occlusion") {
                        float f = 1.0f;
                        if (attr.get_value(&f)) asset->occlusion_strength = f;
                    } else if (prop_name == "lyra:albedoMap") {
                        std::string s;
                        if (attr.get_value(&s) && !s.empty()) {
                            try {
                                AssetID id        = std::stoull(s);
                                asset->albedo_map = G_ASSET_SERVER ? G_ASSET_SERVER->load_asset<TextureAsset>(id) : TextureAssetHandle(id);
                            } catch (...) {
                            }
                        }
                    } else if (prop_name == "lyra:normalMap") {
                        std::string s;
                        if (attr.get_value(&s) && !s.empty()) {
                            try {
                                AssetID id        = std::stoull(s);
                                asset->normal_map = G_ASSET_SERVER ? G_ASSET_SERVER->load_asset<TextureAsset>(id) : TextureAssetHandle(id);
                            } catch (...) {
                            }
                        }
                    } else if (prop_name == "lyra:metallicRoughnessMap") {
                        std::string s;
                        if (attr.get_value(&s) && !s.empty()) {
                            try {
                                AssetID id                    = std::stoull(s);
                                asset->metallic_roughness_map = G_ASSET_SERVER ? G_ASSET_SERVER->load_asset<TextureAsset>(id) : TextureAssetHandle(id);
                            } catch (...) {
                            }
                        }
                    } else if (prop_name == "lyra:emissiveMap") {
                        std::string s;
                        if (attr.get_value(&s) && !s.empty()) {
                            try {
                                AssetID id          = std::stoull(s);
                                asset->emissive_map = G_ASSET_SERVER ? G_ASSET_SERVER->load_asset<TextureAsset>(id) : TextureAssetHandle(id);
                            } catch (...) {
                            }
                        }
                    } else if (prop_name == "lyra:occlusionMap") {
                        std::string s;
                        if (attr.get_value(&s) && !s.empty()) {
                            try {
                                AssetID id           = std::stoull(s);
                                asset->occlusion_map = G_ASSET_SERVER ? G_ASSET_SERVER->load_asset<TextureAsset>(id) : TextureAssetHandle(id);
                            } catch (...) {
                            }
                        }
                    } else if (prop_name == "lyra:normalScale") {
                        float f = 1.0f;
                        if (attr.get_value(&f)) asset->normal_scale = f;
                    } else if (prop_name == "lyra:alphaCutoff") {
                        float f = 0.5f;
                        if (attr.get_value(&f)) asset->alpha_cutoff = f;
                    }
                }
            };

            if (const auto* surface = shader->value.as<tinyusdz::UsdPreviewSurface>()) {
                tinyusdz::value::color3f col;
                if (surface->diffuseColor.get_value().get_scalar(&col)) {
                    asset->base_color_factor.x = col[0];
                    asset->base_color_factor.y = col[1];
                    asset->base_color_factor.z = col[2];
                }
                float f = 0.0f;
                if (surface->metallic.get_value().get_scalar(&f)) asset->metallic_factor = f;
                if (surface->roughness.get_value().get_scalar(&f)) asset->roughness_factor = f;
                if (surface->emissiveColor.get_value().get_scalar(&col)) {
                    asset->emissive_factor.x = col[0];
                    asset->emissive_factor.y = col[1];
                    asset->emissive_factor.z = col[2];
                }
                if (surface->opacity.get_value().get_scalar(&f)) asset->base_color_factor.w = f;
                if (surface->occlusion.get_value().get_scalar(&f)) asset->occlusion_strength = f;

                process_props(surface->props);
            }

            process_props(shader->props);
        }
    }

    return asset;
}

static void unload_material_asset(void* asset)
{
    delete reinterpret_cast<MaterialAsset*>(asset);
}

static uint get_material_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".material";
    }
    return 1;
}

static const char* blend_mode_to_string(MaterialBlendMode mode)
{
    switch (mode) {
        case MaterialBlendMode::OPAQUE:
            return "opaque";
        case MaterialBlendMode::MASK:
            return "mask";
        case MaterialBlendMode::BLEND:
            return "blend";
    }
    return "opaque";
}

static const char* cull_mode_to_string(GPUCullMode mode)
{
    switch (mode) {
        case GPUCullMode::NONE:
            return "none";
        case GPUCullMode::FRONT:
            return "front";
        case GPUCullMode::BACK:
            return "back";
    }
    return "back";
}

static bool save_material_asset(const void* raw_asset, OSPath path)
{
    const auto* asset = reinterpret_cast<const MaterialAsset*>(raw_asset);
    if (!asset) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "#usda 1.0\n"
         << "(\n"
         << "    defaultPrim = \"Material\"\n"
         << ")\n\n"
         << "def Material \"Material\"\n"
         << "{\n"
         << "    token outputs:surface.connect = </Material/SurfaceShader.outputs:surface>\n"
         << "    custom string lyra:blendMode = \"" << blend_mode_to_string(asset->blend_mode) << "\"\n"
         << "    custom string lyra:cullMode = \"" << cull_mode_to_string(asset->cull_mode) << "\"\n"
         << "    custom bool lyra:depthWrite = " << (asset->depth_write ? "true" : "false") << "\n"
         << "    custom bool lyra:depthTest = " << (asset->depth_test ? "true" : "false") << "\n\n"
         << "    def Shader \"SurfaceShader\"\n"
         << "    {\n"
         << "        uniform token info:id = \"UsdPreviewSurface\"\n"
         << "        color3f inputs:diffuseColor = ("
         << asset->base_color_factor.x << ", " << asset->base_color_factor.y << ", " << asset->base_color_factor.z << ")\n"
         << "        float inputs:metallic = " << asset->metallic_factor << "\n"
         << "        float inputs:roughness = " << asset->roughness_factor << "\n"
         << "        color3f inputs:emissiveColor = ("
         << asset->emissive_factor.x << ", " << asset->emissive_factor.y << ", " << asset->emissive_factor.z << ")\n"
         << "        float inputs:opacity = " << asset->base_color_factor.w << "\n"
         << "        float inputs:occlusion = " << asset->occlusion_strength << "\n"
         << "        token outputs:surface\n\n";

    if (asset->albedo_map.valid()) {
        file << "        custom string lyra:albedoMap = \"" << std::to_string(asset->albedo_map.guid) << "\"\n";
    }
    if (asset->normal_map.valid()) {
        file << "        custom string lyra:normalMap = \"" << std::to_string(asset->normal_map.guid) << "\"\n";
    }
    if (asset->metallic_roughness_map.valid()) {
        file << "        custom string lyra:metallicRoughnessMap = \"" << std::to_string(asset->metallic_roughness_map.guid) << "\"\n";
    }
    if (asset->emissive_map.valid()) {
        file << "        custom string lyra:emissiveMap = \"" << std::to_string(asset->emissive_map.guid) << "\"\n";
    }
    if (asset->occlusion_map.valid()) {
        file << "        custom string lyra:occlusionMap = \"" << std::to_string(asset->occlusion_map.guid) << "\"\n";
    }

    file << "        custom float lyra:normalScale = " << asset->normal_scale << "\n"
         << "        custom float lyra:occlusionStrength = " << asset->occlusion_strength << "\n"
         << "        custom float lyra:alphaCutoff = " << asset->alpha_cutoff << "\n"
         << "    }\n"
         << "}\n";

    file.close();
    return !file.fail();
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

namespace lyra::material::saver
{
    void prepare() {}
    void cleanup() {}

    auto create() -> AssetSaverAPI
    {
        auto api                     = AssetSaverAPI{};
        api.configure                = nullptr;
        api.save                     = save_material_asset;
        api.get_supported_extensions = get_material_extensions;
        return api;
    }
} // namespace lyra::material::saver
