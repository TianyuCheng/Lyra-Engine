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

struct AtmosphereApp : public TestApp
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

    explicit AtmosphereApp(const TestAppDescriptor& desc) : TestApp(desc)
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

    ~AtmosphereApp() override
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
        // 1. Camera looking across the horizon with slight upward elevation
        auto  cam_entity = ecs_world.create("MainCamera");
        auto& cam        = ecs_world.add_component<Camera>(cam_entity);
        cam.fov          = 55.0f;
        cam.aspect       = float(desc.width) / float(desc.height);
        cam.near_plane   = 0.1f;
        cam.far_plane    = 1000.0f;
        cam.projection   = glm::perspective(glm::radians(cam.fov), cam.aspect, cam.near_plane, cam.far_plane);

        auto& cam_xform    = ecs_world.add_component<TransformLocal>(cam_entity);
        cam_xform.position = Vector3(0.0f, 1.2f, 3.5f);
        cam_xform.rotation = glm::quatLookAt(glm::normalize(Vector3(0.0f, 0.8f, -1.0f)), Vector3(0.0f, 1.0f, 0.0f));

        // 2. Physical Sun & Directional Light
        auto  sun_entity   = ecs_world.create("Sun");
        auto& dir          = ecs_world.add_component<DirectionalLight>(sun_entity);
        dir.direction      = glm::normalize(Vector3(0.2f, -0.6f, -0.7f));
        dir.color          = Vector3(1.0f, 0.98f, 0.94f);
        dir.intensity      = 60000.0f;
        auto& sun          = ecs_world.add_component<Sun>(sun_entity);
        sun.angular_radius = 0.004675f;
        sun.halo_size      = 12.0f;
        sun.halo_falloff   = 80.0f;
        sun.limb_darkening = 0.6f;

        // 3. Ground Plane
        auto  ground_entity   = ecs_world.create("Ground");
        auto& ground_mesh     = ecs_world.add_component<Mesh>(ground_entity);
        ground_mesh.mesh      = MeshAssetHandle(PROCEDURAL_PLANE_GUID);
        ground_mesh.material  = MaterialAssetHandle(PROCEDURAL_MATERIAL_GROUND_GUID);
        auto& ground_xform    = ecs_world.add_component<TransformLocal>(ground_entity);
        ground_xform.position = Vector3(0.0f, 0.0f, 0.0f);
        ground_xform.scale    = Vector3(1.0f);

        // 4. Procedural Sphere
        auto  sphere_entity   = ecs_world.create("Sphere");
        auto& sphere_mesh     = ecs_world.add_component<Mesh>(sphere_entity);
        sphere_mesh.mesh      = MeshAssetHandle(PROCEDURAL_SPHERE_GUID);
        sphere_mesh.material  = MaterialAssetHandle(PROCEDURAL_MATERIAL_SPHERE_GUID);
        auto& sphere_xform    = ecs_world.add_component<TransformLocal>(sphere_entity);
        sphere_xform.position = Vector3(0.0f, 0.5f, 0.0f);
        sphere_xform.scale    = Vector3(1.0f);
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
                depth_desc.label  = "atmosphere_depth_buffer";
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
TEST_CASE("rhi::vulkan::atmosphere" * doctest::description("Atmospheric scattering sky rendering"))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::atmosphere" * doctest::description("Atmospheric scattering sky rendering"))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::atmosphere" * doctest::description("Atmospheric scattering sky rendering"))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::atmosphere" * doctest::description("Atmospheric scattering sky rendering"))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
