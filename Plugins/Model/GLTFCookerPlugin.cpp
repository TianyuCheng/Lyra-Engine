#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Format/ModelAsset.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <fstream>

using namespace lyra;

static void configure_gltf(AssetServer*, const JSON&) {}

static bool process_gltf(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    tinygltf::Model gltf_model;
    tinygltf::TinyGLTF loader;
    String err;
    String warn;

    bool ret = false;
    String path_str = String(reinterpret_cast<const char*>(source_path));
    if (path_str.find(".glb") != String::npos) {
        ret = loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path_str.c_str());
    } else {
        ret = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, path_str.c_str());
    }

    if (!warn.empty()) spdlog::warn("TinyGLTF: {}", warn);
    if (!err.empty()) spdlog::error("TinyGLTF: {}", err);
    if (!ret) return false;

    ModelAsset model;
    model.root = static_cast<uint>(gltf_model.defaultScene >= 0 ? gltf_model.scenes[gltf_model.defaultScene].nodes[0] : 0);
    
    // This is a complex task. For a basic implementation, we'll traverse nodes and meshes.
    // Mapping GLTF concepts to Lyra concepts.

    JSON deps = JSON::array();

    for (size_t i = 0; i < gltf_model.nodes.size(); ++i) {
        const auto& g_node = gltf_model.nodes[i];
        ModelAsset::Node node;
        node.name = g_node.name;
        
        if (g_node.matrix.size() == 16) {
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    node.transform[r][c] = static_cast<float>(g_node.matrix[r * 4 + c]);
        } else {
            node.transform = Matrix4x4(1.0f);
            // Handle TRS... (omitted for brevity in this first pass)
        }

        if (g_node.mesh >= 0) {
            // Create a Lyra MeshAsset for the GLTF mesh
            const auto& g_mesh = gltf_model.meshes[g_node.mesh];
            MeshAsset mesh;
            MeshLOD& lod = mesh.lods.emplace_back();

            for (const auto& primitive : g_mesh.primitives) {
                MeshSurface surface;
                // Extract attributes and indices from GLTF accessors/buffers...
                // (This requires significant boilerplate code)
            }

            AssetID mesh_id = random_guid();
            Path mesh_path = Path(caches_root) / (std::to_string(mesh_id) + ".mesh");
            mesh.save(mesh_path.c_str());
            node.mesh = AssetHandle<MeshAsset>(mesh_id);
            deps.push_back(mesh_id);
        }

        for (int child_idx : g_node.children) {
            node.children.push_back(static_cast<uint>(child_idx));
        }

        model.nodes.push_back(std::move(node));
    }

    // Save ModelAsset
    JSON model_json;
    model_json["root"] = model.root;
    JSON nodes_arr = JSON::array();
    for (const auto& node : model.nodes) {
        JSON n;
        n["name"] = node.name;
        if (node.mesh.valid()) n["mesh"] = std::to_string(node.mesh.uuid);
        JSON children = JSON::array();
        for (uint c : node.children) children.push_back(c);
        n["children"] = children;
        nodes_arr.push_back(n);
    }
    model_json["nodes"] = nodes_arr;

    Path model_cache_path = Path(caches_root) / (std::to_string(metadata["guid"].get<AssetID>()) + ".model");
    std::ofstream out(model_cache_path);
    out << model_json.dump(4);

    metadata["path"] = std::to_string(metadata["guid"].get<AssetID>()) + ".model";
    metadata["dependencies"] = deps;

    return true;
}

static uint get_gltf_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".gltf";
        extensions[1] = ".glb";
    }
    return 2;
}

namespace lyra::gltf::cooker
{
    void prepare() {}
    void cleanup() {}
    auto create() -> AssetCookerAPI
    {
        auto api = AssetCookerAPI{};
        api.configure = configure_gltf;
        api.process = process_gltf;
        api.get_supported_extensions = get_gltf_extensions;
        return api;
    }
}
