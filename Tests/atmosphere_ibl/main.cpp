#include "helper.h"
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/RenderWorld.h>
#include <Lyra/Rendering/ProceduralMesh.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Sun.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/Transform.h>
#include <cmath>
#include <algorithm>

using namespace lyra;

static constexpr float PI = 3.14159265358979323846f;

// evaluate procedural sky radiance in direction dir
static Vector3 evaluate_procedural_sky(const Vector3& dir, float roughness = 0.0f)
{
    // sun direction and color
    const Vector3 sun_dir   = glm::normalize(Vector3(0.35f, 0.6f, 0.65f));
    const Vector3 sun_color = Vector3(10.0f, 9.5f, 8.5f);

    // zenith to horizon gradient
    Vector3 sky_color;
    if (dir.y >= 0.0f) {
        float t   = std::pow(std::clamp(dir.y, 0.0f, 1.0f), 0.6f);
        sky_color = glm::mix(Vector3(0.72f, 0.78f, 0.88f), Vector3(0.18f, 0.42f, 0.88f), t);
    } else {
        float t   = std::clamp(-dir.y * 2.0f, 0.0f, 1.0f);
        sky_color = glm::mix(Vector3(0.72f, 0.78f, 0.88f), Vector3(0.18f, 0.14f, 0.12f), t);
    }

    // sun specular lobe depending on roughness
    float cos_theta = std::max(glm::dot(dir, sun_dir), 0.0f);
    if (roughness <= 1e-3f) {
        if (cos_theta > 0.998f) {
            sky_color += sun_color * 3.0f;
        }
    } else {
        float exponent = std::max(2.0f, (1.0f - roughness) * 200.0f);
        float lobe     = std::pow(cos_theta, exponent);
        sky_color += sun_color * lobe * (1.0f - roughness * 0.5f);
    }

    // diffuse hemisphere blend for higher roughness
    Vector3 avg_sky = Vector3(0.45f, 0.52f, 0.68f);
    return glm::mix(sky_color, avg_sky, roughness * 0.45f);
}

// compute sh9 diffuse irradiance coefficients from the procedural sky
static void compute_procedural_sh9(Array<Vector4, 9>& out_sh)
{
    out_sh.fill(Vector4(0.0f));

    Array<Vector3, 9> accum{};
    accum.fill(Vector3(0.0f));

    constexpr int SAMPLES_LAT = 64;
    constexpr int SAMPLES_LON = 128;

    for (int y = 0; y < SAMPLES_LAT; ++y) {
        float theta   = (static_cast<float>(y) + 0.5f) / static_cast<float>(SAMPLES_LAT) * PI - (PI * 0.5f);
        float d_omega = std::cos(theta) * (2.0f * PI / static_cast<float>(SAMPLES_LON)) * (PI / static_cast<float>(SAMPLES_LAT));

        for (int x = 0; x < SAMPLES_LON; ++x) {
            float phi = (static_cast<float>(x) + 0.5f) / static_cast<float>(SAMPLES_LON) * 2.0f * PI - PI;

            Vector3 d(std::cos(theta) * std::cos(phi), std::sin(theta), std::cos(theta) * std::sin(phi));
            d = glm::normalize(d);

            Vector3 radiance = evaluate_procedural_sky(d, 0.0f);

            float sh[9];
            sh[0] = 0.28209479f;
            sh[1] = 0.48860251f * d.y;
            sh[2] = 0.48860251f * d.z;
            sh[3] = 0.48860251f * d.x;
            sh[4] = 1.09254843f * (d.x * d.y);
            sh[5] = 1.09254843f * (d.y * d.z);
            sh[6] = 0.31539156f * (3.0f * d.z * d.z - 1.0f);
            sh[7] = 1.09254843f * (d.x * d.z);
            sh[8] = 0.54627421f * (d.x * d.x - d.y * d.y);

            for (int i = 0; i < 9; ++i) {
                accum[i] += radiance * (sh[i] * d_omega);
            }
        }
    }

    constexpr float A0 = PI;
    constexpr float A1 = (2.0f * PI) / 3.0f;
    constexpr float A2 = PI / 4.0f;

    out_sh[0] = Vector4(accum[0] * A0, 1.0f);
    out_sh[1] = Vector4(accum[1] * A1, 1.0f);
    out_sh[2] = Vector4(accum[2] * A1, 1.0f);
    out_sh[3] = Vector4(accum[3] * A1, 1.0f);
    out_sh[4] = Vector4(accum[4] * A2, 1.0f);
    out_sh[5] = Vector4(accum[5] * A2, 1.0f);
    out_sh[6] = Vector4(accum[6] * A2, 1.0f);
    out_sh[7] = Vector4(accum[7] * A2, 1.0f);
    out_sh[8] = Vector4(accum[8] * A2, 1.0f);
}

