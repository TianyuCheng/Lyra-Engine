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

static void configure_gltf(AssetServer*, const JSON&) {}

static MaterialAsset extract_gltf_material(const tinygltf::Material& g_mat)
{
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
    return mat;
}

static TreeMap<int, AssetID> cook_materials(
    const tinygltf::Model& gltf_model,
    AssetID                model_id,
    AssetDependencyScope&  deps,
    const Path&            materials_dir)
{
    TreeMap<int, AssetID> material_map;
    for (usize i = 0; i < gltf_model.materials.size(); ++i) {
        const auto&   g_mat    = gltf_model.materials[i];
        MaterialAsset mat      = extract_gltf_material(g_mat);
        String        mat_name = "material/" + (g_mat.name.empty() ? std::to_string(i) : g_mat.name);
        AssetID       mat_id   = deps.resolve(model_id, mat_name, MaterialAsset::type);
        deps.set_path(mat_id, "materials/" + std::to_string(mat_id) + ".material");
        Path mat_path = materials_dir / (std::to_string(mat_id) + ".material");
        MaterialAsset::saver().save(&mat, mat_path.c_str());
        material_map[static_cast<int>(i)] = mat_id;
    }
    return material_map;
}

static PreviewScene build_preview_scene(const tinygltf::Model& gltf_model)
{
    PreviewScene preview_scene;
    for (const auto& img : gltf_model.images) {
        PreviewTexture ptex;
        ptex.width    = static_cast<uint>(img.width);
        ptex.height   = static_cast<uint>(img.height);
        ptex.channels = static_cast<uint>(img.component);
        ptex.pixels   = img.image;
        preview_scene.textures.push_back(std::move(ptex));
    }

    for (const auto& g_mat : gltf_model.materials) {
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
        if (tex_idx >= 0 && static_cast<usize>(tex_idx) < gltf_model.textures.size()) {
            int img_idx = gltf_model.textures[tex_idx].source;
            if (img_idx >= 0 && static_cast<usize>(img_idx) < gltf_model.images.size()) {
                pmat.albedo_texture_id = img_idx;
            }
        }
        preview_scene.materials.push_back(pmat);
    }
    return preview_scene;
}

static Matrix4x4 extract_node_transform(const tinygltf::Node& g_node)
{
    if (g_node.matrix.size() == 16) {
        Matrix4x4 local(1.0f);
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                local[c][r] = static_cast<float>(g_node.matrix[c * 4 + r]);
            }
        }
        return local;
    }

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
    return T * R * S;
}

static void compute_transforms(
    const tinygltf::Model& gltf_model,
    Vector<Matrix4x4>&     out_local,
    Vector<Matrix4x4>&     out_world)
{
    out_local.resize(gltf_model.nodes.size(), Matrix4x4(1.0f));
    out_world.resize(gltf_model.nodes.size(), Matrix4x4(1.0f));

    for (usize i = 0; i < gltf_model.nodes.size(); ++i) {
        out_local[i] = extract_node_transform(gltf_model.nodes[i]);
    }

    auto traverse = [&](auto& self, int node_idx, const Matrix4x4& parent_world) -> void {
        if (node_idx < 0 || static_cast<usize>(node_idx) >= gltf_model.nodes.size()) return;
        Matrix4x4 world     = parent_world * out_local[node_idx];
        out_world[node_idx] = world;
        for (int child_idx : gltf_model.nodes[node_idx].children) {
            self(self, child_idx, world);
        }
    };

    int active_scene = gltf_model.defaultScene >= 0 ? gltf_model.defaultScene : 0;
    if (active_scene >= 0 && static_cast<usize>(active_scene) < gltf_model.scenes.size()) {
        for (int root_idx : gltf_model.scenes[active_scene].nodes) {
            traverse(traverse, root_idx, Matrix4x4(1.0f));
        }
    } else {
        for (usize i = 0; i < gltf_model.nodes.size(); ++i) {
            traverse(traverse, static_cast<int>(i), Matrix4x4(1.0f));
        }
    }
}

