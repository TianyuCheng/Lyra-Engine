#include "helper.h"
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/FrameArena.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIInits.h>

struct PathTracerApp : public TestApp
{
    RendererPlugin        plugin;
    RendererHandle        renderer_handle;
    FrameGraph::Allocator fg_allocator;
    FrameArena            arena;
    RenderView            view{};

    Vector<Vertex> cornell_vertices;
    Vector<uint>   cornell_indices;

    GPUBuffer cornell_vbuffer;
    GPUBuffer cornell_ibuffer;
    GPUBlas   cornell_blas;
    GPUTlas   cornell_tlas;
    GPUBuffer cornell_scratch;
    usize     scratch_size = 0;

    void add_quad(
        const Vector3& p0,
        const Vector3& p1,
        const Vector3& p2,
        const Vector3& p3,
        const Vector3& normal,
        const Vector3& color = Vector3(1.0f))
    {
        uint base = static_cast<uint>(cornell_vertices.size());
        cornell_vertices.push_back({p0, normal, color, {0.0f, 0.0f}});
        cornell_vertices.push_back({p1, normal, color, {1.0f, 0.0f}});
        cornell_vertices.push_back({p2, normal, color, {1.0f, 1.0f}});
        cornell_vertices.push_back({p3, normal, color, {0.0f, 1.0f}});

        cornell_indices.push_back(base + 0);
        cornell_indices.push_back(base + 1);
        cornell_indices.push_back(base + 2);
        cornell_indices.push_back(base + 0);
        cornell_indices.push_back(base + 2);
        cornell_indices.push_back(base + 3);
    }

    void add_box(const Vector3& center, const Vector3& size, float rot_y)
    {
        Matrix4x4 t     = glm::translate(Matrix4x4(1.0f), center);
        Matrix4x4 r     = glm::rotate(Matrix4x4(1.0f), rot_y, Vector3(0.0f, 1.0f, 0.0f));
        Matrix4x4 s     = glm::scale(Matrix4x4(1.0f), size * 0.5f);
        Matrix4x4 xform = t * r * s;

        auto xf = [&](float x, float y, float z) -> Vector3 {
            return Vector3(xform * Vector4(x, y, z, 1.0f));
        };

        // front (+Z)
        add_quad(xf(-1, -1, 1), xf(1, -1, 1), xf(1, 1, 1), xf(-1, 1, 1), Vector3(0, 0, 1));
        // back (-Z)
        add_quad(xf(1, -1, -1), xf(-1, -1, -1), xf(-1, 1, -1), xf(1, 1, -1), Vector3(0, 0, -1));
        // left (-X)
        add_quad(xf(-1, -1, -1), xf(-1, -1, 1), xf(-1, 1, 1), xf(-1, 1, -1), Vector3(-1, 0, 0));
        // right (+X)
        add_quad(xf(1, -1, 1), xf(1, -1, -1), xf(1, 1, -1), xf(1, 1, 1), Vector3(1, 0, 0));
        // top (+Y)
        add_quad(xf(-1, 1, 1), xf(1, 1, 1), xf(1, 1, -1), xf(-1, 1, -1), Vector3(0, 1, 0));
        // bottom (-Y)
        add_quad(xf(-1, -1, -1), xf(1, -1, -1), xf(1, -1, 1), xf(-1, -1, 1), Vector3(0, -1, 0));
    }

