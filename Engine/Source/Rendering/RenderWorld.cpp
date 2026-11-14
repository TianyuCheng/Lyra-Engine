#include <Lyra/Rendering/RenderWorld.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/SDFVolume.h>
#include <Lyra/Components/Transform.h>
#include <Lyra/Rendering/ProceduralMesh.h>
#include <Lyra/Assets/Format/MaterialAsset.h>
#include <Lyra/Utilities/Logger.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace lyra;

RenderWorld::RenderWorld(GeometryCache& geometry_cache, AssetServer* ams)
    : geometry_cache(geometry_cache), ams(ams)
{
    // do nothing else
}

// todo: evolve full per-frame extraction to retained gpu scene:
//   1. spatial acceleration (octree/bvh) for camera frustum extraction
//   2. dirty transform tracking to avoid full re-extraction of static geometry
//   3. persistent gpu instance buffers with compute culling
void RenderWorld::extract(
    const World&                ecs_world,
    const RendererCapabilities& capabilities,
    float /*delta_time*/)
{
    scene.clear();

    extract_view(ecs_world);
    extract_lights(ecs_world);
    extract_meshes(ecs_world, capabilities);
    extract_sdfs(ecs_world);
    build_acceleration_structures(capabilities);
}

void RenderWorld::extract_view(const World& ecs_world)
{
    auto cam_view = ecs_world.registry.view<Camera>();
    for (auto entity : cam_view) {
        auto& cam = cam_view.get<Camera>(entity);

        Matrix4x4 world_xform(1.0f);
        if (auto* tw = ecs_world.registry.try_get<TransformWorld>(entity)) {
            world_xform = tw->xform;
        } else if (auto* tl = ecs_world.registry.try_get<TransformLocal>(entity)) {
            Matrix4x4 t = glm::translate(Matrix4x4(1.0f), tl->position);
            Matrix4x4 r = glm::mat4_cast(tl->rotation);
            Matrix4x4 s = glm::scale(Matrix4x4(1.0f), tl->scale);
            world_xform = t * r * s;
        }

        Matrix4x4 proj = cam.projection;
        if (cam.aspect > 0.0f) {
            if (cam.type == ProjectionType::PERSPECTIVE) {
                proj = glm::perspective(glm::radians(cam.fov), cam.aspect, cam.near_plane, cam.far_plane);
            } else {
                float half_size = cam.size * 0.5f;
                proj            = glm::ortho(-half_size * cam.aspect, half_size * cam.aspect, -half_size, half_size, cam.near_plane, cam.far_plane);
            }
        }

        view.projection          = proj;
        view.view                = glm::inverse(world_xform);
        view.view_projection     = view.projection * view.view;
        view.inv_view            = world_xform;
        view.inv_projection      = glm::inverse(proj);
        view.inv_view_projection = glm::inverse(view.view_projection);
        view.camera_position     = Vector3(world_xform[3]);
        view.near_plane          = cam.near_plane;
        view.far_plane           = cam.far_plane;
        view.fov                 = cam.fov;
        break; // primary camera
    }
}

void RenderWorld::extract_lights(const World& ecs_world)
{
    // directional lights
    auto dir_view = ecs_world.registry.view<DirectionalLight>();
    for (auto entity : dir_view) {
        auto& light = dir_view.get<DirectionalLight>(entity);

        Matrix4x4 world_xform(1.0f);
        if (auto* tw = ecs_world.registry.try_get<TransformWorld>(entity)) {
            world_xform = tw->xform;
        }

        auto l      = RenderLight{};
        l.type      = RenderLightType::DIRECTIONAL;
        l.direction = glm::normalize(Matrix3x3(world_xform) * light.direction);
        l.color     = light.color;
        l.intensity = light.intensity;
        scene.directional_lights.push_back(l);
    }

    // point lights
    auto point_view = ecs_world.registry.view<PointLight>();
    for (auto entity : point_view) {
        auto& light = point_view.get<PointLight>(entity);

        Matrix4x4 world_xform(1.0f);
        if (auto* tw = ecs_world.registry.try_get<TransformWorld>(entity)) {
            world_xform = tw->xform;
        }

        auto l      = RenderLight{};
        l.type      = RenderLightType::POINT;
        l.position  = Vector3(world_xform[3]) + light.position;
        l.color     = light.color;
        l.intensity = light.intensity;
        l.range     = light.falloff;
        scene.local_lights.push_back(l);
    }

    // spot lights
    auto spot_view = ecs_world.registry.view<SpotLight>();
    for (auto entity : spot_view) {
        auto& light = spot_view.get<SpotLight>(entity);

        Matrix4x4 world_xform(1.0f);
        if (auto* tw = ecs_world.registry.try_get<TransformWorld>(entity)) {
            world_xform = tw->xform;
        }

        auto l             = RenderLight{};
        l.type             = RenderLightType::SPOT;
        l.position         = Vector3(world_xform[3]) + light.position;
        l.direction        = glm::normalize(Matrix3x3(world_xform) * light.direction);
        l.color            = light.color;
        l.intensity        = light.intensity;
        l.range            = light.falloff;
        l.spot_inner_angle = light.angle.x;
        l.spot_outer_angle = light.angle.y;
        scene.local_lights.push_back(l);
    }
}

