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

    // 1. create compute pipeline for cs_build_histogram
    PipelineShaderDescriptor hist_desc{};
    hist_desc.module        = "sys://Shaders/PostProcess/AutoExposure.slang";
    hist_desc.compute_entry = "cs_build_histogram";

    histogram_pipeline = ComputePipelineBuilder(device, compiler)
                             .with_shader(hist_desc)
                             .with_label("auto_exposure_histogram_pipeline")
                             .build();
    if (!histogram_pipeline.valid()) {
        return false;
    }

    // 2. create compute pipeline for cs_adapt_exposure
    PipelineShaderDescriptor adapt_desc{};
    adapt_desc.module        = "sys://Shaders/PostProcess/AutoExposure.slang";
    adapt_desc.compute_entry = "cs_adapt_exposure";

    adapt_pipeline = ComputePipelineBuilder(device, compiler)
                         .with_shader(adapt_desc)
                         .with_label("auto_exposure_adapt_pipeline")
                         .build();
    if (!adapt_pipeline.valid()) {
        return false;
    }

    // 3. allocate persistent 64-bin histogram buffer (256 bytes)
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

    exposure_view.destroy();
    exposure_texture.destroy();
    histogram_buffer.destroy();
    histogram_pipeline.destroy();
    adapt_pipeline.destroy();
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
            hist_bg_desc.layout  = histogram_pipeline.layout.get_bind_group_layout(0);
            hist_bg_desc.entries = hist_entries;
            auto hist_bg         = ctx->allocate(hist_bg_desc);

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
            adapt_bg_desc.layout  = adapt_pipeline.layout.get_bind_group_layout(0);
            adapt_bg_desc.entries = adapt_entries;
            auto adapt_bg         = ctx->allocate(adapt_bg_desc);

            command.set_bind_group(0, adapt_bg);
            command.set_immediates(0, push);
            command.dispatch_workgroups(1, 1, 1);
        });
}
