#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/EditorGridPass.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    struct CameraUniform
    {
        Matrix4x4 proj;
        Matrix4x4 view;
        Matrix4x4 inv_proj;
        Matrix4x4 inv_view;
        Vector3   pos;
        float     padding = 0.0f;
    };
} // namespace

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
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "grid";
        desc.path   = "sys://Shaders/Editor/Grid.slang";
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
        desc.label = "grid_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "grid_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
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
        // color targets with standard premultiplied / alpha blending
        Vector<GPUColorTargetState> rstates;
        auto                        color_state = GPUColorTargetState{};
        color_state.format                      = cached_color_format;
        color_state.blend_enable                = true;
        color_state.blend.color.operation       = GPUBlendOperation::ADD;
        color_state.blend.color.src_factor      = GPUBlendFactor::SRC_ALPHA;
        color_state.blend.color.dst_factor      = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        color_state.blend.alpha.operation       = GPUBlendOperation::ADD;
        color_state.blend.alpha.src_factor      = GPUBlendFactor::ONE;
        color_state.blend.alpha.dst_factor      = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        rstates.push_back(color_state);

        auto desc                                  = GPURenderPipelineDescriptor{};
        desc.layout                                = playout;
        desc.primitive.cull_mode                   = GPUCullMode::NONE;
        desc.primitive.topology                    = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face                  = GPUFrontFace::CCW;
        desc.primitive.strip_index_format          = GPUIndexFormat::UINT32;
        desc.depth_stencil.depth_compare           = GPUCompareFunction::LESS_EQUAL;
        desc.depth_stencil.depth_write_enabled     = false;
        desc.depth_stencil.format                  = cached_depth_format;
        desc.multisample.alpha_to_coverage_enabled = false;
        desc.multisample.count                     = 1;
        desc.vertex.module                         = vshader;
        desc.vertex.buffers                        = {};
        desc.vertex.entry_point                    = "vsmain";
        desc.fragment.module                       = fshader;
        desc.fragment.targets                      = rstates;
        desc.fragment.entry_point                  = "fsmain";
        return device.create_render_pipeline(desc);
    });

    for (uint i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        camera_buffers[i] = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "grid_camera_buffer";
            desc.size               = sizeof(CameraUniform);
            desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });
    }

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 2048;
        return device.create_bind_group_heap(desc);
    });

    if (!blayouts.empty()) {
        for (uint i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            bind_groups[i] = execute([&]() {
                Array<GPUBindGroupEntry, 1> entries = {};

                auto& entry         = entries.at(0);
                entry.type          = GPUResourceType::BUFFER;
                entry.binding       = 0;
                entry.buffer.buffer = camera_buffers[i];
                entry.buffer.offset = 0;
                entry.buffer.size   = 0;

                auto desc    = GPUBindGroupDescriptor{};
                desc.heap    = heap;
                desc.layout  = blayouts.at(0);
                desc.entries = entries;
                return device.create_bind_group(desc);
            });
        }
    }

    initialized = true;
    return true;
}

void EditorGridPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) heap.destroy();
    for (auto& buf : camera_buffers) {
        if (buf.valid()) {
            buf.destroy();
        }
    }
    camera_buffers.fill({});
    bind_groups.fill({});

    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();

    blayouts.clear();
    initialized = false;
}

void EditorGridPass::update_camera_uniform(uint slot, const RenderView& view)
{
    if (slot >= FRAMES_IN_FLIGHT || !camera_buffers[slot].valid()) {
        return;
    }

    auto camera_gpu           = camera_buffers[slot].get_mapped_range<CameraUniform>();
    camera_gpu.at(0).proj     = view.projection;
    camera_gpu.at(0).view     = view.view;
    camera_gpu.at(0).inv_proj = view.inv_projection;
    camera_gpu.at(0).inv_view = view.inv_view;
    camera_gpu.at(0).pos      = view.camera_position;
    camera_gpu.at(0).padding  = 0.0f;
}

EditorGridData EditorGridPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> color_target,
    FrameGraphHandle<FrameGraphTexture> depth_target,
    const RenderView&                   view,
    const EditorGridSettings&           settings)
{
    MAYBE_UNUSED(settings);

    frame_slot        = (frame_slot + 1) % FRAMES_IN_FLIGHT;
    uint current_slot = frame_slot;

    // update camera uniforms for view in current slot
    update_camera_uniform(current_slot, view);

    return builder.add_pass<EditorGridData>(
        "EditorGridPass",
        [&](EditorGridData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_output = pass_builder.render(color_target);
            data.depth_input  = pass_builder.read(depth_target);
        },
        [this, current_slot, extent = view.render_extent](const EditorGridData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
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
            auto depth_attachment           = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view           = depth_tex->view;
            depth_attachment.depth_load_op  = GPULoadOp::LOAD;
            depth_attachment.depth_store_op = GPUStoreOp::STORE;

            auto render_pass                     = GPURenderPassDescriptor{};
            render_pass.color_attachments        = color_attachment;
            render_pass.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(render_pass);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            if (bind_groups[current_slot].valid()) {
                command.set_bind_group(0, bind_groups[current_slot]);
            }
            command.draw(6, 1, 0, 0);
            command.end_render_pass();
        });
}