void RenderWorld::extract_meshes(const World& ecs_world, const RendererCapabilities& capabilities)
{
    // determine geometry forms required by the active renderer
    GeometryForms requested = GeometryForm::INDEXED;
    if (capabilities.supported_primitives.contains(PrimitiveSupport::MESHLET)) {
        requested |= GeometryForm::MESHLET;
    }
    if (capabilities.supported_primitives.contains(PrimitiveSupport::BVH)) {
        requested |= GeometryForm::RAY_TRACING;
    }

    auto mesh_view = ecs_world.registry.view<Mesh>();
    for (auto entity : mesh_view) {
        auto& mesh_comp = mesh_view.get<Mesh>(entity);
        if (!mesh_comp.mesh.valid()) {
            continue;
        }

        Matrix4x4 world_xform(1.0f);
        if (auto* tw = ecs_world.registry.try_get<TransformWorld>(entity)) {
            world_xform = tw->xform;
        } else if (auto* tl = ecs_world.registry.try_get<TransformLocal>(entity)) {
            Matrix4x4 t = glm::translate(Matrix4x4(1.0f), tl->position);
            Matrix4x4 r = glm::mat4_cast(tl->rotation);
            Matrix4x4 s = glm::scale(Matrix4x4(1.0f), tl->scale);
            world_xform = t * r * s;
        }

        // resolve or acquire gpu geometry
        const MeshGeometryEntry* entry = geometry_cache.get(mesh_comp.mesh);
        if (!entry) {
            // 1. check standard procedural mesh primitives
            if (const auto* proc = find_procedural_mesh(mesh_comp.mesh.guid)) {
                entry = geometry_cache.acquire(mesh_comp.mesh, *proc, requested | mesh_comp.preferred_forms);
            }
            // 2. check asset server for cooked/imported assets
            else if (ams) {
                auto mesh_asset = ams->get_asset(mesh_comp.mesh);
                if (!mesh_asset) {
                    ams->load_asset<MeshAsset>(mesh_comp.mesh.guid);
                    mesh_asset = ams->get_asset(mesh_comp.mesh);
                }
                if (mesh_asset) {
                    entry = geometry_cache.acquire(mesh_comp.mesh, *mesh_asset, requested | mesh_comp.preferred_forms);
                }
            }
        } else {
            // ensure any newly requested geometry forms are loaded
            auto needed = (requested | mesh_comp.preferred_forms);
            if ((entry->loaded_forms & needed) != needed) {
                if (const auto* proc = find_procedural_mesh(mesh_comp.mesh.guid)) {
                    entry = geometry_cache.acquire(mesh_comp.mesh, *proc, needed);
                } else if (ams) {
                    if (auto mesh_asset = ams->get_asset(mesh_comp.mesh)) {
                        entry = geometry_cache.acquire(mesh_comp.mesh, *mesh_asset, needed);
                    }
                }
            }
        }

        if (!entry || entry->lods.empty()) {
            continue;
        }

        const auto& lod0 = entry->lods[0];

        // resolve material shading model
        ShadingModel shading_model = ShadingModel::PBR;
        auto         mat_handle    = mesh_comp.material;
        if (mat_handle.valid() && ams) {
            auto mat_ptr = ams->get_asset(mat_handle);
            if (!mat_ptr) {
                ams->load_asset<MaterialAsset>(mat_handle.guid);
                mat_ptr = ams->get_asset(mat_handle);
            }
            if (mat_ptr) {
                shading_model = mat_ptr->shading_model;
            }
        }

        // 1. indexed trimesh extraction
        if (capabilities.supported_primitives.contains(PrimitiveSupport::MESH) && lod0.indexed.has_value()) {
            const auto& geom = *lod0.indexed;
            if (lod0.surfaces.empty()) {
                auto prim          = MeshPrimitive{};
                prim.geometry      = geom;
                prim.first_index   = 0;
                prim.index_count   = geom.index_count;
                prim.material      = mesh_comp.material;
                prim.transform     = world_xform;
                prim.world_bounds  = entry->local_bounds.transformed(world_xform);
                prim.instance_id   = static_cast<uint>(entt::to_integral(entity));
                prim.shading_model = shading_model;
                scene.meshes.push_back(prim);
            } else {
                for (uint s_idx = 0; s_idx < lod0.surfaces.size(); ++s_idx) {
                    const auto& surf   = lod0.surfaces[s_idx];
                    auto        prim   = MeshPrimitive{};
                    prim.geometry      = geom;
                    prim.submesh_index = s_idx;
                    prim.first_index   = surf.slice.first_index;
                    prim.index_count   = surf.slice.index_count;
                    prim.material      = surf.material.valid() ? surf.material : mesh_comp.material;
                    prim.transform     = world_xform;
                    prim.world_bounds  = Bounds3D(surf.min_bounds, surf.max_bounds).transformed(world_xform);
                    prim.instance_id   = static_cast<uint>(entt::to_integral(entity));
                    prim.shading_model = shading_model;
                    scene.meshes.push_back(prim);
                }
            }
        }

        // 2. meshlet cluster presentation extraction
        if (capabilities.supported_primitives.contains(PrimitiveSupport::MESHLET) && lod0.meshlet.has_value()) {
            auto prim           = MeshletPrimitive{};
            prim.geometry       = *lod0.meshlet;
            prim.meshlet_offset = 0;
            prim.meshlet_count  = lod0.meshlet->meshlet_count;
            prim.material       = mesh_comp.material;
            prim.transform      = world_xform;
            prim.world_bounds   = entry->local_bounds.transformed(world_xform);
            prim.instance_id    = static_cast<uint>(entt::to_integral(entity));
            prim.shading_model  = shading_model;
            scene.meshlets.push_back(prim);
        }

        // 3. ray tracing bvh instance extraction
        if (capabilities.supported_primitives.contains(PrimitiveSupport::BVH) && lod0.ray_tracing.has_value()) {
            auto prim                  = BVHPrimitive{};
            prim.blas                  = lod0.ray_tracing->blas.handle;
            prim.instance_custom_index = static_cast<uint>(entt::to_integral(entity));
            prim.mask                  = 0xFF;
            prim.transform             = world_xform;
            prim.world_bounds          = entry->local_bounds.transformed(world_xform);
            scene.bvh_instances.push_back(prim);
        }
    }
}