    void build_cornell_geometry()
    {
        // 1. Floor (y = -1.0)
        add_quad({-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f});

        // 2. Ceiling with central aperture (y = 1.0)
        add_quad({-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -0.35f}, {-1.0f, 1.0f, -0.35f}, {0.0f, -1.0f, 0.0f});
        add_quad({-1.0f, 1.0f, 0.35f}, {1.0f, 1.0f, 0.35f}, {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f});
        add_quad({-1.0f, 1.0f, -0.35f}, {-0.35f, 1.0f, -0.35f}, {-0.35f, 1.0f, 0.35f}, {-1.0f, 1.0f, 0.35f}, {0.0f, -1.0f, 0.0f});
        add_quad({0.35f, 1.0f, -0.35f}, {1.0f, 1.0f, -0.35f}, {1.0f, 1.0f, 0.35f}, {0.35f, 1.0f, 0.35f}, {0.0f, -1.0f, 0.0f});

        // 3. Back wall (z = -1.0)
        add_quad({-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f});

        // 4. Left wall (red, x = -1.0)
        add_quad({-1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f});

        // 5. Right wall (green, x = 1.0)
        add_quad({1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f});

        // 6. Tall block (left-back)
        add_box({-0.35f, -0.3f, -0.35f}, {0.35f, 0.7f, 0.35f}, 0.314f);

        // 7. Short block (right-front)
        add_box({0.35f, -0.65f, 0.3f}, {0.35f, 0.35f, 0.35f}, -0.314f);
    }

