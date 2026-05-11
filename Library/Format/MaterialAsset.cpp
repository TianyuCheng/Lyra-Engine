#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/MaterialAsset.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::material::loader
{
    extern AssetLoaderAPI create();
    extern void           prepare();
    extern void           cleanup();
} // namespace lyra::material::loader

using MaterialLoaderPlugin = BuiltinPlugin<AssetLoaderAPI>;

AssetLoaderAPI MaterialAsset::loader()
{
    static Own<MaterialLoaderPlugin> PLUGIN;
    if (!PLUGIN)
        PLUGIN = std::make_unique<MaterialLoaderPlugin>(
            lyra::material::loader::create,
            lyra::material::loader::prepare,
            lyra::material::loader::cleanup);
    return *PLUGIN->get_api();
}

#include <fstream>
#include <Lyra/Common/Collections.h>

static String cull_mode_to_string(GPUCullMode mode)
{
    switch (mode) {
        case GPUCullMode::NONE: return "none";
        case GPUCullMode::FRONT: return "front";
        case GPUCullMode::BACK: return "back";
    }
    return "back";
}

bool MaterialAsset::save(OSPath path) const
{
    JSON json;

    if (schema.valid()) {
        json["schema"] = std::to_string(schema.uuid);
    }

    // textures
    if (!params.textures.empty()) {
        JSON textures = JSON::object();
        for (auto& [name, handle] : params.textures) {
            if (handle.valid()) {
                textures[name] = std::to_string(handle.uuid);
            }
        }
        json["textures"] = textures;
    }

    // constants
    if (!params.constants.empty()) {
        JSON constants = JSON::object();
        for (auto& [name, value] : params.constants) {
            constants[name] = {value.x, value.y, value.z, value.w};
        }
        json["constants"] = constants;
    }

    // overrides
    if (cull_mode) {
        json["cull_mode"] = cull_mode_to_string(*cull_mode);
    }
    if (depth_write) {
        json["depth_write"] = *depth_write;
    }
    if (depth_test) {
        json["depth_test"] = *depth_test;
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << json.dump(4);
    return true;
}
