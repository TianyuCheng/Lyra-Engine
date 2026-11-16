#include <Lyra/Rendering/Passes/ClearPass.h>

using namespace lyra;
using namespace lyra::passes;

ClearPassData ClearPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> color_target,
    FrameGraphHandle<FrameGraphTexture> depth_target,
    GPUExtent2D                         extent,
    GPUColor                            clear_color,
    float                               clear_depth)
{
    return builder.add_pass<ClearPassData>(
        "ClearPass",
        [&](ClearPassData& data, FrameGraphPassBuilder& pass_builder) {
            if (color_target.valid()) {
                data.color = pass_builder.render(color_target);
            }
            if (depth_target.valid()) {
                data.depth = pass_builder.render(depth_target);
            }
        },
        [extent, clear_color, clear_depth](const ClearPassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid()) {
                return;
            }

            auto render_pass = GPURenderPassDescriptor{};

            if (data.color.valid()) {
                if (auto color_tex = resources.get<FrameGraphTexture>(data.color)) {
                    auto color_attachment         = GPURenderPassColorAttachment{};
                    color_attachment.view         = color_tex->view;
                    color_attachment.clear_value  = clear_color;
                    color_attachment.load_op      = GPULoadOp::CLEAR;
                    color_attachment.store_op     = GPUStoreOp::STORE;
                    render_pass.color_attachments = color_attachment;
                }
            }

            if (data.depth.valid()) {
                if (auto depth_tex = resources.get<FrameGraphTexture>(data.depth)) {
                    auto depth_attachment                 = GPURenderPassDepthStencilAttachment{};
                    depth_attachment.view                 = depth_tex->view;
                    depth_attachment.depth_clear_value    = clear_depth;
                    depth_attachment.depth_load_op        = GPULoadOp::CLEAR;
                    depth_attachment.depth_store_op       = GPUStoreOp::STORE;
                    render_pass.depth_stencil_attachment = depth_attachment;
                }
            }

            ctx->cmdlist.begin_render_pass(render_pass);
            ctx->cmdlist.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            ctx->cmdlist.set_scissor_rect(0, 0, extent.width, extent.height);
            ctx->cmdlist.end_render_pass();
        });
}
