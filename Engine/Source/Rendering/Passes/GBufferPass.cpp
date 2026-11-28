#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/GBufferPass.h>
#include <Lyra/Rendering/ViewUniforms.h>

using namespace lyra;
using namespace lyra::passes;

namespace
{
    /**
     * @brief Per-instance push constant layout matching Slang shader `InstanceData` in `sys://Shaders/Deferred/GBuffer.slang`.
     */
    struct alignas(16) InstanceData
    {
        Matrix4x4 model;                         // 64 bytes (offset 0)
        Vector4   base_color    = Vector4(1.0f); // 16 bytes (offset 64)
        Vector4   emissive      = Vector4(0.0f); // 16 bytes (offset 80)
        float     metallic      = 0.0f;          // 4 bytes  (offset 96)
        float     roughness     = 0.5f;          // 4 bytes  (offset 100)
        float     clear_coat    = 0.0f;          // 4 bytes  (offset 104)
        float     occlusion     = 1.0f;          // 4 bytes  (offset 108)
        uint      shading_model = 1;             // 4 bytes  (offset 112)
        uint      material_id   = 0;             // 4 bytes  (offset 116)
        uint      pad0          = 0;             // 4 bytes  (offset 120)
        uint      pad1          = 0;             // 4 bytes  (offset 124)
    };

    static_assert(sizeof(InstanceData) == 128, "InstanceData must be 128 bytes");
    static_assert(offsetof(InstanceData, model) == 0);
    static_assert(offsetof(InstanceData, base_color) == 64);
    static_assert(offsetof(InstanceData, emissive) == 80);
    static_assert(offsetof(InstanceData, metallic) == 96);
    static_assert(offsetof(InstanceData, roughness) == 100);
    static_assert(offsetof(InstanceData, clear_coat) == 104);
    static_assert(offsetof(InstanceData, occlusion) == 108);
    static_assert(offsetof(InstanceData, shading_model) == 112);
    static_assert(offsetof(InstanceData, material_id) == 116);

    uint to_g_buffer_shading_model(ShadingModel model)
    {
        switch (model) {
            case ShadingModel::UNLIT:
                return 0;
            case ShadingModel::STANDARD:
                return 1;
            case ShadingModel::CLEARCOAT:
                return 2;
            case ShadingModel::SUBSURFACE:
                return 3;
            case ShadingModel::CUSTOM:
                return 4;
            default:
                return 1;
        }
    }
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

    // compile and reflect G-Buffer shader asset
    PipelineShaderDescriptor shader_desc{};
    shader_desc.module         = "sys://Shaders/Deferred/GBuffer.slang";
    shader_desc.vertex_entry   = "vsmain";
    shader_desc.fragment_entry = "fsmain";



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
    Vector<GPUColorTargetState> rstates(4);
    // 0: BaseColor / AO
    rstates[0].format       = GPUTextureFormat::RGBA16UNORM;
    rstates[0].blend_enable = false;
    // 1: Normal / Roughness / Metallic
    rstates[1].format       = GPUTextureFormat::RGBA16SNORM;
    rstates[1].blend_enable = false;
    // 2: Emissive Radiance
    rstates[2].format       = GPUTextureFormat::RG11B10UFLOAT;
    rstates[2].blend_enable = false;
    // 3: Shading Model ID / Clearcoat
    rstates[3].format       = GPUTextureFormat::RG8UINT;
    rstates[3].blend_enable = false;

    pipeline = RenderPipelineBuilder(device, compiler)
                   .with_shader(shader_desc)
                   .with_vertex_layouts(vlayouts)
                   .with_cull_mode(GPUCullMode::BACK)
                   .with_topology(GPUPrimitiveTopology::TRIANGLE_LIST)
                   .with_front_face(GPUFrontFace::CCW)
                   .with_targets(rstates)
                   .with_depth_stencil(cached_depth_format, true, GPUCompareFunction::LESS_EQUAL)
                   .with_label("gbuffer_pipeline")
                   .build();
    if (!pipeline.valid()) {
        return false;
    }

    initialized = pipeline.valid();
    return initialized;
}

void GBufferPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    pipeline.destroy();
    initialized = false;
}