void RenderWorld::extract_sdfs(const World& ecs_world)
{
    auto sdf_view = ecs_world.registry.view<SDFVolume>();
    for (auto entity : sdf_view) {
        auto& sdf_comp = sdf_view.get<SDFVolume>(entity);

        Matrix4x4 world_xform(1.0f);
        if (auto* tw = ecs_world.registry.try_get<TransformWorld>(entity)) {
            world_xform = tw->xform;
        } else if (auto* tl = ecs_world.registry.try_get<TransformLocal>(entity)) {
            Matrix4x4 t = glm::translate(Matrix4x4(1.0f), tl->position);
            Matrix4x4 r = glm::mat4_cast(tl->rotation);
            Matrix4x4 s = glm::scale(Matrix4x4(1.0f), tl->scale);
            world_xform = t * r * s;
        }

        auto prim         = SDFPrimitive{};
        prim.shape_type   = sdf_comp.shape_type;
        prim.blend_op     = sdf_comp.blend_op;
        prim.parameters   = sdf_comp.parameters;
        prim.material     = sdf_comp.material;
        prim.transform    = world_xform;
        prim.world_bounds = Bounds3D(Vector3(-sdf_comp.parameters.x), Vector3(sdf_comp.parameters.x)).transformed(world_xform);
        prim.instance_id  = static_cast<uint>(entt::to_integral(entity));

        if (sdf_comp.material.valid() && ams) {
            if (auto mat_ptr = ams->get_asset(sdf_comp.material)) {
                prim.shading_model = mat_ptr->shading_model;
            }
        }

        scene.sdfs.push_back(prim);
    }
}

void RenderWorld::build_acceleration_structures(const RendererCapabilities& capabilities)
{
    if (!capabilities.features.contains(RendererFeature::HARDWARE_RAYTRACING) || scene.bvh_instances.empty()) {
        return;
    }

    if (!RHI::api()) {
        return;
    }

    auto& device = RHI::get_current_device();
    if (!device.features.raytracing) {
        return;
    }

    // allocate / resize TLAS if needed
    if (!scene.scene_tlas.valid()) {
        auto tlas_desc          = GPUTlasDescriptor{};
        tlas_desc.label         = "render_world_scene_tlas";
        tlas_desc.flags         = GPUBVHFlag::PREFER_FAST_TRACE;
        tlas_desc.max_instances = static_cast<uint>(std::max<usize>(scene.bvh_instances.size(), 64));
        auto tlas               = device.create_tlas(tlas_desc);
        scene.scene_tlas        = tlas.handle;
    }
}
