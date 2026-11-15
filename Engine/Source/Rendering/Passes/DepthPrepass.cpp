#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/DepthPrepass.h>

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

    struct InstanceData
    {
        Matrix4x4 model;
    };
} // namespace

bool DepthPrepass::init(
    GPUDevice        device,
    Compiler         compiler,
    GPUTextureFormat depth_format)
{
    if (initialized) {
        if (cached_depth_format == depth_format) {
            return true;
        }
        destroy();
    }

    this->device              = device;
    this->cached_depth_format = depth_format;

    // compile depth-only shader asset
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "depth_prepass";
        desc.path   = "sys://Shaders/Forward/DepthPrepass.slang";
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
        desc.label = "depth_prepass_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "depth_prepass_fragment_shader";
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
        Vector<GPUVertexAttribute> attributes;
        auto                       pos_attr = GPUVertexAttribute{};
        pos_attr.format                     = GPUVertexFormat::FLOAT32x3;
        pos_attr.offset                     = 0;
        pos_attr.shader_location            = 0;
        attributes.push_back(pos_attr);

        Vector<GPUVertexBufferLayout> vlayouts;
        auto                          vlayout = GPUVertexBufferLayout{};
        vlayout.array_stride                  = sizeof(Vector3) * 2 + sizeof(Vector2); // interleaved vertex stride
        vlayout.step_mode                     = GPUVertexStepMode::VERTEX;
        vlayout.attributes                    = attributes;
        vlayouts.push_back(vlayout);

        auto desc                              = GPURenderPipelineDescriptor{};
        desc.layout                            = playout;
        desc.primitive.cull_mode               = GPUCullMode::BACK;
        desc.primitive.topology                = GPUPrimitiveTopology::TRIANGLE_LIST;
        desc.primitive.front_face              = GPUFrontFace::CCW;
        desc.primitive.strip_index_format      = GPUIndexFormat::UINT32;
        desc.depth_stencil.depth_compare       = GPUCompareFunction::LESS_EQUAL;
        desc.depth_stencil.depth_write_enabled = true;
        desc.depth_stencil.format              = cached_depth_format;
        desc.multisample.count                 = 1;
        desc.vertex.module                     = vshader;
        desc.vertex.buffers                    = vlayouts;
        desc.vertex.entry_point                = "vsmain";
        desc.fragment.module                   = fshader;
        desc.fragment.targets                  = {}; // no color targets for depth-only pass
        desc.fragment.entry_point              = "fsmain";
        return device.create_render_pipeline(desc);
    });

    camera_buffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "depth_prepass_camera_buffer";
        desc.size               = sizeof(CameraUniform);
        desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 2048;
        return device.create_bind_group_heap(desc);
    });

    bind_group = execute([&]() {
        Array<GPUBindGroupEntry, 1> entries = {};

        auto& entry         = entries.at(0);
        entry.type          = GPUResourceType::BUFFER;
        entry.binding       = 0;
        entry.buffer.buffer = camera_buffer;
        entry.buffer.offset = 0;
        entry.buffer.size   = 0;

        auto desc    = GPUBindGroupDescriptor{};
        desc.heap    = heap;
        desc.layout  = blayouts.at(0);
        desc.entries = entries;
        return device.create_bind_group(desc);
    });

    initialized = true;
    return true;
}

void DepthPrepass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) heap.destroy();
    if (camera_buffer.valid()) camera_buffer.destroy();
    if (pipeline.valid()) pipeline.destroy();
    if (playout.valid()) playout.destroy();
    if (fshader.valid()) fshader.destroy();
    if (vshader.valid()) vshader.destroy();
    blayouts.clear();
    bind_group = {};

    initialized = false;
}

void DepthPrepass::update_camera_uniform(const RenderView& view)
{
    if (!camera_buffer.valid()) {
        return;
    }

    auto camera_gpu           = camera_buffer.get_mapped_range<CameraUniform>();
    camera_gpu.at(0).proj     = view.projection;
    camera_gpu.at(0).view     = view.view;
    camera_gpu.at(0).inv_proj = view.inv_projection;
    camera_gpu.at(0).inv_view = view.inv_view;
    camera_gpu.at(0).pos      = view.camera_position;
    camera_gpu.at(0).padding  = 0.0f;
}

DepthPrepassData DepthPrepass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    GPUExtent2D        extent)
{
    update_camera_uniform(view);

    return builder.add_pass<DepthPrepassData>(
        "DepthPrepass",
        [&](DepthPrepassData& data, FrameGraphPassBuilder& pass_builder) {
            auto desc        = GPUTextureDescriptor{};
            desc.label       = "depth_prepass_buffer";
            desc.size.width  = extent.width;
            desc.size.height = extent.height;
            desc.size.depth  = 1;
            desc.dimension   = GPUTextureDimension::x2D;
            desc.format      = cached_depth_format;
            desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;

            data.depth_texture = pass_builder.create<FrameGraphTexture>(desc);
            data.depth_texture = pass_builder.render(data.depth_texture);
        },
        [this, extent, &scene](const DepthPrepassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_texture);
            if (!depth_tex) {
                return;
            }

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto desc                     = GPURenderPassDescriptor{};
            desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bind_group);

            // rasterize all opaque scene meshes
            for (const auto& mesh : scene.meshes) {
                if (!mesh.geometry.valid() || mesh.index_count == 0) {
                    continue;
                }

                InstanceData inst{mesh.transform};
                command.set_immediates(0, inst);
                GPUBuffer vbuf{};
                vbuf.handle = mesh.geometry.vertex_slice.buffer;
                GPUBuffer ibuf{};
                ibuf.handle = mesh.geometry.index_slice.buffer;

                command.set_vertex_buffer(0, vbuf, mesh.geometry.vertex_slice.offset);
                command.set_index_buffer(ibuf, mesh.geometry.index_format, mesh.geometry.index_slice.offset);
                command.draw_indexed(mesh.index_count, 1, mesh.first_index, 0, 0);
            }

            command.end_render_pass();
        });
}