    void setup_buffers()
    {
        build_cornell_geometry();

        auto& current_device = RHI::get_current_device();

        // Vertex buffer
        GPUBufferDescriptor vdesc{};
        vdesc.label              = "cornell_vbuffer";
        vdesc.size               = sizeof(Vertex) * cornell_vertices.size();
        vdesc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
        vdesc.mapped_at_creation = true;
        cornell_vbuffer          = current_device.create_buffer(vdesc);
        auto vmapped             = cornell_vbuffer.get_mapped_range<Vertex>();
        std::memcpy(vmapped.data, cornell_vertices.data(), vdesc.size);
        cornell_vbuffer.unmap();

        // Index buffer
        GPUBufferDescriptor idesc{};
        idesc.label              = "cornell_ibuffer";
        idesc.size               = sizeof(uint) * cornell_indices.size();
        idesc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
        idesc.mapped_at_creation = true;
        cornell_ibuffer          = current_device.create_buffer(idesc);
        auto imapped             = cornell_ibuffer.get_mapped_range<uint>();
        std::memcpy(imapped.data, cornell_indices.data(), idesc.size);
        cornell_ibuffer.unmap();

        // Camera view setup
        view.render_extent        = {desc.width, desc.height};
        view.fov                  = 45.0f;
        view.near_plane           = 0.1f;
        view.far_plane            = 100.0f;
        view.camera_position      = Vector3(0.0f, 0.0f, 2.7f);
        view.projection           = glm::perspective(glm::radians(view.fov), float(desc.width) / float(desc.height), view.near_plane, view.far_plane);
        view.view                 = glm::lookAt(view.camera_position, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        view.view_projection      = view.projection * view.view;
        view.inv_view             = glm::inverse(view.view);
        view.inv_projection       = glm::inverse(view.projection);
        view.inv_view_projection  = glm::inverse(view.view_projection);
        view.prev_view_projection = view.view_projection;
    }

    void setup_as()
    {
        auto& current_device = RHI::get_current_device();

        // BLAS
        GPUBlasDescriptor bdesc{};
        bdesc.label = "cornell_blas";
        bdesc.flags = GPUBVHFlag::PREFER_FAST_TRACE;

        GPUBlasGeometrySizeDescriptor size{};
        size.type                    = GPUBlasType::TRIANGLE;
        size.triangles.flags         = GPUBVHGeometryFlag::BVH_OPAQUE;
        size.triangles.vertex_format = GPUVertexFormat::FLOAT32x3;
        size.triangles.vertex_count  = static_cast<uint>(cornell_vertices.size());
        size.triangles.index_count   = static_cast<uint>(cornell_indices.size());
        size.triangles.index_format  = GPUIndexFormat::UINT32;

        cornell_blas = current_device.create_blas(bdesc, {size});

        // TLAS
        GPUTlasDescriptor tdesc{};
        tdesc.label         = "cornell_tlas";
        tdesc.flags         = GPUBVHFlag::PREFER_FAST_TRACE;
        tdesc.max_instances = 1;
        cornell_tlas        = current_device.create_tlas(tdesc);

        auto blas_sizes = current_device.get_blas_sizes(cornell_blas);
        auto tlas_sizes = current_device.get_tlas_sizes(cornell_tlas);
        scratch_size    = std::max(blas_sizes.build_size, tlas_sizes.build_size);

        GPUBufferDescriptor sdesc{};
        sdesc.label           = "cornell_scratch";
        sdesc.size            = scratch_size > 0 ? scratch_size : 65536;
        sdesc.usage           = GPUBufferUsage::STORAGE;
        sdesc.virtual_address = true;
        cornell_scratch       = current_device.create_buffer(sdesc);
    }

    void build_as()
    {
        auto& current_device = RHI::get_current_device();
        auto  cmd            = execute([&]() {
            GPUCommandBufferDescriptor cdesc{};
            cdesc.queue = GPUQueueType::DEFAULT;
            return current_device.create_command_buffer(cdesc);
        });

        // Build BLAS
        GPUBlasTriangleGeometry tri{};
        tri.size.flags         = GPUBVHGeometryFlag::BVH_OPAQUE;
        tri.size.vertex_format = GPUVertexFormat::FLOAT32x3;
        tri.size.vertex_count  = static_cast<uint>(cornell_vertices.size());
        tri.size.index_count   = static_cast<uint>(cornell_indices.size());
        tri.size.index_format  = GPUIndexFormat::UINT32;
        tri.vertex_buffer      = cornell_vbuffer;
        tri.vertex_stride      = sizeof(Vertex);
        tri.first_vertex       = 0;
        tri.index_buffer       = cornell_ibuffer;
        tri.first_index        = 0;

        GPUBlasBuildEntry b_entry{};
        b_entry.blas                 = cornell_blas;
        b_entry.geometries.type      = GPUBlasType::TRIANGLE;
        b_entry.geometries.triangles = tri;
        cmd.build_blases(cornell_scratch, b_entry);

        // Barrier for BLAS completion
        GPUBufferBarrier barrier{};
        barrier.buffer     = cornell_scratch;
        barrier.size       = scratch_size;
        barrier.src_sync   = GPUBarrierSync::ACCELERATION_STRUCTURE_BUILD;
        barrier.dst_sync   = GPUBarrierSync::ACCELERATION_STRUCTURE_BUILD;
        barrier.src_access = GPUBarrierAccess::ACCELERATION_STRUCTURE_WRITE;
        barrier.dst_access = GPUBarrierAccess::ACCELERATION_STRUCTURE_READ;
        cmd.resource_barrier(barrier);

        // Build TLAS
        GPUTlasInstance inst{};
        std::memset(inst.transform, 0, sizeof(inst.transform));
        inst.transform[0][0] = 1.0f;
        inst.transform[1][1] = 1.0f;
        inst.transform[2][2] = 1.0f;
        inst.mask            = 0xFF;
        inst.blas            = cornell_blas;

        Vector<GPUTlasInstance> insts;
        insts.push_back(inst);

        GPUTlasBuildEntry t_entry{};
        t_entry.tlas      = cornell_tlas;
        t_entry.instances = insts;
        cmd.build_tlases(cornell_scratch, t_entry);
        cmd.submit();
        current_device.wait();
    }

    void setup_renderer()
    {
        auto& current_device = RHI::get_current_device();

        REQUIRE(plugin.load("lyra-pathtracer"));
        auto api = plugin.get_api();
        REQUIRE(api != nullptr);

        RendererDescriptor rdesc{};
        rdesc.device         = current_device;
        rdesc.compiler       = *compiler;
        rdesc.initial_extent = {desc.width, desc.height};
        rdesc.color_format   = get_backbuffer_format();
        rdesc.depth_format   = GPUTextureFormat::DEPTH32FLOAT;
        REQUIRE(api->create_renderer(renderer_handle, rdesc));
        REQUIRE(renderer_handle.valid());

        RendererSettings settings{};
        api->get_settings(renderer_handle, settings);
        settings.max_ray_bounces   = 4;
        settings.samples_per_pixel = 1;
        settings.exposure          = 1.0f;
        api->set_settings(renderer_handle, settings);

        arena.init(current_device);
    }

    explicit PathTracerApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        if (skipped) return;
        setup_buffers();
        setup_as();
        build_as();
        setup_renderer();
    }

