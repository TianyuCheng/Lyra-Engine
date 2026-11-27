#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Rendering/Passes/OceanPass.h>
#include <spdlog/spdlog.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    struct alignas(16) OceanUniform
    {
        Matrix4x4     proj;
        Matrix4x4     view;
        Matrix4x4     inv_proj;
        Matrix4x4     inv_view;
        Vector4       camera_pos_and_time;
        Vector4       sun_dir_and_intensity;
        Vector4       sun_color_and_foam_scale;
        Vector4       water_params;
        Vector4       deep_color;
        Vector4       shallow_color;
        OceanWaveData waves[4];
    };

    struct GridVertex
    {
        Vector3 pos;
        Vector2 uv;
    };
} // namespace

bool OceanPass::init(
    GPUDevice        device,
    Compiler         compiler,
    GPUTextureFormat color_format,
    GPUTextureFormat depth_format,
    uint             grid_subdivs_x,
    uint             grid_subdivs_z,
    float            grid_width,
    float            grid_depth)
{
    if (initialized) {
        if (cached_color_format == color_format && cached_depth_format == depth_format) {
            return true;
        }
        destroy();
    }

    this->device              = device;
    this->cached_color_format = color_format;
    this->cached_depth_format = depth_format;

    pipeline_manager.init(device, compiler);

    // compile and reflect ocean pass shaders
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module         = "sys://Shaders/Water/OceanPass.slang";
    shader_desc.vertex_entry   = "vsmain";
    shader_desc.fragment_entry = "fsmain";

    auto layout_res = pipeline_manager.reflect_render_layout(shader_desc);
    if (!layout_res) {
        spdlog::error("[OceanPass] failed to reflect ocean pass shaders: {}", layout_res.error());
        return false;
    }
    const auto& layout_info = *layout_res;
    blayouts.assign(layout_info.bind_group_layouts.begin(), layout_info.bind_group_layouts.end());

    // generate ocean planar tessellation grid
    Vector<GridVertex> vertices;
    Vector<uint32_t>   indices;

    float half_w   = grid_width * 0.5f;
    float half_d   = grid_depth * 0.5f;
    uint  vx_count = grid_subdivs_x + 1;
    uint  vz_count = grid_subdivs_z + 1;

    vertices.reserve(vx_count * vz_count);
    for (uint z = 0; z < vz_count; ++z) {
        float fz = static_cast<float>(z) / static_cast<float>(grid_subdivs_z);
        float pz = -half_d + fz * grid_depth;

        for (uint x = 0; x < vx_count; ++x) {
            float      fx = static_cast<float>(x) / static_cast<float>(grid_subdivs_x);
            float      px = -half_w + fx * grid_width;
            GridVertex v;
            v.pos = Vector3(px, 0.0f, pz);
            v.uv  = Vector2(fx, fz);
            vertices.push_back(v);
        }
    }

    indices.reserve(grid_subdivs_x * grid_subdivs_z * 6);
    for (uint z = 0; z < grid_subdivs_z; ++z) {
        for (uint x = 0; x < grid_subdivs_x; ++x) {
            uint32_t row1 = z * vx_count;
            uint32_t row2 = (z + 1) * vx_count;

            indices.push_back(row1 + x);
            indices.push_back(row2 + x);
            indices.push_back(row2 + x + 1);

            indices.push_back(row1 + x);
            indices.push_back(row2 + x + 1);
            indices.push_back(row1 + x + 1);
        }
    }

    grid_index_count = static_cast<uint>(indices.size());

    // allocate and upload grid vertex buffer
    auto vb_desc               = GPUBufferDescriptor{};
    vb_desc.label              = "ocean_grid_vbuffer";
    vb_desc.size               = vertices.size() * sizeof(GridVertex);
    vb_desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
    vb_desc.mapped_at_creation = true;
    grid_vbuffer               = device.create_buffer(vb_desc);
    if (grid_vbuffer.valid()) {
        auto mapped = grid_vbuffer.get_mapped_range();
        if (mapped.data) {
            std::memcpy(mapped.data, vertices.data(), vb_desc.size);
        }
        grid_vbuffer.unmap();
    }

    // allocate and upload grid index buffer
    auto ib_desc               = GPUBufferDescriptor{};
    ib_desc.label              = "ocean_grid_ibuffer";
    ib_desc.size               = indices.size() * sizeof(uint32_t);
    ib_desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
    ib_desc.mapped_at_creation = true;
    grid_ibuffer               = device.create_buffer(ib_desc);
    if (grid_ibuffer.valid()) {
        auto mapped = grid_ibuffer.get_mapped_range();
        if (mapped.data) {
            std::memcpy(mapped.data, indices.data(), ib_desc.size);
        }
        grid_ibuffer.unmap();
    }

    // construct pipeline vertex attribute layout matching GridVertex
    Array<GPUVertexAttribute, 2> attrs{};
    attrs[0] = {GPUVertexFormat::FLOAT32x3, 0, 0, "POSITION"};
    attrs[1] = {GPUVertexFormat::FLOAT32x2, 12, 1, "TEXCOORD0"};

    GPUVertexBufferLayout vlayout{};
    vlayout.array_stride = sizeof(GridVertex);
    vlayout.step_mode    = GPUVertexStepMode::VERTEX;
    vlayout.attributes   = attrs;

    Vector<GPUVertexBufferLayout> vlayouts;
    vlayouts.push_back(vlayout);

    GPUColorTargetState color_state{};
    color_state.format       = color_format;
    color_state.write_mask   = GPUColorWrite::ALL;
    color_state.blend_enable = false;
    Vector<GPUColorTargetState> rstates;
    rstates.push_back(color_state);

    auto pipe_res = pipeline_manager.render_pipeline_builder()
                        .with_shader(shader_desc)
                        .with_vertex_layouts(vlayouts)
                        .with_cull_mode(GPUCullMode::NONE)
                        .with_topology(GPUPrimitiveTopology::TRIANGLE_LIST)
                        .with_front_face(GPUFrontFace::CCW)
                        .with_target(color_format)
                        .with_depth_stencil(depth_format, true, GPUCompareFunction::LESS_EQUAL)
                        .with_label("ocean_pipeline")
                        .build();
    if (!pipe_res) {
        spdlog::error("[OceanPass] failed to create ocean render pipeline: {}", pipe_res.error());
        return false;
    }
    pipeline = *pipe_res;

    initialized = pipeline.valid();
    return initialized;
}

void OceanPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (grid_vbuffer.valid()) grid_vbuffer.destroy();
    if (grid_ibuffer.valid()) grid_ibuffer.destroy();
    pipeline_manager.destroy();
    pipeline = {};

    blayouts.clear();
    initialized = false;
}

auto OceanPass::add(FrameGraphBuilder& builder, const OceanPassParams& params) -> OceanPassData
{
    return builder.add_pass<OceanPassData>(
        "OceanPass",
        [&](OceanPassData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_output = pass_builder.render(params.color_target);
            data.depth_output = pass_builder.render(params.depth_target);
        },
        [this, params, extent = params.view.render_extent](
            const OceanPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid() || blayouts.empty()) {
                spdlog::warn("[OceanPass] SKIPPED in execute! ctx={} valid={} blayouts={}", (void*)ctx, pipeline.valid(), blayouts.size());
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_output);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_output);
            if (!color_tex || !depth_tex) {
                spdlog::warn("[OceanPass] color_tex or depth_tex is null!");
                return;
            }
            spdlog::info("[OceanPass] EXECUTING pass! extent={}x{} indices={}", extent.width, extent.height, grid_index_count);

            // prepare ocean uniform
            OceanUniform u{};
            u.proj                = params.view.projection;
            u.view                = params.view.view;
            u.inv_proj            = params.view.inv_projection;
            u.inv_view            = params.view.inv_view;
            u.camera_pos_and_time = Vector4(params.view.camera_position, params.time);

            Vector3 sun_dir(0.25f, 0.65f, -0.70f);
            Vector3 sun_col(1.0f, 0.98f, 0.92f);
            float   sun_int = 40000.0f;
            if (!params.scene.directional_lights.empty()) {
                const auto& dir_light = params.scene.directional_lights.front();
                sun_dir               = -glm::normalize(dir_light.direction);
                sun_col               = dir_light.color;
                sun_int               = dir_light.intensity;
            }

            // convert raw illuminance into exposed radiance, matching the skybox pass
            float exposure_scale       = (params.view.exposure_scale > 0.0f) ? params.view.exposure_scale : (1.0f / (1.2f * 8192.0f));
            float sun_radiance         = glm::clamp(sun_int * exposure_scale * 1.8f, 5.0f, 40.0f);
            u.sun_dir_and_intensity    = Vector4(sun_dir, sun_radiance);
            u.sun_color_and_foam_scale = Vector4(sun_col, params.foam_scale);
            u.water_params             = Vector4(params.water_depth, params.water_clarity, params.wave_speed, params.roughness);
            u.deep_color               = params.deep_color;
            u.shallow_color            = params.shallow_color;

            for (uint i = 0; i < 4; ++i) {
                u.waves[i] = params.waves[i];
            }

            auto u_slice = ctx->create_uniform(u);

            Array<GPUBindGroupEntry, 1> entries{};
            entries.at(0).type    = GPUResourceType::BUFFER;
            entries.at(0).binding = 0;
            entries.at(0).buffer  = u_slice;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.layout  = blayouts.at(0);
            bg_desc.entries = entries;
            auto bg         = ctx->allocate(bg_desc);

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = color_tex->view;
            color_attachment.clear_value = GPUColor{params.scene.clear_color.r, params.scene.clear_color.g, params.scene.clear_color.b, params.scene.clear_color.a};
            color_attachment.load_op     = params.color_load_op;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = params.depth_load_op;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;
            depth_attachment.depth_read_only   = false;

            auto render_pass                     = GPURenderPassDescriptor{};
            render_pass.color_attachments        = color_attachment;
            render_pass.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(render_pass);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bg);
            command.set_vertex_buffer(0, grid_vbuffer);
            command.set_index_buffer(grid_ibuffer, GPUIndexFormat::UINT32, 0);
            command.draw_indexed(grid_index_count, 1, 0, 0, 0);
            command.end_render_pass();
        });
}