// direction vector for cube face f in [0..5]
static Vector3 cube_face_direction(int face, float u, float v)
{
    Vector3 dir(0.0f);
    // clang-format off
    switch (face) {
        case 0: dir = Vector3(1.0f, -v, -u); break;  // +X
        case 1: dir = Vector3(-1.0f, -v, u); break;  // -X
        case 2: dir = Vector3(u, 1.0f, v); break;   // +Y
        case 3: dir = Vector3(u, -1.0f, -v); break;  // -Y
        case 4: dir = Vector3(u, -v, 1.0f); break;   // +Z
        case 5: dir = Vector3(-u, -v, -1.0f); break; // -Z
        default: break;
    }
    // clang-format on
    return glm::normalize(dir);
}

struct AtmosphereIBLApp : public TestApp
{
    RendererPlugin        plugin;
    RendererHandle        renderer_handle;
    ResourceRetire        retire;
    Own<GeometryCache>    geo_cache;
    Own<RenderWorld>      render_world;
    World                 ecs_world;
    FrameGraph::Allocator fg_allocator;

    GPUTexture     env_cubemap_texture;
    GPUTextureView env_cubemap_view;

    explicit AtmosphereIBLApp(const TestAppDescriptor& desc) : TestApp(desc)
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

        create_environment_cubemap(device);
        setup_scene();
    }

    ~AtmosphereIBLApp()
    {
        auto& device = RHI::get_current_device();
        device.wait();

        if (env_cubemap_view.valid()) {
            env_cubemap_view.destroy();
        }
        if (env_cubemap_texture.valid()) {
            env_cubemap_texture.destroy();
        }
        if (renderer_handle.valid()) {
            plugin.get_api()->delete_renderer(renderer_handle);
        }
    }

    void create_environment_cubemap(GPUDevice device)
    {
        constexpr uint BASE_SIZE = 64;
        constexpr uint NUM_MIPS  = 6;

        auto tdesc            = GPUTextureDescriptor{};
        tdesc.label           = "atmosphere_ibl_cubemap";
        tdesc.size.width      = BASE_SIZE;
        tdesc.size.height     = BASE_SIZE;
        tdesc.size.depth      = 1;
        tdesc.dimension       = GPUTextureDimension::x2D;
        tdesc.array_layers    = 6;
        tdesc.format          = GPUTextureFormat::RGBA32FLOAT;
        tdesc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        tdesc.mip_level_count = NUM_MIPS;
        tdesc.sample_count    = 1;

        env_cubemap_texture = device.create_texture(tdesc);

        auto vdesc              = GPUTextureViewDescriptor{};
        vdesc.aspect            = GPUTextureAspect::COLOR;
        vdesc.dimension         = GPUTextureViewDimension::CUBE;
        vdesc.format            = GPUTextureFormat::RGBA32FLOAT;
        vdesc.base_array_layer  = 0;
        vdesc.array_layer_count = 6;
        vdesc.base_mip_level    = 0;
        vdesc.mip_level_count   = NUM_MIPS;

        env_cubemap_view = env_cubemap_texture.create_view(vdesc);

        // calculate total staging buffer size
        struct CopyJob
        {
            uint mip;
            uint face;
            uint size;
            uint offset;
            uint bytes_per_row;
        };

        Vector<CopyJob> jobs;
        uint            total_staging_size = 0;

        for (uint mip = 0; mip < NUM_MIPS; ++mip) {
            uint face_size     = std::max(1u, BASE_SIZE >> mip);
            uint bytes_per_row = face_size * 4 * sizeof(float);
            uint face_bytes    = bytes_per_row * face_size;

            for (uint face = 0; face < 6; ++face) {
                // align offset to 256 bytes
                total_staging_size = (total_staging_size + 255u) & ~255u;
                jobs.push_back(CopyJob{
                    .mip           = mip,
                    .face          = face,
                    .size          = face_size,
                    .offset        = total_staging_size,
                    .bytes_per_row = bytes_per_row,
                });
                total_staging_size += face_bytes;
            }
        }

        auto sdesc               = GPUBufferDescriptor{};
        sdesc.label              = "atmosphere_ibl_staging";
        sdesc.size               = total_staging_size;
        sdesc.usage              = GPUBufferUsage::MAP_WRITE | GPUBufferUsage::COPY_SRC;
        sdesc.mapped_at_creation = true;

        auto staging_buffer = device.create_buffer(sdesc);
        auto mapped         = staging_buffer.get_mapped_range();
        auto dst_bytes      = static_cast<uint8_t*>(mapped.data);

        for (const auto& job : jobs) {
            float roughness = (NUM_MIPS > 1) ? static_cast<float>(job.mip) / static_cast<float>(NUM_MIPS - 1) : 0.0f;
            auto* pixels    = reinterpret_cast<float*>(dst_bytes + job.offset);

            for (uint y = 0; y < job.size; ++y) {
                float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(job.size) * 2.0f - 1.0f;
                for (uint x = 0; x < job.size; ++x) {
                    float   u       = (static_cast<float>(x) + 0.5f) / static_cast<float>(job.size) * 2.0f - 1.0f;
                    Vector3 d       = cube_face_direction(job.face, u, v);
                    Vector3 col     = evaluate_procedural_sky(d, roughness);
                    uint    idx     = (y * job.size + x) * 4;
                    pixels[idx + 0] = col.r;
                    pixels[idx + 1] = col.g;
                    pixels[idx + 2] = col.b;
                    pixels[idx + 3] = 1.0f;
                }
            }
        }
        staging_buffer.unmap();

        auto cmd_desc  = GPUCommandBufferDescriptor{};
        cmd_desc.label = "atmosphere_ibl_upload_cmd";
        cmd_desc.queue = GPUQueueType::DEFAULT;
        auto cmdbuf    = device.create_command_buffer(cmd_desc);

        cmdbuf.resource_barrier(state_transition(env_cubemap_texture, undefined_texture_state(), copy_dst_texture_state(), 0, 6, 0, NUM_MIPS));

        for (const auto& job : jobs) {
            GPUTexelCopyBufferInfo src{};
            src.buffer         = staging_buffer;
            src.offset         = job.offset;
            src.bytes_per_row  = job.bytes_per_row;
            src.rows_per_image = job.size;

            GPUTexelCopyTextureInfo dst{};
            dst.texture   = env_cubemap_texture;
            dst.aspect    = GPUTextureAspect::COLOR;
            dst.mip_level = job.mip;
            dst.origin    = {0, 0, job.face};

            GPUExtent3D extent{job.size, job.size, 1};
            cmdbuf.copy_buffer_to_texture(src, dst, extent);
        }

        cmdbuf.resource_barrier(state_transition(env_cubemap_texture, copy_dst_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING), 0, 6, 0, NUM_MIPS));
        cmdbuf.submit();
        device.wait();

        staging_buffer.destroy();
    }

    void setup_scene()
    {
        // 1. Camera looking at sphere matrix
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

        // 2. Physical Sun & Directional Light
        auto  sun_entity   = ecs_world.create("Sun");
        auto& dir          = ecs_world.add_component<DirectionalLight>(sun_entity);
        dir.direction      = glm::normalize(Vector3(0.35f, -0.6f, -0.65f));
        dir.color          = Vector3(1.0f, 0.98f, 0.94f);
        dir.intensity      = 50000.0f;
        auto& sun          = ecs_world.add_component<Sun>(sun_entity);
        sun.angular_radius = 0.004675f;
        sun.halo_size      = 12.0f;
        sun.halo_falloff   = 80.0f;
        sun.limb_darkening = 0.6f;

        // 3. SkyLight with Diffuse SH9 irradiance
        auto  sky_entity = ecs_world.create("SkyLight");
        auto& sky        = ecs_world.add_component<SkyLight>(sky_entity);
        sky.intensity    = 1.0f;
        sky.color        = Vector3(1.0f);
        compute_procedural_sh9(sky.sh9);

        // 4. 4x4 Sphere Matrix
        // Col roughness: 0.05, 0.35, 0.65, 0.95
        // Row 0: Dielectric (cyan) metallic = 0.0
        // Row 1: Semi-metal (copper tint) metallic = 0.33
        // Row 2: Heavy metal (silver) metallic = 0.67
        // Row 3: Pure metal (gold) metallic = 1.0
        static constexpr float ROUGHNESS_COLS[4] = {0.05f, 0.35f, 0.65f, 0.95f};
        static constexpr float METALLIC_ROWS[4]  = {0.0f, 0.33f, 0.67f, 1.0f};

        for (uint row = 0; row < 4; ++row) {
            for (uint col = 0; col < 4; ++col) {
                AssetID       mat_guid = 0x1B10000000000000ULL + (row * 4 + col);
                MaterialAsset mat{};

                mat.shading_model    = ShadingModel::STANDARD;
                mat.shading_model_id = BuiltinShadingModels::STANDARD;
                mat.roughness_factor = ROUGHNESS_COLS[col];
                mat.metallic_factor  = METALLIC_ROWS[row];

                if (row == 0) {
                    mat.base_color_factor = Vector4(0.12f, 0.45f, 0.85f, 1.0f);
                } else if (row == 1) {
                    mat.base_color_factor = Vector4(0.85f, 0.45f, 0.25f, 1.0f);
                } else if (row == 2) {
                    mat.base_color_factor = Vector4(0.92f, 0.92f, 0.95f, 1.0f);
                } else {
                    mat.base_color_factor = Vector4(1.0f, 0.78f, 0.34f, 1.0f);
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

        // bind our cooked environment cubemap to the render scene
        render_world->get_scene().environment_map = env_cubemap_view.handle;

        FrameGraphBuilder builder;

        FrameGraphTexture backbuffer_tex{};
        backbuffer_tex.texture = backbuffer.texture;
        backbuffer_tex.view    = backbuffer.view;
        backbuffer_tex.format  = get_backbuffer_format();
        backbuffer_tex.state   = undefined_texture_state();
        auto color_target      = builder.import(backbuffer_tex);

        RenderView view    = render_world->get_view();
        view.render_extent = {desc.width, desc.height};

        RenderFrameOutputs outputs{};
        RenderFrameContext ctx{
            .builder        = builder,
            .view           = view,
            .scene          = render_world->get_scene(),
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
        auto fg_ctx = FrameGraphContext{device, swp, command};
        graph->execute(&fg_ctx, &fg_allocator);
        command.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::atmosphere_ibl" * doctest::description("Atmosphere with Split-Sum IBL and sphere roughness/metallic matrix"))
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
    AtmosphereIBLApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::atmosphere_ibl" * doctest::description("Atmosphere with Split-Sum IBL and sphere roughness/metallic matrix"))
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
    AtmosphereIBLApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::atmosphere_ibl" * doctest::description("Atmosphere with Split-Sum IBL and sphere roughness/metallic matrix"))
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
    AtmosphereIBLApp(desc).run();
}
#endif

TEST_CASE("rhi::softgpu::atmosphere_ibl" * doctest::description("Atmosphere with Split-Sum IBL and sphere roughness/metallic matrix"))
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
    AtmosphereIBLApp(desc).run();
}
