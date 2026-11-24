#include <algorithm>
#include <cmath>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/FXAAPass.h>

using namespace lyra;
using namespace lyra::passes;

bool FXAAPass::init(GPUDevice dev, Compiler compiler, GPUTextureFormat output_format)
{
    if (initialized) {
        if (cached_output_format == output_format) {
            return true;
        }
        destroy();
    }

    device               = dev;
    cached_output_format = output_format;

    // 1. compile vertex and fragment shaders from sys://Shaders/PostProcess/FXAA.slang
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "fxaa";
        desc.path   = "sys://Shaders/PostProcess/FXAA.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

    // 2. reflect and create render pipeline
    auto reflection = compiler.reflect({
        {*module, "vsmain"},
        {*module, "fsmain"},
    });
    if (!reflection) {
        return false;
    }

    vshader = execute([&]() {
        auto code  = module->get_shader_blob("vsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "fxaa_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "fxaa_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    if (!vshader.valid() || !fshader.valid()) {
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

    // 3. create bilinear clamp sampler
    linear_sampler = execute([&]() {
        auto desc           = GPUSamplerDescriptor{};
        desc.label          = "fxaa_linear_sampler";
        desc.min_filter     = GPUFilterMode::LINEAR;
        desc.mag_filter     = GPUFilterMode::LINEAR;
        desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
        desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
        return device.create_sampler(desc);
    });

    pipeline = execute([&]() {
        Vector<GPUColorTargetState> rstates;
        auto                        color_state = GPUColorTargetState{};
        color_state.format                      = cached_output_format;
        color_state.blend_enable                = false;
        rstates.push_back(color_state);

        auto desc                         = GPURenderPipelineDescriptor{};
        desc.layout                       = playout;
        desc.primitive.cull_mode          = GPUCullMode::NONE;
        desc.primitive.topology           = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face         = GPUFrontFace::CCW;
        desc.primitive.strip_index_format = GPUIndexFormat::UINT32;
        desc.multisample.count            = 1;
        desc.vertex.module                = vshader;
        desc.vertex.buffers               = {};
        desc.vertex.entry_point           = "vsmain";
        desc.fragment.module              = fshader;
        desc.fragment.targets             = rstates;
        desc.fragment.entry_point         = "fsmain";
        return device.create_render_pipeline(desc);
    });

    initialized = pipeline.valid();
    return initialized;
}

void FXAAPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (linear_sampler.valid()) linear_sampler.destroy();
    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();

    blayouts.clear();
    initialized = false;
}

FXAAData FXAAPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> input_color,
    GPUExtent2D                         extent,
    GPUTextureFormat                    format,
    const FXAASettings&                 settings,
    FrameGraphHandle<FrameGraphTexture> output_target)
{
    return builder.add_pass<FXAAData>(
        "FXAAPass",
        [&](FXAAData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_input = pass_builder.sample(input_color);

            if (output_target.valid()) {
                data.color_output = pass_builder.render(output_target);
            } else {
                auto out_desc            = GPUTextureDescriptor{};
                out_desc.label           = "fxaa_output_texture";
                out_desc.size.width      = extent.width;
                out_desc.size.height     = extent.height;
                out_desc.size.depth      = 1;
                out_desc.dimension       = GPUTextureDimension::x2D;
                out_desc.format          = format;
                out_desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                out_desc.mip_level_count = 1;

                data.color_output = pass_builder.create<FrameGraphTexture>(out_desc);
                data.color_output = pass_builder.render(data.color_output);
            }
        },
        [this, extent, settings](const FXAAData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !valid()) {
                return;
            }

            auto in_tex  = resources.get<FrameGraphTexture>(data.color_input);
            auto out_tex = resources.get<FrameGraphTexture>(data.color_output);
            if (!in_tex || !out_tex) {
                return;
            }

            Array<GPUBindGroupEntry, 2> entries{};
            entries[0].type    = GPUResourceType::TEXTURE;
            entries[0].binding = 0;
            entries[0].texture = in_tex->view;

            entries[1].type    = GPUResourceType::SAMPLER;
            entries[1].binding = 1;
            entries[1].sampler = linear_sampler;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.layout  = blayouts[0];
            bg_desc.entries = entries;
            auto bg         = ctx->allocate(bg_desc);

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = out_tex->view;
            color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
            color_attachment.load_op     = GPULoadOp::CLEAR;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto desc              = GPURenderPassDescriptor{};
            desc.color_attachments = color_attachment;

            FXAAPushConstants push{};
            push.inv_screen_size    = Vector2(1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height));
            push.subpixel_quality   = settings.subpixel_quality;
            push.edge_threshold     = settings.edge_threshold;
            push.edge_threshold_min = settings.edge_threshold_min;
            push.screen_width       = extent.width;
            push.screen_height      = extent.height;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bg);
            command.set_immediates(0, push);
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });
}
