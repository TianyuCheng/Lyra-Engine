#include "helper.h"
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

struct RendererForwardMultiLightApp : public TestApp
{
    RendererPlugin        plugin;
    RendererHandle        renderer_handle;
    ResourceRetire        retire;
    Own<GeometryCache>    geo_cache;
    Own<RenderWorld>      render_world;
    World                 ecs_world;
    FrameGraph::Allocator fg_allocator;
    FrameArena            arena;

    explicit RendererForwardMultiLightApp(const TestAppDescriptor& desc) : TestApp(desc)
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

    ~RendererForwardMultiLightApp()
    {
        auto& device = RHI::get_current_device();
        device.wait();
        if (renderer_handle.valid()) {
            plugin.get_api()->delete_renderer(renderer_handle);
        }
    }

    void setup_scene()
    {
        // 1. Camera looking at objects on the ground plane
        auto  cam_entity = ecs_world.create("MainCamera");
        auto& cam        = ecs_world.add_component<Camera>(cam_entity);
        cam.fov          = 45.0f;
        cam.aspect       = float(desc.width) / float(desc.height);
        cam.near_plane   = 0.1f;
        cam.far_plane    = 100.0f;
        cam.projection   = glm::perspective(glm::radians(cam.fov), cam.aspect, cam.near_plane, cam.far_plane);

        auto& cam_xform    = ecs_world.add_component<TransformLocal>(cam_entity);
        cam_xform.position = Vector3(0.0f, 2.5f, 3.5f);
        cam_xform.rotation = glm::quatLookAt(glm::normalize(Vector3(0.0f, 0.3f, 0.0f) - cam_xform.position), Vector3(0.0f, 1.0f, 0.0f));

        // 2. Directional Sun Light (pitch 45 deg, yaw 30 deg; disabled per task 1.1.4 to amplify difference due to local lights)
        auto  sun_entity   = ecs_world.create("Sun");
        auto& dir          = ecs_world.add_component<DirectionalLight>(sun_entity);
        dir.direction      = glm::normalize(Vector3(0.35355f, -0.7071f, -0.61237f));
        dir.color          = Vector3(1.0f, 0.98f, 0.92f);
        dir.intensity      = 0.0f; // sunlight disabled to amplify multi-light differences
        auto& sun          = ecs_world.add_component<Sun>(sun_entity);
        sun.angular_radius = 0.005f;

        // 3. Procedural Cube sitting on the ground plane
        auto  cube_entity   = ecs_world.create("Cube");
        auto& cube_mesh     = ecs_world.add_component<Mesh>(cube_entity);
        cube_mesh.mesh      = MeshAssetHandle(PROCEDURAL_CUBE_GUID);
        cube_mesh.material  = MaterialAssetHandle(PROCEDURAL_MATERIAL_CUBE_GUID);
        auto& cube_xform    = ecs_world.add_component<TransformLocal>(cube_entity);
        cube_xform.position = Vector3(-0.9f, 0.5f, 0.0f);
        cube_xform.rotation = glm::angleAxis(glm::radians(25.0f), Vector3(0.0f, 1.0f, 0.0f));
        cube_xform.scale    = Vector3(1.0f);

        // 4. Procedural Sphere sitting on the ground plane
        auto  sphere_entity   = ecs_world.create("Sphere");
        auto& sphere_mesh     = ecs_world.add_component<Mesh>(sphere_entity);
        sphere_mesh.mesh      = MeshAssetHandle(PROCEDURAL_SPHERE_GUID);
        sphere_mesh.material  = MaterialAssetHandle(PROCEDURAL_MATERIAL_SPHERE_GUID);
        auto& sphere_xform    = ecs_world.add_component<TransformLocal>(sphere_entity);
        sphere_xform.position = Vector3(0.9f, 0.5f, 0.0f);
        sphere_xform.scale    = Vector3(1.0f);

        // 5. Occluder Pillars (Cylinders)
        auto  left_pillar = ecs_world.create("LeftPillar");
        auto& lp_mesh     = ecs_world.add_component<Mesh>(left_pillar);
        lp_mesh.mesh      = MeshAssetHandle(PROCEDURAL_CYLINDER_GUID);
        lp_mesh.material  = MaterialAssetHandle(PROCEDURAL_MATERIAL_CUBE_GUID);
        auto& lp_xform    = ecs_world.add_component<TransformLocal>(left_pillar);
        lp_xform.position = Vector3(-2.2f, 0.75f, -0.5f);
        lp_xform.scale    = Vector3(0.5f, 1.5f, 0.5f);

        auto  right_pillar = ecs_world.create("RightPillar");
        auto& rp_mesh      = ecs_world.add_component<Mesh>(right_pillar);
        rp_mesh.mesh       = MeshAssetHandle(PROCEDURAL_CYLINDER_GUID);
        rp_mesh.material   = MaterialAssetHandle(PROCEDURAL_MATERIAL_CUBE_GUID);
        auto& rp_xform     = ecs_world.add_component<TransformLocal>(right_pillar);
        rp_xform.position  = Vector3(2.2f, 0.75f, -0.5f);
        rp_xform.scale     = Vector3(0.5f, 1.5f, 0.5f);

        // 6. Procedural Ground Plane
        auto  ground_entity   = ecs_world.create("Ground");
        auto& ground_mesh     = ecs_world.add_component<Mesh>(ground_entity);
        ground_mesh.mesh      = MeshAssetHandle(PROCEDURAL_PLANE_GUID);
        ground_mesh.material  = MaterialAssetHandle(PROCEDURAL_MATERIAL_GROUND_GUID);
        auto& ground_xform    = ecs_world.add_component<TransformLocal>(ground_entity);
        ground_xform.position = Vector3(0.0f, 0.0f, 0.0f);
        ground_xform.scale    = Vector3(1.0f);

        // 7. 16 colored point lights arranged across the test chamber
        struct PointLightDef
        {
            Vector3 pos;
            Vector3 color;
            float   intensity;
            float   falloff;
        };

        const PointLightDef point_lights[16] = {
            // Front row (illuminating front faces of cube, sphere, ground)
            {Vector3(-1.6f, 1.0f, 1.4f), Vector3(1.0f, 0.20f, 0.15f), 10.0f, 4.0f}, // Red (front-left of cube)
            {Vector3(-0.6f, 0.9f, 1.5f), Vector3(1.0f, 0.55f, 0.10f), 10.0f, 4.0f}, // Orange (front of cube)
            {Vector3(0.6f, 0.9f, 1.5f), Vector3(1.0f, 0.90f, 0.15f), 10.0f, 4.0f},  // Yellow (front of sphere)
            {Vector3(1.6f, 1.0f, 1.4f), Vector3(0.6f, 1.00f, 0.15f), 10.0f, 4.0f},  // Lime (front-right of sphere)

            // Mid row (sides and elevated lighting)
            {Vector3(-2.2f, 1.2f, 0.2f), Vector3(0.15f, 0.95f, 0.25f), 10.0f, 4.0f}, // Green (left side of cube)
            {Vector3(-0.4f, 1.4f, 0.4f), Vector3(0.10f, 0.95f, 0.70f), 10.0f, 4.0f}, // Teal (between cube and sphere)
            {Vector3(0.4f, 1.4f, 0.4f), Vector3(0.10f, 0.80f, 1.00f), 10.0f, 4.0f},  // Cyan (between sphere and cube)
            {Vector3(2.2f, 1.2f, 0.2f), Vector3(0.20f, 0.55f, 1.00f), 10.0f, 4.0f},  // Sky Blue (right side of sphere)

            // Overhead / top-reflecting row (elevated at y = 1.7..2.0, z = -0.3..-0.9 to create specular highlights on top faces)
            {Vector3(-1.2f, 1.8f, -0.4f), Vector3(0.20f, 0.40f, 1.00f), 10.0f, 4.0f}, // Blue (overhead top-left of cube)
            {Vector3(-0.6f, 1.9f, -0.7f), Vector3(0.65f, 0.15f, 1.00f), 10.0f, 4.0f}, // Indigo (overhead top-center of cube)
            {Vector3(-0.8f, 1.7f, 0.1f), Vector3(0.95f, 0.15f, 0.90f), 10.0f, 4.0f},  // Magenta (overhead top-front of cube)
            {Vector3(0.9f, 1.9f, -0.5f), Vector3(1.00f, 0.15f, 0.40f), 10.0f, 4.0f},  // Rose (overhead top of sphere)

            // Back / rim row (illuminating pillars and creating rim highlights)
            {Vector3(-2.2f, 1.4f, -1.6f), Vector3(1.00f, 0.85f, 0.30f), 10.0f, 4.0f}, // Warm Gold (behind left pillar)
            {Vector3(-0.9f, 1.5f, -1.8f), Vector3(0.95f, 0.95f, 0.95f), 10.0f, 4.0f}, // White (behind cube)
            {Vector3(0.9f, 1.5f, -1.8f), Vector3(0.40f, 0.90f, 1.00f), 10.0f, 4.0f},  // Cool Cyan (behind sphere)
            {Vector3(2.2f, 1.4f, -1.6f), Vector3(0.95f, 0.20f, 0.40f), 10.0f, 4.0f},  // Crimson (behind right pillar)
        };

        for (uint i = 0; i < 16; ++i) {
            auto  light_entity = ecs_world.create("PointLight_" + std::to_string(i));
            auto& pl           = ecs_world.add_component<PointLight>(light_entity);
            pl.color           = point_lights[i].color;
            pl.intensity       = point_lights[i].intensity;
            pl.falloff         = point_lights[i].falloff;

            auto& xform    = ecs_world.add_component<TransformLocal>(light_entity);
            xform.position = point_lights[i].pos;
        }
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

        // 1. Synchronize ECS scene into RenderScene
        render_world->extract(ecs_world, api->get_capabilities());

        // 2. Build frame graph
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

        SceneLighting scene_lighting;
        scene_lighting.init(render_world->get_scene(), arena, device);

        RenderFrameOutputs outputs{};
        RenderFrameContext ctx{
            .builder        = builder,
            .view           = view,
            .scene          = render_world->get_scene(),
            .view_uniforms  = &view_uniforms,
            .scene_lighting = &scene_lighting,
            .color_target   = color_target,
            .depth_target   = {},
            .color_format   = get_backbuffer_format(),
            .depth_format   = GPUTextureFormat::DEPTH32FLOAT,
            .render_extent  = {desc.width, desc.height},
            .display_extent = {desc.width, desc.height},
            .outputs        = &outputs,
        };

        auto final_color = api->render(renderer_handle, ctx);

        // 3. Final present / postprocessing pass
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
        auto fg_ctx = FrameGraphContext{device, swp, command, &arena, &view_uniforms, &scene_lighting};
        graph->execute(&fg_ctx, &fg_allocator);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::forward_renderer_multilight" * doctest::description("Forward+ renderer rendering multi-light stress scene"))
{
    TestAppDescriptor desc{};
    desc.name          = "vulkan";
    desc.window        = false;
    desc.backend       = RHIBackend::VULKAN;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    RendererForwardMultiLightApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::forward_renderer_multilight" * doctest::description("Forward+ renderer rendering multi-light stress scene"))
{
    TestAppDescriptor desc{};
    desc.name          = "d3d12";
    desc.window        = false;
    desc.backend       = RHIBackend::D3D12;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    RendererForwardMultiLightApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::forward_renderer_multilight" * doctest::description("Forward+ renderer rendering multi-light stress scene"))
{
    TestAppDescriptor desc{};
    desc.name          = "metal";
    desc.window        = false;
    desc.backend       = RHIBackend::METAL;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_flags = CompileFlag::DEBUG;
    RendererForwardMultiLightApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::forward_renderer_multilight" * doctest::description("Forward+ renderer rendering multi-light stress scene"))
{
    TestAppDescriptor desc{};
    desc.name          = "softgpu";
    desc.window        = false;
    desc.backend       = RHIBackend::SOFTGPU;
    desc.width         = 640;
    desc.height        = 480;
    desc.rhi_flags     = RHIFlag::DEBUG;
    desc.compile_flags = CompileFlag::DEBUG;
    RendererForwardMultiLightApp(desc).run();
}
