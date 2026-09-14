#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Format/ModelAsset.h>
#include "ModelUtils.h"

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <filesystem>
#include <fstream>

using namespace lyra;
using namespace lyra::model;

namespace fs = std::filesystem;

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

    // Cache subdirectories
    fs::path root(caches_root);
    fs::path models_dir    = root / "models";
    fs::path meshes_dir    = root / "meshes";
    fs::path textures_dir  = root / "textures";
    fs::path materials_dir = root / "materials";
    fs::create_directories(models_dir);
    fs::create_directories(meshes_dir);
    fs::create_directories(textures_dir);
    fs::create_directories(materials_dir);

    ModelAsset model;
    model.root = static_cast<uint>(gltf_model.defaultScene >= 0 ? gltf_model.scenes[gltf_model.defaultScene].nodes[0] : 0);

    JSON deps = JSON::array();
    TreeMap<int, AssetID> material_map;

    // Process Materials
    for (size_t i = 0; i < gltf_model.materials.size(); ++i) {
        const auto& g_mat = gltf_model.materials[i];
        MaterialAsset mat;

        mat.base_color_factor = Vector4(
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[0]),
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[1]),
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[2]),
            static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[3])
        );
        mat.metallic_factor  = static_cast<float>(g_mat.pbrMetallicRoughness.metallicFactor);
        mat.roughness_factor = static_cast<float>(g_mat.pbrMetallicRoughness.roughnessFactor);

        if (g_mat.emissiveFactor.size() == 3) {
            mat.emissive_factor = Vector3(
                static_cast<float>(g_mat.emissiveFactor[0]),
                static_cast<float>(g_mat.emissiveFactor[1]),
                static_cast<float>(g_mat.emissiveFactor[2])
            );
        }

        if (g_mat.alphaMode == "BLEND") {
            mat.blend_mode  = MaterialBlendMode::BLEND;
            mat.depth_write = false;
        } else if (g_mat.alphaMode == "MASK") {
            mat.blend_mode   = MaterialBlendMode::MASK;
            mat.alpha_cutoff = static_cast<float>(g_mat.alphaCutoff);
        }

        if (g_mat.doubleSided) {
            mat.cull_mode = GPUCullMode::NONE;
        }

        AssetID mat_id = random_guid();
        Path mat_path = materials_dir / (std::to_string(mat_id) + ".material");
        MaterialAsset::saver().save(&mat, mat_path.c_str());
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

                if (primitive.indices >= 0) {
                    const auto& accessor = gltf_model.accessors[primitive.indices];
                    const auto& view     = gltf_model.bufferViews[accessor.bufferView];
                    const auto& buffer   = gltf_model.buffers[view.buffer];

                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        lod.index_format = GPUIndexFormat::UINT16;
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        lod.index_format = GPUIndexFormat::UINT32;
                    } else {
                        get_logger()->error("unsupported index format");
                        continue;
                    }

                    size_t idx_size = (lod.index_format == GPUIndexFormat::UINT16) ? 2 : 4;
                    size_t stride   = accessor.ByteStride(view);

                    surface.slice.first_index = static_cast<uint>(lod.index_data.size() / idx_size);
                    surface.slice.index_count = static_cast<uint>(accessor.count);

                    lod.index_data.resize(lod.index_data.size() + accessor.count * idx_size);
                    uint8_t* dst = lod.index_data.data() + surface.slice.first_index * idx_size;
                    const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

                    for (size_t k = 0; k < accessor.count; ++k) {
                        memcpy(dst + k * idx_size, src + k * stride, idx_size);
                    }
                }

                for (auto& [attr_name, attr_idx] : primitive.attributes) {
                    const auto& accessor = gltf_model.accessors[attr_idx];
                    const auto& view     = gltf_model.bufferViews[accessor.bufferView];
                    const auto& buffer   = gltf_model.buffers[view.buffer];

                    MeshSemantics semantic;
                    GPUVertexFormat format;

                    if (attr_name == "POSITION") {
                        semantic = MeshSemantics::POSITION;
                        format   = GPUVertexFormat::FLOAT32x3;
                    } else if (attr_name == "NORMAL") {
                        semantic = MeshSemantics::NORMAL;
                        format   = GPUVertexFormat::FLOAT32x3;
                    } else if (attr_name == "TEXCOORD_0") {
                        semantic = MeshSemantics::TEXCOORD0;
                        format   = GPUVertexFormat::FLOAT32x2;
                    } else if (attr_name == "TANGENT") {
                        semantic = MeshSemantics::TANGENT;
                        format   = GPUVertexFormat::FLOAT32x4;
                    } else {
                        continue;
                    }

                    if (attr_name == "POSITION") {
                        if (accessor.minValues.size() == 3) {
                            surface.min_bounds = glm::min(surface.min_bounds, Vector3(accessor.minValues[0], accessor.minValues[1], accessor.minValues[2]));
                        }
                        if (accessor.maxValues.size() == 3) {
                            surface.max_bounds = glm::max(surface.max_bounds, Vector3(accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2]));
                        }
                        mesh.min_bounds = glm::min(mesh.min_bounds, surface.min_bounds);
                        mesh.max_bounds = glm::max(mesh.max_bounds, surface.max_bounds);
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
                        attr->format    = format;
                    }

                    size_t comp_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
                    size_t num_comp  = tinygltf::GetNumComponentsInType(accessor.type);
                    size_t elem_size = comp_size * num_comp;
                    size_t stride    = accessor.ByteStride(view);

                    surface.slice.first_vertex = static_cast<uint>(attr->element_count);
                    surface.slice.vertex_count = static_cast<uint>(accessor.count);

                    attr->element_count += static_cast<uint32_t>(accessor.count);
                    attr->data.resize(attr->data.size() + accessor.count * elem_size);

                    uint8_t* dst = attr->data.data() + surface.slice.first_vertex * elem_size;
                    const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

                    for (size_t k = 0; k < accessor.count; ++k) {
                        memcpy(dst + k * elem_size, src + k * stride, elem_size);
                    }
                }

                lod.surfaces.push_back(surface);
            }

            AssetID mesh_id = random_guid();
            Path mesh_path = meshes_dir / (std::to_string(mesh_id) + ".mesh");
            MeshAsset::saver().save(&mesh, mesh_path.c_str());
            node.mesh = AssetHandle<MeshAsset>(mesh_id);
            deps.push_back(mesh_id);
        }

        for (int child_idx : g_node.children) {
            node.children.push_back(static_cast<uint>(child_idx));
        }

        model.nodes.push_back(std::move(node));
    }

    // Save ModelAsset (USDA format)
    AssetID model_id = metadata["guid"].get<AssetID>();
    Path model_cache_path = models_dir / (std::to_string(model_id) + ".model");
    ModelAsset::saver().save(&model, model_cache_path.c_str());

    metadata["path"]         = "models/" + std::to_string(model_id) + ".model";
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
