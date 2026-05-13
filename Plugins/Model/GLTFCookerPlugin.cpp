#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Format/ModelAsset.h>
#include "ModelUtils.h"

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <fstream>

using namespace lyra;
using namespace lyra::model;

static void configure_gltf(AssetServer*, const JSON&) {}

static bool process_gltf(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    tinygltf::Model gltf_model;
    tinygltf::TinyGLTF loader;
    String err;
    String warn;

    bool ret = false;
    String path_str = Path(source_path).string();
    if (path_str.find(".glb") != String::npos) {
        ret = loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path_str.c_str());
    } else {
        ret = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, path_str.c_str());
    }

    if (!warn.empty()) get_logger()->warn("tinygltf: {}", warn);
    if (!err.empty())  get_logger()->error("tinygltf: {}", err);
    if (!ret) return false;

    ModelAsset model;
    model.root = static_cast<uint>(gltf_model.defaultScene >= 0 ? gltf_model.scenes[gltf_model.defaultScene].nodes[0] : 0);

    JSON deps = JSON::array();
    TreeMap<int, AssetID> material_map;

    // Process Materials
    for (size_t i = 0; i < gltf_model.materials.size(); ++i) {
        const auto& g_mat = gltf_model.materials[i];
        MaterialAsset mat;

        // Basic PBR parameters
        mat.params.constants["baseColorFactor"] = Vector4(
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[0]),
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[1]),
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[2]),
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[3])
        );
        mat.params.constants["metallicRoughnessFactor"] = Vector4(
            static_cast<float>(g_mat.pbrMetallicRoughness.metallicFactor),
            static_cast<float>(g_mat.pbrMetallicRoughness.roughnessFactor),
            0.0f, 0.0f
        );

        // Alpha mode
        if (g_mat.alphaMode == "BLEND") mat.depth_write = false;
        if (g_mat.doubleSided) mat.cull_mode = GPUCullMode::NONE;

        AssetID mat_id = random_guid();
        Path mat_path = Path(caches_root) / (std::to_string(mat_id) + ".mat");
        mat.save(mat_path.c_str());
        material_map[static_cast<int>(i)] = mat_id;
        deps.push_back(mat_id);
    }

    // Process Nodes and Meshes
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
            if (g_node.translation.size() == 3) {
                node.transform = glm::translate(node.transform, Vector3(static_cast<float>(g_node.translation[0]), static_cast<float>(g_node.translation[1]), static_cast<float>(g_node.translation[2])));
            }
            if (g_node.rotation.size() == 4) {
                Quaternion q(static_cast<float>(g_node.rotation[3]), static_cast<float>(g_node.rotation[0]), static_cast<float>(g_node.rotation[1]), static_cast<float>(g_node.rotation[2]));
                node.transform *= Matrix4x4(glm::mat4_cast(q));
            }
            if (g_node.scale.size() == 3) {
                node.transform = glm::scale(node.transform, Vector3(static_cast<float>(g_node.scale[0]), static_cast<float>(g_node.scale[1]), static_cast<float>(g_node.scale[2])));
            }
        }

        if (g_node.mesh >= 0) {
            const auto& g_mesh = gltf_model.meshes[g_node.mesh];
            MeshAsset mesh;
            MeshLOD& lod = mesh.lods.emplace_back();

            mesh.min_bounds = Vector3(std::numeric_limits<float>::max());
            mesh.max_bounds = Vector3(std::numeric_limits<float>::lowest());

            for (const auto& primitive : g_mesh.primitives) {
                MeshSurface surface;
                surface.min_bounds = Vector3(std::numeric_limits<float>::max());
                surface.max_bounds = Vector3(std::numeric_limits<float>::lowest());

                if (primitive.material >= 0) {
                    surface.material = AssetHandle<MaterialAsset>(material_map[primitive.material]);
                }

                // Indices
                if (primitive.indices >= 0) {
                    const auto& accessor = gltf_model.accessors[primitive.indices];
                    const auto& view = gltf_model.bufferViews[accessor.bufferView];
                    const auto& buffer = gltf_model.buffers[view.buffer];

                    lod.index_format = (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? GPUIndexFormat::UINT16 : GPUIndexFormat::UINT32;
                    size_t stride = accessor.ByteStride(view);
                    surface.slice.first_index = static_cast<uint>(lod.index_data.size() / (lod.index_format == GPUIndexFormat::UINT16 ? 2 : 4));
                    surface.slice.index_count = static_cast<uint>(accessor.count);

                    lod.index_data.resize(lod.index_data.size() + accessor.count * (lod.index_format == GPUIndexFormat::UINT16 ? 2 : 4));
                    uint8_t* dst = lod.index_data.data() + surface.slice.first_index * (lod.index_format == GPUIndexFormat::UINT16 ? 2 : 4);
                    const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

                    for (size_t k = 0; k < accessor.count; ++k) {
                        memcpy(dst + k * (lod.index_format == GPUIndexFormat::UINT16 ? 2 : 4), src + k * stride, (lod.index_format == GPUIndexFormat::UINT16 ? 2 : 4));
                    }
                }

                // Attributes
                for (auto& [name, attr_idx] : primitive.attributes) {
                    const auto& accessor = gltf_model.accessors[attr_idx];
                    const auto& view = gltf_model.bufferViews[accessor.bufferView];
                    const auto& buffer = gltf_model.buffers[view.buffer];

                    MeshSemantics semantic = MeshSemantics::POSITION;
                    GPUVertexFormat format = GPUVertexFormat::FLOAT32x3;

                    if (name == "POSITION") {
                        semantic = MeshSemantics::POSITION;
                        format = GPUVertexFormat::FLOAT32x3;
                        if (accessor.minValues.size() == 3) {
                            surface.min_bounds = Vector3(accessor.minValues[0], accessor.minValues[1], accessor.minValues[2]);
                            mesh.min_bounds = glm::min(mesh.min_bounds, surface.min_bounds);
                        }
                        if (accessor.maxValues.size() == 3) {
                            surface.max_bounds = Vector3(accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2]);
                            mesh.max_bounds = glm::max(mesh.max_bounds, surface.max_bounds);
                        }
                    } else if (name == "NORMAL") {
                        semantic = MeshSemantics::NORMAL;
                        format = GPUVertexFormat::FLOAT32x3;
                    } else if (name == "TEXCOORD_0") {
                        semantic = MeshSemantics::TEXCOORD0;
                        format = GPUVertexFormat::FLOAT32x2;
                    } else {
                        continue; // Skip unsupported attributes
                    }

                    MeshAttribute* attr = nullptr;
                    for (auto& a : lod.attributes) {
                        if (a.semantics == semantic) {
                            attr = &a;
                            break;
                        }
                    }

                    if (!attr) {
                        attr = &lod.attributes.emplace_back();
                        attr->semantics = semantic;
                        attr->format = format;
                    }

                    size_t component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
                    size_t num_components = tinygltf::GetNumComponentsInType(accessor.type);
                    size_t element_size = component_size * num_components;
                    size_t stride = accessor.ByteStride(view);

                    surface.slice.first_vertex = static_cast<uint>(attr->element_count);
                    surface.slice.vertex_count = static_cast<uint>(accessor.count);

                    attr->element_count += accessor.count;
                    attr->data.resize(attr->data.size() + accessor.count * element_size);

                    uint8_t* dst = attr->data.data() + surface.slice.first_vertex * element_size;
                    const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

                    for (size_t k = 0; k < accessor.count; ++k) {
                        memcpy(dst + k * element_size, src + k * stride, element_size);
                    }
                }

                lod.surfaces.push_back(surface);
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
        if (node.mesh.valid())     n["mesh"]     = std::to_string(node.mesh.uuid);
        if (node.material.valid()) n["material"] = std::to_string(node.material.uuid);

        JSON children = JSON::array();
        for (uint c : node.children) children.push_back(c);
        n["children"] = children;

        JSON transform = JSON::array();
        for (int r = 0; r < 4; ++r) {
            JSON row = JSON::array();
            for (int c = 0; c < 4; ++c) row.push_back(node.transform[r][c]);
            transform.push_back(row);
        }
        n["transform"] = transform;

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
