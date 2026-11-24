#include <algorithm>
#include <cmath>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/DepthOfFieldPass.h>

using namespace lyra;
using namespace lyra::passes;

bool DepthOfFieldPass::init(GPUDevice dev, Compiler compiler)
{
    if (initialized) {
        return true;
    }

    device = dev;

    // 1. compile compute shader module from sys://Shaders/PostProcess/DepthOfField.slang
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "depth_of_field";
        desc.path   = "sys://Shaders/PostProcess/DepthOfField.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

    // 2. reflect and create compute pipeline
    auto reflection = compiler.reflect({
        {*module, "cs_dof_bokeh"},
    });
    if (!reflection) {
        return false;
    }

    cshader = execute([&]() {
        auto code  = module->get_shader_blob("cs_dof_bokeh");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "dof_compute_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });
    if (!cshader.valid()) {
        return false;
    }

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

    pipeline = execute([&]() {
        auto cp_desc                = GPUComputePipelineDescriptor{};
        cp_desc.label               = "dof_pipeline";
        cp_desc.layout              = playout;
        cp_desc.compute.module      = cshader;
        cp_desc.compute.entry_point = "cs_dof_bokeh";
        return device.create_compute_pipeline(cp_desc);
    });

    // 3. create linear clamp sampler
    linear_sampler = execute([&]() {
        auto desc           = GPUSamplerDescriptor{};
        desc.label          = "dof_linear_sampler";
        desc.min_filter     = GPUFilterMode::LINEAR;
        desc.mag_filter     = GPUFilterMode::LINEAR;
        desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
        desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
        return device.create_sampler(desc);
    });

    initialized = pipeline.valid();
    return initialized;
}

void DepthOfFieldPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (linear_sampler.valid()) linear_sampler.destroy();
    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (cshader.valid()) cshader.destroy();

    blayouts.clear();
    initialized = false;
}

DOFData DepthOfFieldPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> color_texture,
    FrameGraphHandle<FrameGraphTexture> depth_texture,
    GPUExtent2D                         extent,
    float                               near_plane,
    float                               far_plane,
    const DOFSettings&                  settings)
{
    return builder.add_pass<DOFData>(
        "DepthOfFieldPass",
        [&](DOFData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_input = pass_builder.sample(color_texture);
            data.depth_input = pass_builder.sample(depth_texture);

            auto out_desc            = GPUTextureDescriptor{};
            out_desc.label           = "dof_output_texture";
            out_desc.size.width      = extent.width;
            out_desc.size.height     = extent.height;
            out_desc.size.depth      = 1;
            out_desc.dimension       = GPUTextureDimension::x2D;
            out_desc.format          = GPUTextureFormat::RGBA16FLOAT;
            out_desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
            out_desc.mip_level_count = 1;

            data.dof_output = pass_builder.create<FrameGraphTexture>(out_desc);
            data.dof_output = pass_builder.write(data.dof_output);
        },
        [this, extent, near_plane, far_plane, settings](const DOFData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !valid()) {
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_input);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_input);
            auto out_tex   = resources.get<FrameGraphTexture>(data.dof_output);
            if (!color_tex || !depth_tex || !out_tex) {
                return;
            }

            auto command = ctx->cmdlist;

            DOFPushConstants push{};
            push.inv_screen_size = Vector2(1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height));
            push.focal_distance  = settings.focal_distance;
            push.focal_length    = settings.focal_length;
            push.aperture_f_stop = settings.aperture_f_stop;
            push.max_coc_radius  = settings.enabled ? settings.max_coc_radius : 0.0f;
            push.near_plane      = near_plane;
            push.far_plane       = far_plane;
            push.screen_width    = extent.width;
            push.screen_height   = extent.height;

            command.set_pipeline(pipeline);

            Array<GPUBindGroupEntry, 4> entries{};
            entries[0].type    = GPUResourceType::TEXTURE;
            entries[0].binding = 0;
            entries[0].texture = color_tex->view;

            entries[1].type    = GPUResourceType::TEXTURE;
            entries[1].binding = 1;
            entries[1].texture = depth_tex->view;

            entries[2].type    = GPUResourceType::SAMPLER;
            entries[2].binding = 2;
            entries[2].sampler = linear_sampler;

            entries[3].type    = GPUResourceType::STORAGE_TEXTURE;
            entries[3].binding = 3;
            entries[3].texture = out_tex->view;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.layout  = blayouts[0];
            bg_desc.entries = entries;
            auto bg         = ctx->allocate(bg_desc);

            command.set_bind_group(0, bg);
            command.set_immediates(0, push);
            command.dispatch_workgroups((extent.width + 7) / 8, (extent.height + 7) / 8, 1);
        });
}
