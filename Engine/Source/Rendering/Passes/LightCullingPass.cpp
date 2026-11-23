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

    // 1. compile compute shader from sys://Shaders/Forward/LightCulling.slang
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "light_culling";
        desc.path   = "sys://Shaders/Forward/LightCulling.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

    auto reflection = compiler.reflect({
        {*module, "csmain"},
    });

    if (!reflection) {
        return false;
    }

    cshader = execute([&]() {
        auto code  = module->get_shader_blob("csmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "light_culling_compute_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    if (!cshader.valid()) {
        return false;
    }

    // 2. bind group layouts and pipeline layout from reflection
    blayouts.clear();
    for (auto& desc : reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        blayouts.push_back(blayout);
    }

    playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = blayouts;
        desc.immediate_size     = reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    // 3. compute pipeline
    auto cp_desc                = GPUComputePipelineDescriptor{};
    cp_desc.label               = "light_culling_pipeline";
    cp_desc.layout              = playout;
    cp_desc.compute.module      = cshader;
    cp_desc.compute.entry_point = "csmain";
    pipeline                    = device.create_compute_pipeline(cp_desc);

    // 4. bind group heap
    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 512;
        return device.create_bind_group_heap(desc);
    });

    // 5. lights staging buffer
    lights_buffer = execute([&]() {
        auto lb_desc               = GPUBufferDescriptor{};
        lb_desc.label              = "light_culling_lights_buffer";
        lb_desc.size               = std::max<usize>(sizeof(GPULightData), 256 * sizeof(GPULightData));
        lb_desc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
        lb_desc.mapped_at_creation = true;
        return device.create_buffer(lb_desc);
    });

    initialized = pipeline.valid();
    return initialized;
}

void LightCullingPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) {
        heap.destroy();
    }
    if (lights_buffer.valid()) {
        lights_buffer.destroy();
    }
    if (pipeline.valid()) {
        pipeline.destroy();
    }
    if (playout.valid()) {
        playout.destroy();
    }
    if (cshader.valid()) {
        cshader.destroy();
    }
    blayouts.clear();
    initialized = false;
}

LightCullingData LightCullingPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene)
{
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
            grid_desc.size           = total_clusters * 2 * sizeof(uint32_t); // uint2 (offset, count)
            grid_desc.usage          = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST;
            data.cluster_grid_buffer = pass_builder.create<FrameGraphBuffer>(grid_desc);

            auto index_desc       = GPUBufferDescriptor{};
            index_desc.label      = "cluster_light_index_list";
            index_desc.size       = total_clusters * 64 * sizeof(uint32_t); // max 64 lights per cluster
            index_desc.usage      = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST;
            data.light_index_list = pass_builder.create<FrameGraphBuffer>(index_desc);

            data.cluster_grid_buffer = pass_builder.write(data.cluster_grid_buffer);
            data.light_index_list    = pass_builder.write(data.light_index_list);
        },
        [this, &view, &scene, cluster_grid_x, cluster_grid_y, cluster_grid_z](const LightCullingData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid() || blayouts.empty()) {
                return;
            }

            auto grid_buf  = resources.get<FrameGraphBuffer>(data.cluster_grid_buffer);
            auto index_buf = resources.get<FrameGraphBuffer>(data.light_index_list);
            if (!grid_buf || !index_buf) {
                return;
            }

            // gather lights from scene
            Vector<GPULightData> gathered_lights;
            gathered_lights.reserve(scene.directional_lights.size() + scene.local_lights.size());

            for (const auto& dl : scene.directional_lights) {
                GPULightData l{};
                l.position     = dl.position;
                l.range        = dl.range;
                l.direction    = dl.direction;
                l.intensity    = dl.intensity;
                l.color        = dl.color;
                l.type         = static_cast<uint>(RenderLightType::DIRECTIONAL);
                l.cast_shadows = dl.cast_shadows ? 1 : 0;
                gathered_lights.push_back(l);
            }

            for (const auto& ll : scene.local_lights) {
                GPULightData l{};
                l.position         = ll.position;
                l.range            = ll.range;
                l.direction        = ll.direction;
                l.intensity        = ll.intensity;
                l.color            = ll.color;
                l.type             = static_cast<uint>(ll.type);
                l.spot_inner_angle = ll.spot_inner_angle;
                l.spot_outer_angle = ll.spot_outer_angle;
                l.cast_shadows     = ll.cast_shadows ? 1 : 0;
                gathered_lights.push_back(l);
            }

            // update staging lights buffer
            usize required_size = std::max<usize>(sizeof(GPULightData), gathered_lights.size() * sizeof(GPULightData));
            if (!lights_buffer.valid() || lights_buffer.size < required_size) {
                if (lights_buffer.valid()) {
                    lights_buffer.destroy();
                }
                auto lb_desc               = GPUBufferDescriptor{};
                lb_desc.label              = "light_culling_lights_buffer";
                lb_desc.size               = required_size;
                lb_desc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::MAP_WRITE;
                lb_desc.mapped_at_creation = true;
                lights_buffer              = device.create_buffer(lb_desc);
            }

            if (lights_buffer.valid() && !gathered_lights.empty()) {
                auto mapped = lights_buffer.get_mapped_range<GPULightData>();
                for (usize i = 0; i < gathered_lights.size() && i < mapped.count; ++i) {
                    mapped.at(i) = gathered_lights[i];
                }
            }

            // create bind group
            Array<GPUBindGroupEntry, 3> entries{};
            entries[0].type          = GPUResourceType::BUFFER;
            entries[0].binding       = 0;
            entries[0].buffer.buffer = lights_buffer.handle;
            entries[0].buffer.offset = 0;
            entries[0].buffer.size   = 0;

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
            bg_desc.heap    = heap;
            bg_desc.layout  = blayouts[0];
            bg_desc.entries = entries;
            auto bg         = device.create_bind_group(bg_desc);

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
            push.num_lights        = static_cast<uint>(gathered_lights.size());

            auto command = ctx->cmdlist;
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bg);
            command.set_immediates(0, push);
            command.dispatch_workgroups((cluster_grid_x + 15) / 16, (cluster_grid_y + 15) / 16, cluster_grid_z);
        });
}