GBufferData GBufferPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    GPUExtent2D        extent)
{
    (void)view;

    return builder.add_pass<GBufferData>(
        "GBufferPass",
        [&](GBufferData& data, FrameGraphPassBuilder& pass_builder) {
            auto albedo_desc        = GPUTextureDescriptor{};
            albedo_desc.label       = "gbuffer_albedo_occlusion";
            albedo_desc.size.width  = extent.width;
            albedo_desc.size.height = extent.height;
            albedo_desc.size.depth  = 1;
            albedo_desc.dimension   = GPUTextureDimension::x2D;
            albedo_desc.format      = GPUTextureFormat::RGBA16UNORM;
            albedo_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.albedo_occlusion   = pass_builder.create<FrameGraphTexture>(albedo_desc);

            auto normal_desc               = GPUTextureDescriptor{};
            normal_desc.label              = "gbuffer_normal_roughness_metallic";
            normal_desc.size.width         = extent.width;
            normal_desc.size.height        = extent.height;
            normal_desc.size.depth         = 1;
            normal_desc.dimension          = GPUTextureDimension::x2D;
            normal_desc.format             = GPUTextureFormat::RGBA16SNORM;
            normal_desc.usage              = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.normal_roughness_metallic = pass_builder.create<FrameGraphTexture>(normal_desc);

            auto emissive_desc        = GPUTextureDescriptor{};
            emissive_desc.label       = "gbuffer_emissive";
            emissive_desc.size.width  = extent.width;
            emissive_desc.size.height = extent.height;
            emissive_desc.size.depth  = 1;
            emissive_desc.dimension   = GPUTextureDimension::x2D;
            emissive_desc.format      = GPUTextureFormat::RG11B10UFLOAT;
            emissive_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.emissive             = pass_builder.create<FrameGraphTexture>(emissive_desc);

            auto shading_model_desc        = GPUTextureDescriptor{};
            shading_model_desc.label       = "gbuffer_shading_model_clearcoat";
            shading_model_desc.size.width  = extent.width;
            shading_model_desc.size.height = extent.height;
            shading_model_desc.size.depth  = 1;
            shading_model_desc.dimension   = GPUTextureDimension::x2D;
            shading_model_desc.format      = GPUTextureFormat::RG8UINT;
            shading_model_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.shading_model_clearcoat   = pass_builder.create<FrameGraphTexture>(shading_model_desc);

            auto depth_desc        = GPUTextureDescriptor{};
            depth_desc.label       = "gbuffer_depth_stencil";
            depth_desc.size.width  = extent.width;
            depth_desc.size.height = extent.height;
            depth_desc.size.depth  = 1;
            depth_desc.dimension   = GPUTextureDimension::x2D;
            depth_desc.format      = cached_depth_format;
            depth_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.depth_stencil     = pass_builder.create<FrameGraphTexture>(depth_desc);

            data.albedo_occlusion          = pass_builder.render(data.albedo_occlusion);
            data.normal_roughness_metallic = pass_builder.render(data.normal_roughness_metallic);
            data.emissive                  = pass_builder.render(data.emissive);
            data.shading_model_clearcoat   = pass_builder.render(data.shading_model_clearcoat);
            data.depth_stencil             = pass_builder.render(data.depth_stencil);
        },
        [this, extent, &scene](const GBufferData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid()) {
                return;
            }

            auto albedo_tex        = resources.get<FrameGraphTexture>(data.albedo_occlusion);
            auto normal_tex        = resources.get<FrameGraphTexture>(data.normal_roughness_metallic);
            auto emissive_tex      = resources.get<FrameGraphTexture>(data.emissive);
            auto shading_model_tex = resources.get<FrameGraphTexture>(data.shading_model_clearcoat);
            auto depth_tex         = resources.get<FrameGraphTexture>(data.depth_stencil);
            if (!albedo_tex || !normal_tex || !emissive_tex || !shading_model_tex || !depth_tex) {
                return;
            }

            Vector<GPURenderPassColorAttachment> color_attachments(4);
            color_attachments[0].view        = albedo_tex->view;
            color_attachments[0].clear_value = GPUColor{scene.clear_color.x, scene.clear_color.y, scene.clear_color.z, 1.0f};
            color_attachments[0].load_op     = GPULoadOp::CLEAR;
            color_attachments[0].store_op    = GPUStoreOp::STORE;

            color_attachments[1].view        = normal_tex->view;
            color_attachments[1].clear_value = GPUColor{0.0f, 0.0f, 0.5f, 0.0f};
            color_attachments[1].load_op     = GPULoadOp::CLEAR;
            color_attachments[1].store_op    = GPUStoreOp::STORE;

            color_attachments[2].view        = emissive_tex->view;
            color_attachments[2].clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};
            color_attachments[2].load_op     = GPULoadOp::CLEAR;
            color_attachments[2].store_op    = GPUStoreOp::STORE;

            color_attachments[3].view        = shading_model_tex->view;
            color_attachments[3].clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};
            color_attachments[3].load_op     = GPULoadOp::CLEAR;
            color_attachments[3].store_op    = GPUStoreOp::STORE;

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
            const auto& blayouts = pipeline.layout.bind_group_layouts;
            if (ctx->view_uniforms && ctx->arena && !blayouts.empty()) {
                auto camera_bg = ctx->view_uniforms->bind_group(blayouts[0], *ctx->arena, ctx->device);
                if (camera_bg.valid()) {
                    command.set_bind_group(0, camera_bg);
                }
            }

            // rasterize all opaque scene meshes into G-Buffer
            for (const auto& mesh : scene.meshes) {
                if (!mesh.geometry.valid() || mesh.index_count == 0) {
                    continue;
                }

                InstanceData inst{};
                inst.model         = mesh.transform;
                inst.base_color    = mesh.material_params.base_color;
                inst.emissive      = mesh.material_params.emissive;
                inst.metallic      = mesh.material_params.metallic;
                inst.roughness     = mesh.material_params.roughness;
                inst.clear_coat    = mesh.material_params.clear_coat;
                inst.occlusion     = 1.0f;
                inst.shading_model = to_g_buffer_shading_model(mesh.shading_model);
                inst.material_id   = mesh.material_id;

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
