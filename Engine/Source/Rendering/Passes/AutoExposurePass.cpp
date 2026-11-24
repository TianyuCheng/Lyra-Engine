#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/AutoExposurePass.h>

using namespace lyra;
using namespace lyra::passes;

bool AutoExposurePass::init(GPUDevice dev, Compiler compiler)
{
    if (initialized) {
        return true;
    }

    device = dev;

    // 1. compile compute shader module from sys://Shaders/PostProcess/AutoExposure.slang
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "auto_exposure";
        desc.path   = "sys://Shaders/PostProcess/AutoExposure.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

    // 2. reflect and create pipeline for cs_build_histogram
    auto hist_reflection = compiler.reflect({
        {*module, "cs_build_histogram"},
    });
    if (!hist_reflection) {
        return false;
    }

    histogram_shader = execute([&]() {
        auto code  = module->get_shader_blob("cs_build_histogram");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "auto_exposure_histogram_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });
    if (!histogram_shader.valid()) {
        return false;
    }

    histogram_blayouts.clear();
    for (auto& desc : hist_reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        histogram_blayouts.push_back(blayout);
    }

    histogram_playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = histogram_blayouts;
        desc.immediate_size     = hist_reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    histogram_pipeline = execute([&]() {
        auto cp_desc                = GPUComputePipelineDescriptor{};
        cp_desc.label               = "auto_exposure_histogram_pipeline";
        cp_desc.layout              = histogram_playout;
        cp_desc.compute.module      = histogram_shader;
        cp_desc.compute.entry_point = "cs_build_histogram";
        return device.create_compute_pipeline(cp_desc);
    });

    // 3. reflect and create pipeline for cs_adapt_exposure
    auto adapt_reflection = compiler.reflect({
        {*module, "cs_adapt_exposure"},
    });
    if (!adapt_reflection) {
        return false;
    }

    adapt_shader = execute([&]() {
        auto code  = module->get_shader_blob("cs_adapt_exposure");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "auto_exposure_adapt_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });
    if (!adapt_shader.valid()) {
        return false;
    }

    adapt_blayouts.clear();
    for (auto& desc : adapt_reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        adapt_blayouts.push_back(blayout);
    }

    adapt_playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = adapt_blayouts;
        desc.immediate_size     = adapt_reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    adapt_pipeline = execute([&]() {
        auto cp_desc                = GPUComputePipelineDescriptor{};
        cp_desc.label               = "auto_exposure_adapt_pipeline";
        cp_desc.layout              = adapt_playout;
        cp_desc.compute.module      = adapt_shader;
        cp_desc.compute.entry_point = "cs_adapt_exposure";
        return device.create_compute_pipeline(cp_desc);
    });

    // 4. allocate persistent 64-bin histogram buffer (256 bytes)
    histogram_buffer = execute([&]() {
        auto bdesc  = GPUBufferDescriptor{};
        bdesc.label = "auto_exposure_histogram_buffer";
        bdesc.size  = 64 * sizeof(uint);
        bdesc.usage = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST;
        return device.create_buffer(bdesc);
    });

    // 5. allocate persistent 1x1 exposure data texture { exposure_scale, ev100, avg_luminance, 1.0 }
    exposure_texture = execute([&]() {
        auto tdesc            = GPUTextureDescriptor{};
        tdesc.label           = "auto_exposure_texture";
        tdesc.size            = {1, 1, 1};
        tdesc.dimension       = GPUTextureDimension::x2D;
        tdesc.format          = GPUTextureFormat::RGBA16FLOAT;
        tdesc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        tdesc.mip_level_count = 1;
        return device.create_texture(tdesc);
    });
    exposure_view    = exposure_texture.create_view();

    execute([&]() {
        auto cmd_desc  = GPUCommandBufferDescriptor{};
        cmd_desc.label = "auto_exposure_tex_transition_cmd";
        cmd_desc.queue = GPUQueueType::DEFAULT;
        auto cmdbuf    = device.create_command_buffer(cmd_desc);
        cmdbuf.resource_barrier(state_transition(exposure_texture, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        cmdbuf.submit();
        device.wait();
    });

    // 6. create bind group heap
    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 512;
        return device.create_bind_group_heap(desc);
    });

    initialized = histogram_pipeline.valid() && adapt_pipeline.valid() &&
                  histogram_buffer.valid() && exposure_texture.valid();
    return initialized;
}

void AutoExposurePass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) heap.destroy();
    if (exposure_view.valid()) exposure_view.destroy();
    if (exposure_texture.valid()) exposure_texture.destroy();
    if (histogram_buffer.valid()) histogram_buffer.destroy();
    if (adapt_pipeline.valid()) adapt_pipeline.destroy();
    if (adapt_playout.valid()) adapt_playout.destroy();
    if (adapt_shader.valid()) adapt_shader.destroy();
    if (histogram_pipeline.valid()) histogram_pipeline.destroy();
    if (histogram_playout.valid()) histogram_playout.destroy();
    if (histogram_shader.valid()) histogram_shader.destroy();

    histogram_blayouts.clear();
    adapt_blayouts.clear();
    initialized = false;
}

AutoExposureData AutoExposurePass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> hdr_input,
    GPUExtent2D                         extent,
    float                               delta_time,
    const AutoExposureSettings&         settings)
{
    FrameGraphTexture imported_exp{};
    imported_exp.texture = exposure_texture;
    imported_exp.view    = exposure_view;
    imported_exp.format  = GPUTextureFormat::RGBA16FLOAT;
    auto exp_handle      = builder.import(imported_exp);

    return builder.add_pass<AutoExposureData>(
        "AutoExposurePass",
        [&](AutoExposureData& data, FrameGraphPassBuilder& pass_builder) {
            data.hdr_input     = pass_builder.sample(hdr_input);
            data.exposure_data = pass_builder.write(exp_handle);
        },
        [this, extent, delta_time, settings](const AutoExposureData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !valid()) {
                return;
            }

            auto hdr_tex = resources.get<FrameGraphTexture>(data.hdr_input);
            auto exp_tex = resources.get<FrameGraphTexture>(data.exposure_data);
            if (!hdr_tex || !exp_tex) {
                return;
            }

            auto command = ctx->cmdlist;

            AutoExposurePushConstants push{};
            push.min_ev        = settings.min_ev;
            push.max_ev        = settings.max_ev;
            push.speed_up      = settings.speed_up;
            push.speed_down    = settings.speed_down;
            push.low_percent   = settings.low_percent;
            push.high_percent  = settings.high_percent;
            push.delta_time    = delta_time;
            push.pixel_count   = extent.width * extent.height;
            push.screen_width  = extent.width;
            push.screen_height = extent.height;

            // =========================================================================
            // Stage 1: Build 64-Bin Luminance Histogram
            // =========================================================================
            command.set_pipeline(histogram_pipeline);

            Array<GPUBindGroupEntry, 2> hist_entries{};
            hist_entries[0].type    = GPUResourceType::TEXTURE;
            hist_entries[0].binding = 0;
            hist_entries[0].texture = hdr_tex->view;

            hist_entries[1].type          = GPUResourceType::BUFFER;
            hist_entries[1].binding       = 1;
            hist_entries[1].buffer.buffer = histogram_buffer.handle;
            hist_entries[1].buffer.offset = 0;
            hist_entries[1].buffer.size   = 64 * sizeof(uint);

            auto hist_bg_desc    = GPUBindGroupDescriptor{};
            hist_bg_desc.heap    = heap;
            hist_bg_desc.layout  = histogram_blayouts[0];
            hist_bg_desc.entries = hist_entries;
            auto hist_bg         = device.create_bind_group(hist_bg_desc);

            command.set_bind_group(0, hist_bg);
            command.set_immediates(0, push);
            command.dispatch_workgroups((extent.width + 15) / 16, (extent.height + 15) / 16, 1);

            // =========================================================================
            // Stage 2: Temporal Eye Adaptation
            // =========================================================================
            command.set_pipeline(adapt_pipeline);

            Array<GPUBindGroupEntry, 2> adapt_entries{};
            adapt_entries[0].type          = GPUResourceType::BUFFER;
            adapt_entries[0].binding       = 1;
            adapt_entries[0].buffer.buffer = histogram_buffer.handle;
            adapt_entries[0].buffer.offset = 0;
            adapt_entries[0].buffer.size   = 64 * sizeof(uint);

            adapt_entries[1].type    = GPUResourceType::STORAGE_TEXTURE;
            adapt_entries[1].binding = 2;
            adapt_entries[1].texture = exp_tex->view;

            auto adapt_bg_desc    = GPUBindGroupDescriptor{};
            adapt_bg_desc.heap    = heap;
            adapt_bg_desc.layout  = adapt_blayouts[0];
            adapt_bg_desc.entries = adapt_entries;
            auto adapt_bg         = device.create_bind_group(adapt_bg_desc);

            command.set_bind_group(0, adapt_bg);
            command.set_immediates(0, push);
            command.dispatch_workgroups(1, 1, 1);
        });
}