static void extract_primitive_indices(
    const tinygltf::Model& gltf_model,
    int                    accessor_idx,
    MeshLOD&               lod,
    MeshSurface&           surface)
{
    if (accessor_idx < 0 || static_cast<usize>(accessor_idx) >= gltf_model.accessors.size()) return;
    const auto& accessor = gltf_model.accessors[accessor_idx];

    if (accessor.bufferView < 0 || static_cast<usize>(accessor.bufferView) >= gltf_model.bufferViews.size()) return;
    const auto& view = gltf_model.bufferViews[accessor.bufferView];

    if (view.buffer < 0 || static_cast<usize>(view.buffer) >= gltf_model.buffers.size()) return;
    const auto& buffer = gltf_model.buffers[view.buffer];

    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        lod.index_format = GPUIndexFormat::UINT16;
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        lod.index_format = GPUIndexFormat::UINT32;
    } else {
        get_logger()->error("unsupported index format");
        return;
    }

    usize idx_size = (lod.index_format == GPUIndexFormat::UINT16) ? 2 : 4;
    usize stride   = accessor.ByteStride(view);

    surface.slice.first_index = static_cast<uint>(lod.index_data.size() / idx_size);
    surface.slice.index_count = static_cast<uint>(accessor.count);

    usize req_bytes = view.byteOffset + accessor.byteOffset + (accessor.count > 0 ? (accessor.count - 1) * stride + idx_size : 0);
    if (buffer.data.size() < req_bytes || accessor.count == 0) return;

    lod.index_data.resize(lod.index_data.size() + accessor.count * idx_size);
    uint8_t*       dst = lod.index_data.data() + surface.slice.first_index * idx_size;
    const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

    for (usize k = 0; k < accessor.count; ++k) {
        memcpy(dst + k * idx_size, src + k * stride, idx_size);
    }
}

static bool resolve_vertex_semantic(const String& attr_name, MeshSemantics& out_semantic, GPUVertexFormat& out_format)
{
    if (attr_name == "POSITION") {
        out_semantic = MeshSemantics::POSITION;
        out_format   = GPUVertexFormat::FLOAT32x3;
        return true;
    }
    if (attr_name == "NORMAL") {
        out_semantic = MeshSemantics::NORMAL;
        out_format   = GPUVertexFormat::FLOAT32x3;
        return true;
    }
    if (attr_name == "TEXCOORD_0") {
        out_semantic = MeshSemantics::TEXCOORD0;
        out_format   = GPUVertexFormat::FLOAT32x2;
        return true;
    }
    if (attr_name == "TANGENT") {
        out_semantic = MeshSemantics::TANGENT;
        out_format   = GPUVertexFormat::FLOAT32x4;
        return true;
    }
    return false;
}

static void extract_primitive_attribute(
    const tinygltf::Model& gltf_model,
    const String&          attr_name,
    int                    attr_idx,
    MeshLOD&               lod,
    MeshSurface&           surface,
    MeshAsset&             mesh)
{
    if (attr_idx < 0 || static_cast<usize>(attr_idx) >= gltf_model.accessors.size()) return;
    const auto& accessor = gltf_model.accessors[attr_idx];

    if (accessor.bufferView < 0 || static_cast<usize>(accessor.bufferView) >= gltf_model.bufferViews.size()) return;
    const auto& view = gltf_model.bufferViews[accessor.bufferView];

    if (view.buffer < 0 || static_cast<usize>(view.buffer) >= gltf_model.buffers.size()) return;
    const auto& buffer = gltf_model.buffers[view.buffer];

    MeshSemantics   semantic;
    GPUVertexFormat format;
    if (!resolve_vertex_semantic(attr_name, semantic, format)) return;

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

    usize comp_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    usize num_comp  = tinygltf::GetNumComponentsInType(accessor.type);
    usize elem_size = comp_size * num_comp;
    usize stride    = accessor.ByteStride(view);

    surface.slice.first_vertex = static_cast<uint>(attr->element_count);
    surface.slice.vertex_count = static_cast<uint>(accessor.count);

    usize req_bytes = view.byteOffset + accessor.byteOffset + (accessor.count > 0 ? (accessor.count - 1) * stride + elem_size : 0);
    if (buffer.data.size() < req_bytes || accessor.count == 0 || elem_size == 0) return;

    attr->element_count += static_cast<uint>(accessor.count);
    attr->data.resize(attr->data.size() + accessor.count * elem_size);

    uint8_t*       dst = attr->data.data() + surface.slice.first_vertex * elem_size;
    const uint8_t* src = buffer.data.data() + view.byteOffset + accessor.byteOffset;

    for (usize k = 0; k < accessor.count; ++k) {
        memcpy(dst + k * elem_size, src + k * stride, elem_size);
    }
}

