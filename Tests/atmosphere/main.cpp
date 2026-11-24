#include "helper.h"
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/RenderWorld.h>
#include <Lyra/Rendering/ProceduralMesh.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Sun.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/Transform.h>

struct AtmosphereApp : public TestApp
{
    RendererPlugin        plugin;
    RendererHandle        renderer_handle;
    ResourceRetire        retire;
    Own<GeometryCache>    geo_cache;
    Own<RenderWorld>      render_world;
    World                 ecs_world;
    FrameGraph::Allocator fg_allocator;
    FrameArena            arena;

    explicit AtmosphereApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        auto& device = RHI::get_current_device();

        REQUIRE(plugin.load("lyra-forward"));
        auto api = plugin.get_api();
        REQUIRE(api != nullptr);

        RendererDescriptor rdesc{};
        rdesc.device         = device;
        rdesc.compiler       = *compiler;
        rdesc.initial_extent = {desc.width, desc.height};
        rdesc.color_format   = get_backbuffer_format();
        rdesc.depth_format   = GPUTextureFormat::DEPTH32FLOAT;
        REQUIRE(api->create_renderer(renderer_handle, rdesc));
        REQUIRE(renderer_handle.valid());

        RendererSettings settings{};
        api->get_settings(renderer_handle, settings);
        settings.enable_shadows = true;
        api->set_settings(renderer_handle, settings);

        geo_cache = make_own<GeometryCache>(retire);
        init_procedural_meshes(*geo_cache);

        render_world = make_own<RenderWorld>(*geo_cache);

        setup_scene();
    }

    ~AtmosphereApp()
    {
        auto& device = RHI::get_current_device();
        device.wait();
        if (renderer_handle.valid()) {
            plugin.get_api()->delete_renderer(renderer_handle);
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

        auto api = plugin.get_api();

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

        render_world->extract(ecs_world, api->get_capabilities());

        FrameGraphBuilder builder;

        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        RenderView view    = render_world->get_view();
        view.render_extent = {desc.width, desc.height};

        ViewUniforms view_uniforms;
        view_uniforms.init(view, arena);

        RenderFrameOutputs outputs{};
        RenderFrameContext ctx{
            .builder        = builder,
            .view           = view,
            .scene          = render_world->get_scene(),
            .view_uniforms  = &view_uniforms,
            .color_target   = color_target,
            .depth_target   = {},
            .color_format   = get_backbuffer_format(),
            .depth_format   = GPUTextureFormat::DEPTH32FLOAT,
            .render_extent  = {desc.width, desc.height},
            .display_extent = {desc.width, desc.height},
            .outputs        = &outputs,
        };

        auto final_color = api->render(renderer_handle, ctx);

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
    desc.name           = "vulkan";
    desc.window         = false;
    desc.backend        = RHIBackend::VULKAN;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::SPIRV;
    desc.compile_flags  = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::atmosphere" * doctest::description("Atmospheric scattering sky rendering"))
{
    TestAppDescriptor desc{};
    desc.name           = "d3d12";
    desc.window         = false;
    desc.backend        = RHIBackend::D3D12;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::DXIL;
    desc.compile_flags  = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::atmosphere" * doctest::description("Atmospheric scattering sky rendering"))
{
    TestAppDescriptor desc{};
    desc.name           = "metal";
    desc.window         = false;
    desc.backend        = RHIBackend::METAL;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target = CompileTarget::MSL;
    desc.compile_flags  = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::atmosphere" * doctest::description("Atmospheric scattering sky rendering"))
{
    TestAppDescriptor desc{};
    desc.name           = "softgpu";
    desc.window         = false;
    desc.backend        = RHIBackend::SOFTGPU;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG;
    desc.compile_target = CompileTarget::HOST;
    desc.compile_flags  = CompileFlag::DEBUG;
    AtmosphereApp(desc).run();
}