    ~PathTracerApp() override
    {
        auto& current_device = RHI::get_current_device();
        if (current_device.valid()) {
            current_device.wait();
            if (renderer_handle.valid() && plugin.get_api()) {
                plugin.get_api()->delete_renderer(renderer_handle);
            }
            if (cornell_scratch.valid()) cornell_scratch.destroy();
            if (cornell_tlas.valid()) cornell_tlas.destroy();
            if (cornell_blas.valid()) cornell_blas.destroy();
            if (cornell_ibuffer.valid()) cornell_ibuffer.destroy();
            if (cornell_vbuffer.valid()) cornell_vbuffer.destroy();
            arena.destroy();
        }
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto api = plugin.get_api();
        REQUIRE(api != nullptr);

        const uint accumulation_samples = 16;
        for (uint i = 0; i < accumulation_samples; ++i) {
            auto cmd = execute([&]() {
                GPUCommandBufferDescriptor cdesc{};
                cdesc.queue = GPUQueueType::DEFAULT;
                return device.create_command_buffer(cdesc);
            });

            FrameGraphBuilder builder;

            FrameGraphTexture backbuffer_tex{};
            backbuffer_tex.texture = backbuffer.texture;
            backbuffer_tex.view    = backbuffer.view;
            backbuffer_tex.format  = get_backbuffer_format();
            backbuffer_tex.state   = undefined_texture_state();
            auto color_target      = builder.import(backbuffer_tex);

            RenderScene scene{};
            scene.scene_tlas = cornell_tlas;

            RenderFrameOutputs outputs{};
            RenderFrameContext ctx{
                .builder        = builder,
                .view           = view,
                .scene          = scene,
                .view_uniforms  = nullptr,
                .scene_lighting = nullptr,
                .color_target   = color_target,
                .depth_target   = {},
                .color_format   = get_backbuffer_format(),
                .depth_format   = GPUTextureFormat::DEPTH32FLOAT,
                .render_extent  = {desc.width, desc.height},
                .display_extent = {desc.width, desc.height},
                .outputs        = &outputs,
            };

            auto final_color = api->render(renderer_handle, ctx);

            if (i == accumulation_samples - 1) {
                builder.add_pass(
                    "present_pass",
                    [&](FrameGraph::PassBuilder& pass) {
                        (void)pass.read(final_color, FrameGraphReadOp::NOP);
                        pass.preserve();
                    },
                    [this, final_color](FrameGraph::Resources& res, FrameGraphContext* fg_ctx) {
                        auto tex = res.get<FrameGraph::Texture>(final_color);
                        postprocessing_compute(fg_ctx->cmdlist, tex->texture);
                    });
            }

            auto graph  = builder.build();
            auto fg_ctx = FrameGraphContext{device, swp, cmd, &arena, nullptr, nullptr};
            graph->execute(&fg_ctx, &fg_allocator);
            cmd.submit();
            device.wait();

            arena.tick();
        }
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::path_tracer" * doctest::description("Reference Path Tracer Cornell Box"))
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
    desc.features       = {GPUFeatureName::RAYTRACING};
    PathTracerApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::path_tracer" * doctest::description("Reference Path Tracer Cornell Box"))
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
    desc.features       = {GPUFeatureName::RAYTRACING};
    PathTracerApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::path_tracer" * doctest::description("Reference Path Tracer Cornell Box"))
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
    desc.features       = {GPUFeatureName::RAYTRACING};
    PathTracerApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::path_tracer" * doctest::description("Reference Path Tracer Cornell Box"))
{
    TestAppDescriptor desc{};
    desc.name           = "softgpu";
    desc.window         = false;
    desc.backend        = RHIBackend::SOFTGPU;
    desc.width          = 640;
    desc.height         = 480;
    desc.rhi_flags      = RHIFlag::DEBUG;
    desc.compile_target = CompileTarget::SPIRV;
    desc.compile_flags  = CompileFlag::DEBUG;
    desc.features       = {GPUFeatureName::RAYTRACING};
    PathTracerApp(desc).run();
}