static PreviewMesh extract_preview_mesh(
    const tinygltf::Mesh& g_mesh,
    const MeshLOD&        lod,
    const Matrix4x4&      world_transform)
{
    PreviewMesh rmesh;
    rmesh.transform = world_transform;
    if (!g_mesh.primitives.empty() && g_mesh.primitives[0].material >= 0) {
        rmesh.material_id = g_mesh.primitives[0].material;
    }

    for (const auto& attr : lod.attributes) {
        if (attr.semantics == MeshSemantics::POSITION) {
            usize count = attr.element_count;
            if (count * sizeof(Vector3) <= attr.data.size()) {
                rmesh.positions.resize(count);
                memcpy(rmesh.positions.data(), attr.data.data(), count * sizeof(Vector3));
            }
        } else if (attr.semantics == MeshSemantics::NORMAL) {
            usize count = attr.element_count;
            if (count * sizeof(Vector3) <= attr.data.size()) {
                rmesh.normals.resize(count);
                memcpy(rmesh.normals.data(), attr.data.data(), count * sizeof(Vector3));
            }
        } else if (attr.semantics == MeshSemantics::TEXCOORD0) {
            usize count = attr.element_count;
            if (count * sizeof(Vector2) <= attr.data.size()) {
                rmesh.uvs.resize(count);
                memcpy(rmesh.uvs.data(), attr.data.data(), count * sizeof(Vector2));
            }
        }
    }

    if (lod.index_format == GPUIndexFormat::UINT16) {
        usize         count   = lod.index_data.size() / 2;
        const ushort* src_idx = reinterpret_cast<const ushort*>(lod.index_data.data());
        rmesh.indices.resize(count);
        for (usize k = 0; k < count; ++k) {
            rmesh.indices[k] = src_idx[k];
        }
    } else if (lod.index_format == GPUIndexFormat::UINT32) {
        usize       count   = lod.index_data.size() / 4;
        const uint* src_idx = reinterpret_cast<const uint*>(lod.index_data.data());
        rmesh.indices.assign(src_idx, src_idx + count);
    }

    return rmesh;
}

static MeshAsset cook_mesh(
    const tinygltf::Model&       gltf_model,
    const tinygltf::Mesh&        g_mesh,
    const TreeMap<int, AssetID>& material_map)
{
    MeshAsset mesh;
    MeshLOD&  lod = mesh.lods.emplace_back();

    mesh.min_bounds = Vector3(std::numeric_limits<float>::max());
    mesh.max_bounds = Vector3(std::numeric_limits<float>::lowest());

    for (const auto& primitive : g_mesh.primitives) {
        MeshSurface surface;
        surface.min_bounds = Vector3(std::numeric_limits<float>::max());
        surface.max_bounds = Vector3(std::numeric_limits<float>::lowest());

        if (primitive.material >= 0) {
            auto it = material_map.find(primitive.material);
            if (it != material_map.end()) {
                surface.material = AssetHandle<MaterialAsset>(it->second);
            }
        }

        extract_primitive_indices(gltf_model, primitive.indices, lod, surface);

        for (const auto& [attr_name, attr_idx] : primitive.attributes) {
            extract_primitive_attribute(gltf_model, attr_name, attr_idx, lod, surface, mesh);
        }

        lod.surfaces.push_back(surface);
    }
    return mesh;
}

