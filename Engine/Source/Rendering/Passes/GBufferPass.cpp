#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/GBufferPass.h>

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
        Matrix4x4    model;
        Vector4      base_color    = Vector4(1.0f);
        float        roughness     = 0.5f;
        float        metallic      = 0.0f;
        ShadingModel shading_model = ShadingModel::PBR;
        uint         material_id   = 0;
    };
} // namespace

bool GBufferPass::init(
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

    // compile G-Buffer shader asset
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "gbuffer";
        desc.path   = "sys://Shaders/Deferred/GBuffer.slang";
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
        desc.label = "gbuffer_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "gbuffer_fragment_shader";
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
        Vector<GPUVertexBufferLayout> vlayouts;

        // slot 0: position stream (float3, stride 12)
        auto pos_attr            = GPUVertexAttribute{};
        pos_attr.format          = GPUVertexFormat::FLOAT32x3;
        pos_attr.offset          = 0;
        pos_attr.shader_location = 0;
        pos_attr.shader_semantic = "POSITION";

        Vector<GPUVertexAttribute> pos_attribs{pos_attr};
        auto                       pos_layout = GPUVertexBufferLayout{};
        pos_layout.array_stride               = sizeof(Vector3);
        pos_layout.step_mode                  = GPUVertexStepMode::VERTEX;
        pos_layout.attributes                 = pos_attribs;
        vlayouts.push_back(pos_layout);

        // slot 1: normal stream (float3, stride 12)
        auto norm_attr            = GPUVertexAttribute{};
        norm_attr.format          = GPUVertexFormat::FLOAT32x3;
        norm_attr.offset          = 0;
        norm_attr.shader_location = 1;
        norm_attr.shader_semantic = "NORMAL";

        Vector<GPUVertexAttribute> norm_attribs{norm_attr};
        auto                       norm_layout = GPUVertexBufferLayout{};
        norm_layout.array_stride               = sizeof(Vector3);
        norm_layout.step_mode                  = GPUVertexStepMode::VERTEX;
        norm_layout.attributes                 = norm_attribs;
        vlayouts.push_back(norm_layout);

        // slot 2: texcoord0 stream (float2, stride 8)
        auto uv_attr            = GPUVertexAttribute{};
        uv_attr.format          = GPUVertexFormat::FLOAT32x2;
        uv_attr.offset          = 0;
        uv_attr.shader_location = 2;
        uv_attr.shader_semantic = "TEXCOORD";

        Vector<GPUVertexAttribute> uv_attribs{uv_attr};
        auto                       uv_layout = GPUVertexBufferLayout{};
        uv_layout.array_stride               = sizeof(Vector2);
        uv_layout.step_mode                  = GPUVertexStepMode::VERTEX;
        uv_layout.attributes                 = uv_attribs;
        vlayouts.push_back(uv_layout);

        // MRT color target states
        Vector<GPUColorTargetState> rstates(3);
        // 0: Albedo / Metallic
        rstates[0].format       = GPUTextureFormat::RGBA8UNORM;
        rstates[0].blend_enable = false;
        // 1: Normal / Roughness
        rstates[1].format       = GPUTextureFormat::RGBA16FLOAT;
        rstates[1].blend_enable = false;
        // 2: Material Info
        rstates[2].format       = GPUTextureFormat::RGBA32UINT;
        rstates[2].blend_enable = false;

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
        desc.fragment.targets                  = rstates;
        desc.fragment.entry_point              = "fsmain";
        return device.create_render_pipeline(desc);
    });

    camera_buffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "gbuffer_camera_buffer";
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

void GBufferPass::destroy()
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

void GBufferPass::update_camera_uniform(const RenderView& view)
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

