#include "helper.h"
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Rendering/RenderWorld.h>
#include <Lyra/Rendering/ProceduralMesh.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Rendering/Passes/ForwardPass.h>
#include <Lyra/Rendering/Passes/ShadowMapPass.h>
#include <Lyra/Rendering/Passes/LightCullingPass.h>
#include <Lyra/Rendering/Passes/SkyboxPass.h>
#include <Lyra/Rendering/GeometryBindGroupCache.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Sun.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/Transform.h>

struct ShadingModelsApp : public TestApp
{
    passes::ShadowMapPass       shadow_pass;
    passes::LightCullingPass    light_culling_pass;
    passes::SkyboxPass          skybox_pass;
    passes::ForwardPass         forward_pass;
    Ref<GeometryBindGroupCache> geometry_bind_group_cache;
    ResourceRetire              retire;
    Own<GeometryCache>          geo_cache;
    Own<RenderWorld>            render_world;
    World                       ecs_world;
    FrameGraph::Allocator       fg_allocator;
    FrameArena                  arena;

    explicit ShadingModelsApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();

        geometry_bind_group_cache = make_ref<GeometryBindGroupCache>();
        REQUIRE(forward_pass.init(device, *compiler, get_backbuffer_format(), GPUTextureFormat::DEPTH32FLOAT, "sys://Shaders", geometry_bind_group_cache));
        REQUIRE(shadow_pass.init(device, *compiler, GPUTextureFormat::DEPTH32FLOAT, "sys://Shaders", geometry_bind_group_cache));
        REQUIRE(light_culling_pass.init(device, *compiler));
        REQUIRE(skybox_pass.init(device, *compiler, get_backbuffer_format(), GPUTextureFormat::DEPTH32FLOAT));

        geo_cache = make_own<GeometryCache>(retire);
        init_procedural_meshes(*geo_cache);

        render_world = make_own<RenderWorld>(*geo_cache);