static bool process_gltf(JSON& metadata, OSPath source_path, OSPath caches_root)
{
    try {
        tinygltf::Model    gltf_model;
        tinygltf::TinyGLTF loader;
        String             err;
        String             warn;

        String path_str = Path(source_path).string();
        bool   ret      = (path_str.find(".glb") != String::npos)
                              ? loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path_str.c_str())
                              : loader.LoadASCIIFromFile(&gltf_model, &err, &warn, path_str.c_str());

        if (!warn.empty()) get_logger()->warn("tinygltf: {}", warn);
        if (!err.empty()) get_logger()->error("tinygltf: {}", err);
        if (!ret) return false;

        CacheDirs cache = ensure_cache_dirs(caches_root);

        if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
            return false;
        }

        AssetID              model_id = metadata["guid"].get<AssetID>();
        AssetDependencyScope deps(metadata, source_path, caches_root);

        ModelAsset model;
        if (gltf_model.defaultScene >= 0 && static_cast<usize>(gltf_model.defaultScene) < gltf_model.scenes.size() && !gltf_model.scenes[gltf_model.defaultScene].nodes.empty()) {
            model.root = static_cast<uint>(gltf_model.scenes[gltf_model.defaultScene].nodes[0]);
        } else if (!gltf_model.scenes.empty() && !gltf_model.scenes[0].nodes.empty()) {
            model.root = static_cast<uint>(gltf_model.scenes[0].nodes[0]);
        } else {
            model.root = 0;
        }

        TreeMap<int, AssetID> material_map  = cook_materials(gltf_model, model_id, deps, cache.materials);
        PreviewScene          preview_scene = build_preview_scene(gltf_model);

        Vector<Matrix4x4> local_transforms;
        Vector<Matrix4x4> world_transforms;
        compute_transforms(gltf_model, local_transforms, world_transforms);

        HashSet<int> saved_meshes;
        for (usize i = 0; i < gltf_model.nodes.size(); ++i) {
            const auto&      g_node = gltf_model.nodes[i];
            ModelAsset::Node node;
            node.name      = g_node.name;
            node.transform = local_transforms[i];

            if (g_node.mesh >= 0 && static_cast<usize>(g_node.mesh) < gltf_model.meshes.size()) {
                const auto& g_mesh = gltf_model.meshes[g_node.mesh];
                MeshAsset   mesh   = cook_mesh(gltf_model, g_mesh, material_map);

                String  mesh_name = "mesh/" + (g_mesh.name.empty() ? std::to_string(g_node.mesh) : g_mesh.name);
                AssetID mesh_id   = deps.resolve(model_id, mesh_name, MeshAsset::type);
                deps.set_path(mesh_id, "meshes/" + std::to_string(mesh_id) + ".mesh");

                if (saved_meshes.insert(g_node.mesh).second) {
                    Path mesh_path = cache.meshes / (std::to_string(mesh_id) + ".mesh");
                    MeshAsset::saver().save(&mesh, mesh_path.c_str());
                }
                node.mesh = AssetHandle<MeshAsset>(mesh_id);

                if (!mesh.lods.empty()) {
                    preview_scene.meshes.push_back(extract_preview_mesh(g_mesh, mesh.lods[0], world_transforms[i]));
                }
            }

            for (int child_idx : g_node.children) {
                if (child_idx >= 0 && static_cast<usize>(child_idx) < gltf_model.nodes.size()) {
                    node.children.push_back(static_cast<uint>(child_idx));
                }
            }

            model.nodes.push_back(std::move(node));
        }

        Path model_cache_path = cache.models / (std::to_string(model_id) + ".model");
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
