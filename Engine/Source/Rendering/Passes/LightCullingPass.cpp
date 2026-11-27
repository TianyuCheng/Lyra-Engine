#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/LightCullingPass.h>

using namespace lyra;
using namespace lyra::passes;

bool LightCullingPass::init(GPUDevice dev, Compiler compiler)
{
    if (initialized) {
        return true;
    }

    device = dev;
    pipeline_manager.init(device, compiler);

    // 1. reflect compute layout from sys://Shaders/Forward/LightCulling.slang
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module        = "sys://Shaders/Forward/LightCulling.slang";
    shader_desc.compute_entry = "csmain";

    auto layout = pipeline_manager.reflect_compute_layout(shader_desc);
    if (!layout) {
        return false;
    }
    blayouts.assign(layout->bind_group_layouts.begin(), layout->bind_group_layouts.end());

    auto pipe = pipeline_manager.compute_pipeline_builder()
                        .with_shader(shader_desc)
                        .with_label("light_culling_pipeline")
                        .build();
    if (!pipe) {
        return false;
    }
    pipeline = *pipe;

    initialized = pipeline.valid();
    return initialized;
}

void LightCullingPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    pipeline_manager.destroy();
    pipeline = {};
    blayouts.clear();
    lights_slice = {};
    initialized  = false;
}

LightCullingData LightCullingPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    GPUBufferSlice     in_lights_slice)
{
    lights_slice              = in_lights_slice;
    const uint cluster_grid_x = std::max(1u, (view.render_extent.width + 15) / 16);
    const uint cluster_grid_y = std::max(1u, (view.render_extent.height + 15) / 16);
    const uint cluster_grid_z = 24;
    const uint total_clusters = cluster_grid_x * cluster_grid_y * cluster_grid_z;

    return builder.add_pass<LightCullingData>(
        "LightCullingPass",
        [&, cluster_grid_x, cluster_grid_y, cluster_grid_z, total_clusters](LightCullingData& data, FrameGraphPassBuilder& pass_builder) {
            data.cluster_grid_x = cluster_grid_x;
            data.cluster_grid_y = cluster_grid_y;
            data.cluster_grid_z = cluster_grid_z;

            auto grid_desc           = GPUBufferDescriptor{};
            grid_desc.label          = "cluster_grid_buffer";
            grid_desc.size           = total_clusters * 2 * sizeof(uint); // uint2 (offset, count)
            grid_desc.usage          = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST | GPUBufferUsage::MAP_WRITE;
            data.cluster_grid_buffer = pass_builder.create<FrameGraphBuffer>(grid_desc);

            auto index_desc       = GPUBufferDescriptor{};
            index_desc.label      = "cluster_light_index_list";
            index_desc.size       = total_clusters * 64 * sizeof(uint); // max 64 lights per cluster
            index_desc.usage      = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST | GPUBufferUsage::MAP_WRITE;
            data.light_index_list = pass_builder.create<FrameGraphBuffer>(index_desc);

            data.cluster_grid_buffer = pass_builder.write(data.cluster_grid_buffer);
            data.light_index_list    = pass_builder.write(data.light_index_list);
        },
        [this, view, &scene, cluster_grid_x, cluster_grid_y, cluster_grid_z](const LightCullingData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid() || blayouts.empty()) {
                return;
            }

            auto grid_buf  = resources.get<FrameGraphBuffer>(data.cluster_grid_buffer);
            auto index_buf = resources.get<FrameGraphBuffer>(data.light_index_list);
            if (!grid_buf || !index_buf) {
                return;
            }

            auto active_slice = lights_slice;
            if (!active_slice.valid() && ctx->scene_lighting && ctx->scene_lighting->lights_slice.valid()) {
                active_slice = ctx->scene_lighting->lights_slice;
            }
            if (!active_slice.valid() && ctx->arena) {
                SceneLighting sl{};
                sl.init(scene, *ctx->arena, ctx->device);
                active_slice = sl.lights_slice;
            }
            lights_slice = active_slice;

            // create bind group
            Array<GPUBindGroupEntry, 3> entries{};
            entries[0].type    = GPUResourceType::BUFFER;
            entries[0].binding = 0;
            entries[0].buffer  = active_slice;

            entries[1].type          = GPUResourceType::BUFFER;
            entries[1].binding       = 1;
            entries[1].buffer.buffer = grid_buf->buffer;
            entries[1].buffer.offset = 0;
            entries[1].buffer.size   = 0;

            entries[2].type          = GPUResourceType::BUFFER;
            entries[2].binding       = 2;
            entries[2].buffer.buffer = index_buf->buffer;
            entries[2].buffer.offset = 0;
            entries[2].buffer.size   = 0;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.layout  = blayouts[0];
            bg_desc.entries = entries;
            auto bg         = ctx->allocate(bg_desc);

            LightCullingPushConstants push{};
            push.view_matrix       = view.view;
            push.proj_matrix       = view.projection;
            push.inv_proj_matrix   = view.inv_projection;
            push.screen_resolution = Vector2(static_cast<float>(view.render_extent.width), static_cast<float>(view.render_extent.height));
            push.near_plane        = view.near_plane > 0.0f ? view.near_plane : 0.1f;
            push.far_plane         = view.far_plane > push.near_plane ? view.far_plane : 100.0f;
            push.cluster_grid_x    = cluster_grid_x;
            push.cluster_grid_y    = cluster_grid_y;
            push.cluster_grid_z    = cluster_grid_z;
            push.num_lights        = (ctx->scene_lighting) ? ctx->scene_lighting->total_light_count : static_cast<uint>(scene.directional_lights.size() + scene.local_lights.size());

            auto command = ctx->cmdlist;
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bg);
            command.set_immediates(0, push);
            command.dispatch_workgroups((cluster_grid_x + 31) / 32, (cluster_grid_y + 31) / 32, cluster_grid_z);
        });
}
