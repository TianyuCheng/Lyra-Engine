#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/TonemappingPass.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    struct TonemapPushConstants
    {
        float exposure          = 1.0f;
        uint  operator_type     = 0; // 0 = ACES, 1 = Reinhard, 2 = AgX, 3 = Uncharted2
        float bloom_intensity   = 1.0f;
        uint  use_auto_exposure = 0;
    };

    static_assert(sizeof(TonemapPushConstants) == 16, "TonemapPushConstants must be 16 bytes");
} // namespace

bool TonemappingPass::init(
    GPUDevice        dev,
    Compiler         compiler,
    GPUTextureFormat output_format)
{
    if (initialized) {
        if (cached_output_format == output_format) {
            return true;
        }
        destroy();
    }

    device               = dev;
    cached_output_format = output_format;

    // 1. compile tonemapping shader asset
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "tonemap";
        desc.path   = "sys://Shaders/PostProcess/Tonemap.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

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
        desc.label = "tonemap_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "tonemap_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    sampler = execute([&]() {
        auto desc           = GPUSamplerDescriptor{};
        desc.label          = "tonemap_linear_sampler";
        desc.min_filter     = GPUFilterMode::LINEAR;
        desc.mag_filter     = GPUFilterMode::LINEAR;
        desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
        desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
        return device.create_sampler(desc);
    });

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

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 2048;
        return device.create_bind_group_heap(desc);
    });

    // 2. default 1x1 black bloom fallback
    default_bloom_tex  = execute([&]() {
        auto desc            = GPUTextureDescriptor{};
        desc.label           = "default_black_bloom";
        desc.size            = {1, 1, 1};
        desc.dimension       = GPUTextureDimension::x2D;
        desc.format          = GPUTextureFormat::RGBA16FLOAT;
        desc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        desc.mip_level_count = 1;
        return device.create_texture(desc);
    });
    default_bloom_view = default_bloom_tex.create_view();

    // 3. default 1x1 unit exposure fallback
    default_exposure_tex  = execute([&]() {
        auto desc            = GPUTextureDescriptor{};
        desc.label           = "default_unit_exposure";
        desc.size            = {1, 1, 1};
        desc.dimension       = GPUTextureDimension::x2D;
        desc.format          = GPUTextureFormat::RGBA16FLOAT;
        desc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        desc.mip_level_count = 1;
        return device.create_texture(desc);
    });
    default_exposure_view = default_exposure_tex.create_view();

    execute([&]() {
        auto cmd_desc  = GPUCommandBufferDescriptor{};
        cmd_desc.label = "tonemap_default_tex_transition_cmd";
        cmd_desc.queue = GPUQueueType::DEFAULT;
        auto cmdbuf    = device.create_command_buffer(cmd_desc);
        cmdbuf.resource_barrier(state_transition(default_bloom_tex, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        cmdbuf.resource_barrier(state_transition(default_exposure_tex, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        cmdbuf.submit();
        device.wait();
    });

    initialized = pipeline.valid();
    return initialized;
}

void TonemappingPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (default_exposure_view.valid()) default_exposure_view.destroy();
    if (default_exposure_tex.valid()) default_exposure_tex.destroy();
    if (default_bloom_view.valid()) default_bloom_view.destroy();
    if (default_bloom_tex.valid()) default_bloom_tex.destroy();
    if (heap.valid()) heap.destroy();
    if (sampler.valid()) sampler.destroy();
    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();
    blayouts.clear();

    initialized = false;
}

TonemappingData TonemappingPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> hdr_input,
    GPUExtent2D                         extent,
    FrameGraphHandle<FrameGraphTexture> bloom_input,
    FrameGraphHandle<FrameGraphTexture> exposure_input,
    const TonemappingSettings&          settings,
    GPUTextureFormat                    output_format,
    FrameGraphHandle<FrameGraphTexture> output_target)
{
    return builder.add_pass<TonemappingData>(
        "TonemappingPass",
        [&](TonemappingData& data, FrameGraphPassBuilder& pass_builder) {
            data.hdr_input = pass_builder.sample(hdr_input);
            if (bloom_input.valid()) {
                data.bloom_input = pass_builder.sample(bloom_input);
            }
            if (exposure_input.valid()) {
                data.exposure_input = pass_builder.sample(exposure_input);
            }

            if (output_target.valid()) {
                data.ldr_output = pass_builder.render(output_target);
            } else {
                auto ldr_desc        = GPUTextureDescriptor{};
                ldr_desc.label       = "tonemapped_ldr_buffer";
                ldr_desc.size.width  = extent.width;
                ldr_desc.size.height = extent.height;
                ldr_desc.size.depth  = 1;
                ldr_desc.dimension   = GPUTextureDimension::x2D;
                ldr_desc.format      = output_format;
                ldr_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
                data.ldr_output      = pass_builder.create<FrameGraphTexture>(ldr_desc);
                data.ldr_output      = pass_builder.render(data.ldr_output);
            }
        },
        [this, extent, settings](const TonemappingData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto hdr_tex = resources.get<FrameGraphTexture>(data.hdr_input);
            auto ldr_tex = resources.get<FrameGraphTexture>(data.ldr_output);
            if (!hdr_tex || !ldr_tex) {
                return;
            }

            GPUTextureView bloom_v = default_bloom_view;
            if (data.bloom_input.valid()) {
                if (auto b_tex = resources.get<FrameGraphTexture>(data.bloom_input)) {
                    bloom_v = b_tex->view;
                }
            }

            GPUTextureView exposure_v = default_exposure_view;
            if (data.exposure_input.valid()) {
                if (auto e_tex = resources.get<FrameGraphTexture>(data.exposure_input)) {
                    exposure_v = e_tex->view;
                }
            }

            Array<GPUBindGroupEntry, 4> entries{};
            entries[0].type    = GPUResourceType::TEXTURE;
            entries[0].binding = 0;
            entries[0].texture = hdr_tex->view;

            entries[1].type    = GPUResourceType::SAMPLER;
            entries[1].binding = 1;
            entries[1].sampler = sampler;

            entries[2].type    = GPUResourceType::TEXTURE;
            entries[2].binding = 2;
            entries[2].texture = bloom_v;

            entries[3].type    = GPUResourceType::TEXTURE;
            entries[3].binding = 3;
            entries[3].texture = exposure_v;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.heap    = heap;
            bg_desc.layout  = blayouts[0];
            bg_desc.entries = entries;
            auto bg         = device.create_bind_group(bg_desc);

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = ldr_tex->view;
            color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
            color_attachment.load_op     = GPULoadOp::CLEAR;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto desc              = GPURenderPassDescriptor{};
            desc.color_attachments = color_attachment;

            TonemapPushConstants push{};
            push.exposure          = settings.exposure;
            push.operator_type     = settings.operator_type;
            push.bloom_intensity   = settings.bloom_intensity;
            push.use_auto_exposure = settings.use_auto_exposure ? 1 : 0;

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