GBufferData GBufferPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    GPUExtent2D        extent)
{
    update_camera_uniform(view);

    return builder.add_pass<GBufferData>(
        "GBufferPass",
        [&](GBufferData& data, FrameGraphPassBuilder& pass_builder) {
            auto albedo_desc        = GPUTextureDescriptor{};
            albedo_desc.label       = "gbuffer_albedo_metallic";
            albedo_desc.size.width  = extent.width;
            albedo_desc.size.height = extent.height;
            albedo_desc.size.depth  = 1;
            albedo_desc.dimension   = GPUTextureDimension::x2D;
            albedo_desc.format      = GPUTextureFormat::RGBA8UNORM;
            albedo_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.albedo_metallic    = pass_builder.create<FrameGraphTexture>(albedo_desc);

            auto normal_desc        = GPUTextureDescriptor{};
            normal_desc.label       = "gbuffer_normal_roughness";
            normal_desc.size.width  = extent.width;
            normal_desc.size.height = extent.height;
            normal_desc.size.depth  = 1;
            normal_desc.dimension   = GPUTextureDimension::x2D;
            normal_desc.format      = GPUTextureFormat::RGBA16FLOAT;
            normal_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.normal_roughness   = pass_builder.create<FrameGraphTexture>(normal_desc);

            auto mat_desc        = GPUTextureDescriptor{};
            mat_desc.label       = "gbuffer_material_info";
            mat_desc.size.width  = extent.width;
            mat_desc.size.height = extent.height;
            mat_desc.size.depth  = 1;
            mat_desc.dimension   = GPUTextureDimension::x2D;
            mat_desc.format      = GPUTextureFormat::RGBA32UINT;
            mat_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.material_info   = pass_builder.create<FrameGraphTexture>(mat_desc);

            auto depth_desc        = GPUTextureDescriptor{};
            depth_desc.label       = "gbuffer_depth_stencil";
            depth_desc.size.width  = extent.width;
            depth_desc.size.height = extent.height;
            depth_desc.size.depth  = 1;
            depth_desc.dimension   = GPUTextureDimension::x2D;
            depth_desc.format      = cached_depth_format;
            depth_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.depth_stencil     = pass_builder.create<FrameGraphTexture>(depth_desc);

            data.albedo_metallic  = pass_builder.render(data.albedo_metallic);
            data.normal_roughness = pass_builder.render(data.normal_roughness);
            data.material_info    = pass_builder.render(data.material_info);
            data.depth_stencil    = pass_builder.render(data.depth_stencil);
        },
        [this, extent, &scene](const GBufferData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto albedo_tex   = resources.get<FrameGraphTexture>(data.albedo_metallic);
            auto normal_tex   = resources.get<FrameGraphTexture>(data.normal_roughness);
            auto material_tex = resources.get<FrameGraphTexture>(data.material_info);
            auto depth_tex    = resources.get<FrameGraphTexture>(data.depth_stencil);
            if (!albedo_tex || !normal_tex || !material_tex || !depth_tex) {
                return;
            }

            Vector<GPURenderPassColorAttachment> color_attachments(3);
            color_attachments[0].view        = albedo_tex->view;
            color_attachments[0].clear_value = GPUColor{scene.clear_color.x, scene.clear_color.y, scene.clear_color.z, 0.0f};
            color_attachments[0].load_op     = GPULoadOp::CLEAR;
            color_attachments[0].store_op    = GPUStoreOp::STORE;

            color_attachments[1].view        = normal_tex->view;
            color_attachments[1].clear_value = GPUColor{0.5f, 0.5f, 1.0f, 1.0f};
            color_attachments[1].load_op     = GPULoadOp::CLEAR;
            color_attachments[1].store_op    = GPUStoreOp::STORE;

            color_attachments[2].view        = material_tex->view;
            color_attachments[2].clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};
            color_attachments[2].load_op     = GPULoadOp::CLEAR;
            color_attachments[2].store_op    = GPUStoreOp::STORE;

            auto depth_attachment              = GPURenderPassDepthStencilAttachment{};
            depth_attachment.view              = depth_tex->view;
            depth_attachment.depth_clear_value = 1.0f;
            depth_attachment.depth_load_op     = GPULoadOp::CLEAR;
            depth_attachment.depth_store_op    = GPUStoreOp::STORE;

            auto desc                     = GPURenderPassDescriptor{};
            desc.color_attachments        = color_attachments;
            desc.depth_stencil_attachment = depth_attachment;

            auto command = ctx->cmdlist;
            command.begin_render_pass(desc);
            command.set_viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
            command.set_scissor_rect(0, 0, extent.width, extent.height);
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bind_group);

            // rasterize all opaque scene meshes into G-Buffer
            for (const auto& mesh : scene.meshes) {
                if (!mesh.geometry.valid() || mesh.index_count == 0) {
                    continue;
                }

                InstanceData inst{};
                inst.model         = mesh.transform;
                inst.base_color    = mesh.material_params.base_color;
                inst.roughness     = mesh.material_params.roughness;
                inst.metallic      = mesh.material_params.metallic;
                inst.shading_model = mesh.shading_model;

                command.set_immediates(0, inst);
                GPUBuffer vbuf{};
                vbuf.handle = mesh.geometry.vertex_slice.buffer;
                GPUBuffer ibuf{};
                ibuf.handle = mesh.geometry.index_slice.buffer;

                const auto& offsets     = mesh.geometry.stream_offsets;
                GPUSize64   base_offset = mesh.geometry.vertex_slice.offset;
                GPUSize64   pos_offset  = base_offset + (offsets.has_position() ? offsets.position : 0);
                GPUSize64   norm_offset = base_offset + (offsets.has_normal() ? offsets.normal : (offsets.has_position() ? offsets.position : 0));
                GPUSize64   uv_offset   = base_offset + (offsets.has_texcoord0() ? offsets.texcoord0 : (offsets.has_position() ? offsets.position : 0));

                command.set_vertex_buffer(0, vbuf, pos_offset);
                command.set_vertex_buffer(1, vbuf, norm_offset);
                command.set_vertex_buffer(2, vbuf, uv_offset);
                command.set_index_buffer(ibuf, mesh.geometry.index_format, mesh.geometry.index_slice.offset);
                command.draw_indexed(mesh.index_count, 1, mesh.first_index, 0, 0);
            }

            command.end_render_pass();
        });
}
