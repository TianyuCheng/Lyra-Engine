#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/EditorGridPass.h>
#include <Lyra/Rendering/ViewUniforms.h>

using namespace lyra;
using namespace lyra::passes;

bool EditorGridPass::init(
    GPUDevice        device,
    Compiler         compiler,
    GPUTextureFormat color_format,
    GPUTextureFormat depth_format)
{
    // check idempotency and handle format updates
    if (initialized) {
        if (cached_color_format == color_format && cached_depth_format == depth_format) {
            return true;
        }
        destroy();
    }

    this->device              = device;
    this->cached_color_format = color_format;
    this->cached_depth_format = depth_format;

    // compile shader program from engine shader asset
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module         = "sys://Shaders/Editor/Grid.slang";
    shader_desc.vertex_entry   = "vsmain";
    shader_desc.fragment_entry = "fsmain";

    pipeline = RenderPipelineBuilder(device, compiler)
                   .with_shader(shader_desc)
                   .with_cull_mode(GPUCullMode::NONE)
                   .with_topology(GPUPrimitiveTopology::TRIANGLE_LIST)
                   .with_front_face(GPUFrontFace::CCW)
                   .with_target(cached_color_format, BlendPreset::ALPHA)
                   .with_depth_stencil(cached_depth_format, false, GPUCompareFunction::LESS_EQUAL)
                   .with_label("editor_grid_pipeline")
                   .build();
    if (!pipeline.valid()) {
        return false;
    }

    initialized = pipeline.valid();
    return initialized;
}

void EditorGridPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    pipeline.destroy();
    initialized = false;
}

EditorGridData EditorGridPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> color_target,
    FrameGraphHandle<FrameGraphTexture> depth_target,
    const RenderView&                   view,
    const EditorGridSettings&           settings)
{
    MAYBE_UNUSED(settings);

    return builder.add_pass<EditorGridData>(
        "EditorGridPass",
        [&](EditorGridData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_output = pass_builder.render(color_target);
            data.depth_input  = pass_builder.read(depth_target);
        },
        [this, extent = view.render_extent](const EditorGridData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto color_tex = resources.get<FrameGraphTexture>(data.color_output);
            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_input);
            if (!color_tex || !depth_tex) {
                return;
            }

            // color attachment: load rendered scene color to blend grid over it
            auto color_attachment     = GPURenderPassColorAttachment{};
            color_attachment.view     = color_tex->view;
            color_attachment.load_op  = GPULoadOp::LOAD;
            color_attachment.store_op = GPUStoreOp::STORE;

            // depth attachment: load scene depth buffer for depth-testing against ground plane
            auto depth_attachment            = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view            = depth_tex->view;
            depth_attachment.depth_load_op   = GPULoadOp::LOAD;
            depth_attachment.depth_store_op  = GPUStoreOp::STORE;
            depth_attachment.depth_read_only = true;

            auto render_pass                     = GPURenderPassDescriptor{};
            render_pass.color_attachments        = color_attachment;
            render_pass.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(render_pass);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            const auto& blayouts = pipeline.layout.bind_group_layouts;
            if (ctx->view_uniforms && ctx->arena && !blayouts.empty()) {
                auto camera_bg = ctx->view_uniforms->bind_group(blayouts[0], *ctx->arena, ctx->device);
                if (camera_bg.valid()) {
                    command.set_bind_group(0, camera_bg);
                }
            }
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });
}
