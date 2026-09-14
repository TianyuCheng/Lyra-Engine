#include <fstream>
#include <functional>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Format/SceneAsset.h>

#include <tinyusdz.hh>
#include <usda-reader.hh>
#include <stream-reader.hh>

using namespace lyra;

static Logger get_logger()
{
    static Logger logger = create_logger("Scene", LogLevel::trace);
    return logger;
}

static uint extract_scene_nodes(
    SceneAsset&           scene,
    const tinyusdz::Prim& prim)
{
    const tinyusdz::Xform* xform = prim.as<tinyusdz::Xform>();
    if (!xform) {
        for (const auto& child : prim.children()) {
            extract_scene_nodes(scene, child);
        }
        return 0;
    }

    SceneAsset::Node node;
    node.name = prim.element_name();

    for (const auto& [prop_name, prop] : xform->props) {
        if (!prop.is_attribute()) continue;
        const auto& attr = prop.get_attribute();

        if (prop_name == "lyra:name") {
            std::string s;
            if (attr.get_value(&s)) node.name = s;
        } else if (prop_name == "lyra:model") {
            std::string s;
            if (attr.get_value(&s) && !s.empty()) {
                try {
                    node.model = ModelAssetHandle(std::stoull(s));
                } catch (...) {
                }
            }
        } else if (prop_name == "lyra:mesh") {
            std::string s;
            if (attr.get_value(&s) && !s.empty()) {
                try {
                    node.mesh = MeshAssetHandle(std::stoull(s));
                } catch (...) {
                }
            }
        } else if (prop_name == "lyra:material") {
            std::string s;
            if (attr.get_value(&s) && !s.empty()) {
                try {
                    node.material = MaterialAssetHandle(std::stoull(s));
                } catch (...) {
                }
            }
        } else if (prop_name == "xformOp:transform") {
            tinyusdz::value::matrix4d m;
            if (attr.get_value(&m)) {
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        node.transform[r][c] = static_cast<float>(m.m[r][c]);
                    }
                }
            }
        }
    }

    uint node_idx = static_cast<uint>(scene.nodes.size());
    scene.nodes.push_back(node);

    for (const auto& child : prim.children()) {
        const tinyusdz::Xform* child_xform = child.as<tinyusdz::Xform>();
        if (child_xform) {
            uint child_idx = extract_scene_nodes(scene, child);
            scene.nodes[node_idx].children.push_back(child_idx);
        }
    }

    return node_idx;
}

static void* load_scene_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    if (content.empty()) {
        get_logger()->error("failed to read scene file: {}", path);
        return nullptr;
    }

    tinyusdz::StreamReader sr(
        reinterpret_cast<const uint8_t*>(content.data()),
        content.size(),
        false);

    tinyusdz::usda::USDAReader reader(&sr);
    if (!reader.read() || !reader.reconstruct_stage()) {
        get_logger()->error("failed to parse .scene USDA at {}: {}", path, reader.get_error());
        return nullptr;
    }

    const tinyusdz::Stage& stage = reader.get_stage();
    auto                   asset = new SceneAsset();

    for (const auto& root_prim : stage.root_prims()) {
        extract_scene_nodes(*asset, root_prim);
    }

    return asset;
}

static void unload_scene_asset(void* asset)
{
    delete reinterpret_cast<SceneAsset*>(asset);
}

static uint get_scene_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".scene";
    }
    return 1;
}

static bool save_scene_asset(const void* raw_asset, OSPath path)
{
    const auto* asset = reinterpret_cast<const SceneAsset*>(raw_asset);
    if (!asset) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    std::string default_prim = asset->nodes.empty() ? "node_0" : ("node_" + std::to_string(asset->root));
    file << "#usda 1.0\n"
         << "(\n"
         << "    defaultPrim = \"" << default_prim << "\"\n"
         << "    customLayerData = {\n"
         << "        string lyra_root = \"" << asset->root << "\"\n"
         << "    }\n"
         << ")\n\n";

    std::function<void(uint, int)> write_node = [&](uint idx, int indent) {
        if (idx >= asset->nodes.size()) return;
        const auto& node = asset->nodes[idx];
        std::string pad(indent * 4, ' ');
        std::string child_pad((indent + 1) * 4, ' ');

        std::string prim_name = "node_" + std::to_string(idx);

        file << pad << "def Xform \"" << prim_name << "\"\n"
             << pad << "{\n"
             << child_pad << "custom string lyra:name = \"" << node.name << "\"\n";
        if (node.model.valid())
            file << child_pad << "custom string lyra:model = \"" << std::to_string(node.model.uuid) << "\"\n";
        if (node.mesh.valid())
            file << child_pad << "custom string lyra:mesh = \"" << std::to_string(node.mesh.uuid) << "\"\n";
        if (node.material.valid())
            file << child_pad << "custom string lyra:material = \"" << std::to_string(node.material.uuid) << "\"\n";
        file << child_pad << "matrix4d xformOp:transform = ( ";
        for (int r = 0; r < 4; ++r) {
            file << "(";
            for (int c = 0; c < 4; ++c) {
                file << node.transform[r][c];
                if (c < 3) file << ", ";
            }
            file << ")";
            if (r < 3) file << ", ";
        }
        file << " )\n"
             << child_pad << "uniform token[] xformOpOrder = [\"xformOp:transform\"]\n";

        for (uint child_idx : node.children) {
            file << "\n";
            write_node(child_idx, indent + 1);
        }
        file << pad << "}\n";
    };

    if (!asset->nodes.empty()) {
        write_node(asset->root, 0);
    }

    file.close();
    return !file.fail();
}

namespace lyra::scene::loader
{
    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_SCENE_VERBOSITY"));
    }

    void cleanup() {}

    auto create() -> AssetLoaderAPI
    {
        auto api                     = AssetLoaderAPI{};
        api.load                     = load_scene_asset;
        api.unload                   = unload_scene_asset;
        api.get_supported_extensions = get_scene_extensions;
        return api;
    }
} // namespace lyra::scene::loader

namespace lyra::scene::saver
{
    void prepare() {}
    void cleanup() {}

    auto create() -> AssetSaverAPI
    {
        auto api                     = AssetSaverAPI{};
        api.configure                = nullptr;
        api.save                     = save_scene_asset;
        api.get_supported_extensions = get_scene_extensions;
        return api;
    }
} // namespace lyra::scene::saver
