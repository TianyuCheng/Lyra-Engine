#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/TonemappingPass.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    struct TonemapParams
    {
        float exposure      = 1.0f;
        uint  operator_type = 0; // 0 = ACES, 1 = Reinhard, 2 = AgX, 3 = Uncharted2
        float padding[2]    = {0.0f, 0.0f};
    };
} // namespace

bool TonemappingPass::init(
    GPUDevice        device,
    Compiler         compiler,
    GPUTextureFormat output_format)
{
    if (initialized) {
        if (cached_output_format == output_format) {
            return true;
        }
        destroy();
    }

    this->device               = device;
    this->cached_output_format = output_format;

    // compile tonemapping shader asset
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

    initialized = true;
    return true;
}

void TonemappingPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) heap.destroy();
    if (sampler.valid()) sampler.destroy();
    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();
    blayouts.clear();
    bind_group = {};

    initialized = false;
}

TonemappingData TonemappingPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> hdr_input,
    const RendererSettings&             settings,
    GPUTextureFormat                    output_format)
{
    return builder.add_pass<TonemappingData>(
        "TonemappingPass",
        [&](TonemappingData& data, FrameGraphPassBuilder& pass_builder) {
            data.hdr_input = pass_builder.sample(hdr_input);

            auto ldr_desc        = GPUTextureDescriptor{};
            ldr_desc.label       = "tonemapped_ldr_buffer";
            ldr_desc.size.width  = 1280;
            ldr_desc.size.height = 720;
            ldr_desc.size.depth  = 1;
            ldr_desc.dimension   = GPUTextureDimension::x2D;
            ldr_desc.format      = output_format;
            ldr_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.ldr_output      = pass_builder.create<FrameGraphTexture>(ldr_desc);
            data.ldr_output      = pass_builder.render(data.ldr_output);
        },
        [this, settings](const TonemappingData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto hdr_tex = resources.get<FrameGraphTexture>(data.hdr_input);
            auto ldr_tex = resources.get<FrameGraphTexture>(data.ldr_output);
            if (!hdr_tex || !ldr_tex) {
                return;
            }

            // create transient bind group pairing the HDR input texture view and linear sampler
            Array<GPUBindGroupEntry, 2> entries = {};
            entries.at(0).type                  = GPUResourceType::TEXTURE;
            entries.at(0).binding               = 0;
            entries.at(0).texture               = hdr_tex->view;

            entries.at(1).type    = GPUResourceType::SAMPLER;
            entries.at(1).binding = 1;
            entries.at(1).sampler = sampler;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.heap    = heap;
            bg_desc.layout  = blayouts.at(0);
            bg_desc.entries = entries;
            auto bg         = device.create_bind_group(bg_desc);

            auto color_attachment        = GPURenderPassColorAttachment{};
            color_attachment.view        = ldr_tex->view;
            color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 1.0f};
            color_attachment.load_op     = GPULoadOp::CLEAR;
            color_attachment.store_op    = GPUStoreOp::STORE;

            auto desc              = GPURenderPassDescriptor{};
            desc.color_attachments = color_attachment;

            TonemapParams params{};
            params.exposure      = settings.exposure;
            params.operator_type = 0; // ACES filmic default

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, 1280.0f, 720.0f);
            command.set_scissor_rect(0, 0, 1280, 720);
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bg);
            command.set_immediates(0, params);
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });
}