        setup_scene();
    }

    ~ShadingModelsApp() override
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();
        device.wait();
        skybox_pass.destroy();
        forward_pass.destroy();
        shadow_pass.destroy();
        light_culling_pass.destroy();
        if (geometry_bind_group_cache) {
            geometry_bind_group_cache->destroy();
            geometry_bind_group_cache.reset();
        }
    }

    void setup_scene()
    {
        // 1. Camera looking at the center of the sphere matrix
        auto  cam_entity = ecs_world.create("MainCamera");
        auto& cam        = ecs_world.add_component<Camera>(cam_entity);
        cam.fov          = 42.0f;
        cam.aspect       = float(desc.width) / float(desc.height);
        cam.near_plane   = 0.1f;
        cam.far_plane    = 100.0f;
        cam.projection   = glm::perspective(glm::radians(cam.fov), cam.aspect, cam.near_plane, cam.far_plane);

        auto& cam_xform    = ecs_world.add_component<TransformLocal>(cam_entity);
        cam_xform.position = Vector3(0.0f, 0.4f, 6.2f);
        cam_xform.rotation = glm::quatLookAt(glm::normalize(Vector3(0.0f, 0.0f, 0.0f) - cam_xform.position), Vector3(0.0f, 1.0f, 0.0f));

        // 2. Directional Sun Light illuminating spheres with strong specular and shadows
        auto  sun_entity   = ecs_world.create("Sun");
        auto& dir          = ecs_world.add_component<DirectionalLight>(sun_entity);
        dir.direction      = glm::normalize(Vector3(0.5f, -0.7f, -0.6f));
        dir.color          = Vector3(1.0f, 0.98f, 0.95f);
        dir.intensity      = 25000.0f;
        auto& sun          = ecs_world.add_component<Sun>(sun_entity);
        sun.angular_radius = 0.005f;

        // 3. Neutral backdrop plane behind the spheres
        auto  backdrop_entity   = ecs_world.create("Backdrop");
        auto& backdrop_mesh     = ecs_world.add_component<Mesh>(backdrop_entity);
        backdrop_mesh.mesh      = MeshAssetHandle(PROCEDURAL_PLANE_GUID);
        backdrop_mesh.material  = MaterialAssetHandle(PROCEDURAL_MATERIAL_GROUND_GUID);
        auto& backdrop_xform    = ecs_world.add_component<TransformLocal>(backdrop_entity);
        backdrop_xform.position = Vector3(0.0f, 0.0f, -0.6f);
        backdrop_xform.rotation = glm::angleAxis(glm::radians(-90.0f), Vector3(1.0f, 0.0f, 0.0f));
        backdrop_xform.scale    = Vector3(10.0f);

        // 4. Matrix of 16 Spheres (4 columns x 4 rows)
        // Col roughness: 0.05, 0.35, 0.65, 0.95
        // Row 0: Standard PBR Dielectric (deep blue)
        // Row 1: Standard PBR Metal (gold)
        // Row 2: Clearcoat (ruby red with glossy coat)
        // Row 3: Subsurface (translucent jade/marble)
        static constexpr float ROUGHNESS_COLS[4] = {0.05f, 0.35f, 0.65f, 0.95f};

        for (uint row = 0; row < 4; ++row) {
            for (uint col = 0; col < 4; ++col) {
                AssetID       mat_guid = 0x54ADE00000000000ULL + (row * 4 + col);
                MaterialAsset mat{};

                float roughness = ROUGHNESS_COLS[col];

                if (row == 0) {
                    // Standard Dielectric
                    mat.shading_model     = ShadingModel::STANDARD;
                    mat.shading_model_id  = BuiltinShadingModels::STANDARD;
                    mat.base_color_factor = Vector4(0.12f, 0.38f, 0.85f, 1.0f);
                    mat.metallic_factor   = 0.0f;
                    mat.roughness_factor  = roughness;
                } else if (row == 1) {
                    // Standard Metal
                    mat.shading_model     = ShadingModel::STANDARD;
                    mat.shading_model_id  = BuiltinShadingModels::STANDARD;
                    mat.base_color_factor = Vector4(1.0f, 0.78f, 0.34f, 1.0f);
                    mat.metallic_factor   = 1.0f;
                    mat.roughness_factor  = roughness;
                } else if (row == 2) {
                    // Clearcoat
                    mat.shading_model        = ShadingModel::CLEARCOAT;
                    mat.shading_model_id     = BuiltinShadingModels::CLEARCOAT;
                    mat.base_color_factor    = Vector4(0.85f, 0.12f, 0.15f, 1.0f);
                    mat.metallic_factor      = 0.0f;
                    mat.roughness_factor     = roughness;
                    mat.clear_coat_factor    = 1.0f;
                    mat.clear_coat_roughness = 0.04f;
                } else {
                    // Subsurface
                    mat.shading_model     = ShadingModel::SUBSURFACE;
                    mat.shading_model_id  = BuiltinShadingModels::SUBSURFACE;
                    mat.base_color_factor = Vector4(0.72f, 0.88f, 0.78f, 1.0f);
                    mat.metallic_factor   = 0.0f;
                    mat.roughness_factor  = roughness;
                    mat.custom_data       = Vector4(0.3f, 0.6f, 0.4f, 1.0f); // SSS profile
                }

                render_world->register_material(mat_guid, mat);

                String name   = "Sphere_R" + std::to_string(row) + "_C" + std::to_string(col);
                auto   entity = ecs_world.create(name);
                auto&  mesh   = ecs_world.add_component<Mesh>(entity);
                mesh.mesh     = MeshAssetHandle(PROCEDURAL_SPHERE_GUID);
                mesh.material = MaterialAssetHandle(mat_guid);

                auto& xform    = ecs_world.add_component<TransformLocal>(entity);
                float x_pos    = (float(col) - 1.5f) * 1.15f;
                float y_pos    = (1.5f - float(row)) * 1.15f;
                xform.position = Vector3(x_pos, y_pos, 0.0f);
                xform.scale    = Vector3(0.92f);
            }
        }
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        if (skipped) return;

        auto& device = RHI::get_current_device();

        auto command = execute([&]() {
            auto cmd_desc  = GPUCommandBufferDescriptor{};
            cmd_desc.queue = GPUQueueType::DEFAULT;
            return device.create_command_buffer(cmd_desc);
        });

        if (desc.window) {
            command.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
            command.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);
        }

        // update world transforms
        auto update_xform = [&](Entity e) {
            if (auto tl = ecs_world.registry.try_get<TransformLocal>(e)) {
                Matrix4x4 t = glm::translate(Matrix4x4(1.0f), tl->position);
                Matrix4x4 r = Matrix4x4(glm::mat4_cast(tl->rotation));
                Matrix4x4 s = glm::scale(Matrix4x4(1.0f), tl->scale);
                ecs_world.add_component<TransformWorld>(e, TransformWorld{.xform = t * r * s});
            }
        };
        for (auto [entity] : ecs_world.registry.storage<Entity>().each()) {
            update_xform(entity);
        }

        RendererCapabilities caps{};
        caps.features               = RendererFeature::RASTERIZATION | RendererFeature::POST_PROCESSING | RendererFeature::DEBUG_VISUALIZATION;
        caps.supported_primitives   = PrimitiveSupport::MESH;
        caps.max_directional_lights = 4;
        caps.max_local_lights       = 4096;
        caps.max_samples_per_pixel  = 1;
        caps.supports_hdr           = true;
        render_world->extract(ecs_world, caps);

        // 2. Build frame graph
        FrameGraphBuilder builder;

        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        struct DepthPassData
        {
            FrameGraphHandle<FrameGraphTexture> depth;
        };
        auto depth_alloc = builder.add_pass<DepthPassData>(
            "DepthAllocPass",
            [&](DepthPassData& data, FrameGraphPassBuilder& pass_builder) {
                auto depth_desc   = GPUTextureDescriptor{};
                depth_desc.label  = "shading_models_depth_buffer";
                depth_desc.size   = {desc.width, desc.height, 1};
                depth_desc.format = GPUTextureFormat::DEPTH32FLOAT;
                depth_desc.usage  = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                data.depth        = pass_builder.create<FrameGraphTexture>(depth_desc);
            },
            [](const DepthPassData&, FrameGraphResources&, FrameGraphContext*) {
            });
        auto depth_target = depth_alloc.depth;

        RenderView view    = render_world->get_view();
        view.render_extent = {desc.width, desc.height};

        ViewUniforms view_uniforms;
        view_uniforms.init(view, arena);

        const auto& scene = render_world->get_scene();

        // 1. Cascaded shadow mapping pass
        FrameGraphHandle<FrameGraphTexture> shadow_map_h{};
        if (!scene.directional_lights.empty() && shadow_pass.valid()) {
            auto s_data  = shadow_pass.add(builder, view, scene);
            shadow_map_h = s_data.shadow_map_array;
        }

        // 2. Light culling pass
        auto           culling_data = light_culling_pass.add(builder, view, scene, {});
        GPUBufferSlice lights_slice = light_culling_pass.get_lights_slice();

        // 3. Skybox pass (clears color and depth)
        auto sky_data      = skybox_pass.add(builder, color_target, depth_target, view, scene, GPULoadOp::CLEAR, GPULoadOp::CLEAR);
        auto current_color = sky_data.color_output;

        // 4. Forward base pass
        passes::ForwardPassParams params{
            .color_target         = current_color,
            .depth_target         = depth_target,
            .shadow_map           = shadow_map_h,
            .cluster_grid_buffer  = culling_data.cluster_grid_buffer,
            .light_index_list     = culling_data.light_index_list,
            .lights_slice         = lights_slice,
            .cluster_grid_x       = culling_data.cluster_grid_x,
            .cluster_grid_y       = culling_data.cluster_grid_y,
            .cluster_grid_z       = culling_data.cluster_grid_z,
            .num_lights           = static_cast<uint>(scene.directional_lights.size() + scene.local_lights.size()),
            .light_view_proj      = shadow_pass.get_light_view_proj(),
            .view                 = view,
            .scene                = scene,
            .extent               = {desc.width, desc.height},
            .color_format         = get_backbuffer_format(),
            .depth_format         = GPUTextureFormat::DEPTH32FLOAT,
            .depth_prepass_active = false,
            .color_load_op        = GPULoadOp::LOAD,
            .depth_load_op        = GPULoadOp::LOAD,
        };
        auto fp_data     = forward_pass.add(builder, params);
        auto final_color = fp_data.color_target;

        // 5. Final present / postprocessing pass
        builder.add_pass(
            "present_pass",
            [&](FrameGraph::PassBuilder& pass) {
                (void)pass.read(final_color, FrameGraphReadOp::NOP);
                pass.preserve();
            },
            [this, final_color](FrameGraph::Resources& res, FrameGraphContext* fg_ctx) {
                auto tex = res.get<FrameGraph::Texture>(final_color);
                postprocessing(fg_ctx->cmdlist, tex->texture);
            });

        auto graph  = builder.build();
        auto fg_ctx = FrameGraphContext{device, swp, command, &arena, &view_uniforms};
        graph->execute(&fg_ctx, &fg_allocator);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::shading_models" * doctest::description("Sphere matrix verifying unified shading models under scene lighting"))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ShadingModelsApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::shading_models" * doctest::description("Sphere matrix verifying unified shading models under scene lighting"))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ShadingModelsApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::shading_models" * doctest::description("Sphere matrix verifying unified shading models under scene lighting"))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    ShadingModelsApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::shading_models" * doctest::description("Sphere matrix verifying unified shading models under scene lighting"))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    ShadingModelsApp(desc).run();
}
