#include <filesystem>

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSPreview.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include "ModelUtils.h"

using namespace lyra;
using namespace lyra::model;

namespace fs = std::filesystem;

static void configure_gltf(AssetServer*, const JSON&) {}

static bool process_gltf(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    try {
        tinygltf::Model    gltf_model;
        tinygltf::TinyGLTF loader;
        String             err;
        String             warn;

        bool   ret      = false;
        String path_str = Path(source_path).string();
        if (path_str.find(".glb") != String::npos) {
            ret = loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path_str.c_str());
        } else {
            ret = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, path_str.c_str());
        }

        if (!warn.empty()) get_logger()->warn("tinygltf: {}", warn);
        if (!err.empty()) get_logger()->error("tinygltf: {}", err);
        if (!ret) return false;

        // cache subdirectories
        fs::path root(caches_root);
        fs::path models_dir    = root / "models";
        fs::path meshes_dir    = root / "meshes";
        fs::path textures_dir  = root / "textures";
        fs::path materials_dir = root / "materials";
        fs::create_directories(models_dir);
        fs::create_directories(meshes_dir);
        fs::create_directories(textures_dir);
        fs::create_directories(materials_dir);

        if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
            return false;
        }

        AssetID              model_id = metadata["guid"].get<AssetID>();
        AssetDependencyScope deps(metadata, source_path, caches_root);

        ModelAsset model;
        if (gltf_model.defaultScene >= 0 && static_cast<size_t>(gltf_model.defaultScene) < gltf_model.scenes.size() && !gltf_model.scenes[gltf_model.defaultScene].nodes.empty()) {
            model.root = static_cast<uint>(gltf_model.scenes[gltf_model.defaultScene].nodes[0]);
        } else if (!gltf_model.scenes.empty() && !gltf_model.scenes[0].nodes.empty()) {
            model.root = static_cast<uint>(gltf_model.scenes[0].nodes[0]);
        } else {
            model.root = 0;
        }

        TreeMap<int, AssetID> material_map;

        // process materials
        for (size_t i = 0; i < gltf_model.materials.size(); ++i) {
            const auto&   g_mat = gltf_model.materials[i];
            MaterialAsset mat;

            if (g_mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                mat.base_color_factor = Vector4(
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[0]),
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[1]),
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[2]),
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[3]));
            }
            mat.metallic_factor  = static_cast<float>(g_mat.pbrMetallicRoughness.metallicFactor);
            mat.roughness_factor = static_cast<float>(g_mat.pbrMetallicRoughness.roughnessFactor);

            if (g_mat.emissiveFactor.size() == 3) {
                mat.emissive_factor = Vector3(
                    static_cast<float>(g_mat.emissiveFactor[0]),
                    static_cast<float>(g_mat.emissiveFactor[1]),
                    static_cast<float>(g_mat.emissiveFactor[2]));
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

            String  mat_name = "material/" + (g_mat.name.empty() ? std::to_string(i) : g_mat.name);
            AssetID mat_id   = deps.resolve(model_id, mat_name, MaterialAsset::type);
            deps.set_path(mat_id, "materials/" + std::to_string(mat_id) + ".material");
            Path mat_path = materials_dir / (std::to_string(mat_id) + ".material");
            MaterialAsset::saver().save(&mat, mat_path.c_str());
            material_map[static_cast<int>(i)] = mat_id;
        }

        PreviewScene preview_scene;
        for (const auto& img : gltf_model.images) {
            PreviewTexture ptex;
            ptex.width    = static_cast<uint>(img.width);
            ptex.height   = static_cast<uint>(img.height);
            ptex.channels = static_cast<uint>(img.component);
            ptex.pixels   = img.image;
            preview_scene.textures.push_back(std::move(ptex));
        }

        for (size_t i = 0; i < gltf_model.materials.size(); ++i) {
            const auto&     g_mat = gltf_model.materials[i];
            PreviewMaterial pmat;
            if (g_mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                pmat.base_color_factor = Vector4(
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[0]),
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[1]),
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[2]),
                    static_cast<float>(g_mat.pbrMetallicRoughness.baseColorFactor[3]));
            }
            pmat.roughness = static_cast<float>(g_mat.pbrMetallicRoughness.roughnessFactor);
            pmat.metallic  = static_cast<float>(g_mat.pbrMetallicRoughness.metallicFactor);
            int tex_idx    = g_mat.pbrMetallicRoughness.baseColorTexture.index;
            if (tex_idx >= 0 && static_cast<size_t>(tex_idx) < gltf_model.textures.size()) {
                int img_idx = gltf_model.textures[tex_idx].source;
                if (img_idx >= 0 && static_cast<size_t>(img_idx) < gltf_model.images.size()) {
                    pmat.albedo_texture_id = img_idx;
                }
            }
            preview_scene.materials.push_back(pmat);
        }

        // precalculate local and world transforms across the glTF node hierarchy
        Vector<Matrix4x4> local_transforms(gltf_model.nodes.size(), Matrix4x4(1.0f));
        Vector<Matrix4x4> world_transforms(gltf_model.nodes.size(), Matrix4x4(1.0f));

        for (size_t i = 0; i < gltf_model.nodes.size(); ++i) {
            const auto& g_node = gltf_model.nodes[i];
            if (g_node.matrix.size() == 16) {
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) {
                        local_transforms[i][c][r] = static_cast<float>(g_node.matrix[c * 4 + r]);
                    }
                }
            } else {
                Matrix4x4 T(1.0f);
                if (g_node.translation.size() == 3) {
                    T = glm::translate(Matrix4x4(1.0f), Vector3(static_cast<float>(g_node.translation[0]), static_cast<float>(g_node.translation[1]), static_cast<float>(g_node.translation[2])));
                }
                Matrix4x4 R(1.0f);
                if (g_node.rotation.size() == 4) {
                    Quaternion q(static_cast<float>(g_node.rotation[3]), static_cast<float>(g_node.rotation[0]), static_cast<float>(g_node.rotation[1]), static_cast<float>(g_node.rotation[2]));
                    R = Matrix4x4(glm::mat4_cast(q));
                }
                Matrix4x4 S(1.0f);
                if (g_node.scale.size() == 3) {
                    S = glm::scale(Matrix4x4(1.0f), Vector3(static_cast<float>(g_node.scale[0]), static_cast<float>(g_node.scale[1]), static_cast<float>(g_node.scale[2])));
                }
                local_transforms[i] = T * R * S;
            }
        }

        auto compute_world = [&](auto& self, int node_idx, const Matrix4x4& parent_world) -> void {
            if (node_idx < 0 || static_cast<size_t>(node_idx) >= gltf_model.nodes.size()) return;
            Matrix4x4 world = parent_world * local_transforms[node_idx];
            world_transforms[node_idx] = world;
            for (int child_idx : gltf_model.nodes[node_idx].children) {
                self(self, child_idx, world);
            }
        };

        int active_scene = gltf_model.defaultScene >= 0 ? gltf_model.defaultScene : 0;
        if (active_scene >= 0 && static_cast<size_t>(active_scene) < gltf_model.scenes.size()) {
            for (int root_idx : gltf_model.scenes[active_scene].nodes) {
                compute_world(compute_world, root_idx, Matrix4x4(1.0f));
            }
        } else {
            for (size_t i = 0; i < gltf_model.nodes.size(); ++i) {
                compute_world(compute_world, static_cast<int>(i), Matrix4x4(1.0f));
            }
        }

        // process nodes and meshes
        HashSet<int> saved_meshes;
        for (size_t i = 0; i < gltf_model.nodes.size(); ++i) {
            const auto&      g_node = gltf_model.nodes[i];
            ModelAsset::Node node;
            node.name      = g_node.name;
            node.transform = local_transforms[i];

            if (g_node.mesh >= 0 && static_cast<size_t>(g_node.mesh) < gltf_model.meshes.size()) {
                const auto& g_mesh = gltf_model.meshes[g_node.mesh];
                MeshAsset   mesh;
                MeshLOD&    lod = mesh.lods.emplace_back();

                mesh.min_bounds = Vector3(std::numeric_limits<float>::max());
                mesh.max_bounds = Vector3(std::numeric_limits<float>::lowest());

                for (const auto& primitive : g_mesh.primitives) {
                    MeshSurface surface;
                    surface.min_bounds = Vector3(std::numeric_limits<float>::max());
                    surface.max_bounds = Vector3(std::numeric_limits<float>::lowest());

                    if (primitive.material >= 0 && material_map.find(primitive.material) != material_map.end()) {
                        surface.material = AssetHandle<MaterialAsset>(material_map[primitive.material]);
                    }

                    if (primitive.indices >= 0 && static_cast<size_t>(primitive.indices) < gltf_model.accessors.size()) {
                        const auto& accessor = gltf_model.accessors[primitive.indices];
                        if (accessor.bufferView >= 0 && static_cast<size_t>(accessor.bufferView) < gltf_model.bufferViews.size()) {
                            const auto& view = gltf_model.bufferViews[accessor.bufferView];
                            if (view.buffer >= 0 && static_cast<size_t>(view.buffer) < gltf_model.buffers.size()) {
                                const auto& buffer = gltf_model.buffers[view.buffer];

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

                                size_t req_bytes = view.byteOffset + accessor.byteOffset + (accessor.count > 0 ? (accessor.count - 1) * stride + idx_size : 0);
                                if (buffer.data.size() >= req_bytes && accessor.count > 0) {
                                    lod.index_data.resize(lod.index_data.size() + accessor.count * idx_size);
                                    uint8_t*       dst = lod.index_data.data() + surface.slice.first_index * idx_size;
                                    const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

                                    for (size_t k = 0; k < accessor.count; ++k) {
                                        memcpy(dst + k * idx_size, src + k * stride, idx_size);
                                    }
                                }
                            }
                        }
                    }

                    for (auto& [attr_name, attr_idx] : primitive.attributes) {
                        if (attr_idx < 0 || static_cast<size_t>(attr_idx) >= gltf_model.accessors.size()) continue;
                        const auto& accessor = gltf_model.accessors[attr_idx];
                        if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= gltf_model.bufferViews.size()) continue;
                        const auto& view = gltf_model.bufferViews[accessor.bufferView];
                        if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= gltf_model.buffers.size()) continue;
                        const auto& buffer = gltf_model.buffers[view.buffer];

                        MeshSemantics   semantic;
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
                            attr            = &lod.attributes.emplace_back();
                            attr->semantics = semantic;
                            attr->format    = format;
                        }

                        size_t comp_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
                        size_t num_comp  = tinygltf::GetNumComponentsInType(accessor.type);
                        size_t elem_size = comp_size * num_comp;
                        size_t stride    = accessor.ByteStride(view);

                        surface.slice.first_vertex = static_cast<uint>(attr->element_count);
                        surface.slice.vertex_count = static_cast<uint>(accessor.count);

                        size_t req_bytes = view.byteOffset + accessor.byteOffset + (accessor.count > 0 ? (accessor.count - 1) * stride + elem_size : 0);
                        if (buffer.data.size() >= req_bytes && accessor.count > 0 && elem_size > 0) {
                            attr->element_count += static_cast<uint>(accessor.count);
                            attr->data.resize(attr->data.size() + accessor.count * elem_size);

                            uint8_t*       dst = attr->data.data() + surface.slice.first_vertex * elem_size;
                            const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

                            for (size_t k = 0; k < accessor.count; ++k) {
                                memcpy(dst + k * elem_size, src + k * stride, elem_size);
                            }
                        }
                    }

                    lod.surfaces.push_back(surface);
                }

                String  mesh_name = "mesh/" + (g_mesh.name.empty() ? std::to_string(g_node.mesh) : g_mesh.name);
                AssetID mesh_id   = deps.resolve(model_id, mesh_name, MeshAsset::type);
                deps.set_path(mesh_id, "meshes/" + std::to_string(mesh_id) + ".mesh");
                if (saved_meshes.insert(g_node.mesh).second) {
                    Path mesh_path = meshes_dir / (std::to_string(mesh_id) + ".mesh");
                    MeshAsset::saver().save(&mesh, mesh_path.c_str());
                }
                node.mesh = AssetHandle<MeshAsset>(mesh_id);

                PreviewMesh rmesh;
                rmesh.transform = world_transforms[i];
                if (!g_mesh.primitives.empty() && g_mesh.primitives[0].material >= 0) {
                    rmesh.material_id = g_mesh.primitives[0].material;
                }
                for (const auto& attr : lod.attributes) {
                    if (attr.semantics == MeshSemantics::POSITION) {
                        size_t count = attr.element_count;
                        if (count * sizeof(Vector3) <= attr.data.size()) {
                            rmesh.positions.resize(count);
                            memcpy(rmesh.positions.data(), attr.data.data(), count * sizeof(Vector3));
                        }
                    } else if (attr.semantics == MeshSemantics::NORMAL) {
                        size_t count = attr.element_count;
                        if (count * sizeof(Vector3) <= attr.data.size()) {
                            rmesh.normals.resize(count);
                            memcpy(rmesh.normals.data(), attr.data.data(), count * sizeof(Vector3));
                        }
                    } else if (attr.semantics == MeshSemantics::TEXCOORD0) {
                        size_t count = attr.element_count;
                        if (count * sizeof(Vector2) <= attr.data.size()) {
                            rmesh.uvs.resize(count);
                            memcpy(rmesh.uvs.data(), attr.data.data(), count * sizeof(Vector2));
                        }
                    }
                }
                if (lod.index_format == GPUIndexFormat::UINT16) {
                    size_t        count   = lod.index_data.size() / 2;
                    const ushort* src_idx = reinterpret_cast<const ushort*>(lod.index_data.data());
                    rmesh.indices.resize(count);
                    for (size_t k = 0; k < count; ++k)
                        rmesh.indices[k] = src_idx[k];
                } else if (lod.index_format == GPUIndexFormat::UINT32) {
                    size_t      count   = lod.index_data.size() / 4;
                    const uint* src_idx = reinterpret_cast<const uint*>(lod.index_data.data());
                    rmesh.indices.assign(src_idx, src_idx + count);
                }
                preview_scene.meshes.push_back(std::move(rmesh));
            }

            for (int child_idx : g_node.children) {
                if (child_idx >= 0 && static_cast<size_t>(child_idx) < gltf_model.nodes.size()) {
                    node.children.push_back(static_cast<uint>(child_idx));
                }
            }

            model.nodes.push_back(std::move(node));
        }

        // save model asset (USDA format)
        Path model_cache_path = models_dir / (std::to_string(model_id) + ".model");
        ModelAsset::saver().save(&model, model_cache_path.c_str());

        metadata["path"] = "models/" + std::to_string(model_id) + ".model";
        deps.commit();

        preview_api().generate_thumbnail(preview_scene, metadata);

        return true;
    } catch (const std::exception& e) {
        get_logger()->error("Exception while cooking GLTF file {}: {}", Path(source_path).string(), e.what());
        return false;
    } catch (...) {
        get_logger()->error("Unknown error while cooking GLTF file {}", Path(source_path).string());
        return false;
    }
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
        auto api                     = AssetCookerAPI{};
        api.configure                = configure_gltf;
        api.process                  = process_gltf;
        api.get_supported_extensions = get_gltf_extensions;
        return api;
    }
} // namespace lyra::gltf::cooker
