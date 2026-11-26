#include <cstring>

#include "SoftSampler.h"
#include "SoftPipeline.h"
#include "SoftShaderExecution.h"
#include "SoftCommandBuffer.h"

using namespace lyra;
using namespace lyra::softgpu;

void SoftCommandBuffer::reset()
{
    commands.clear();
}

void SoftCommandBuffer::begin_render_pass(const GPURenderPassDescriptor& desc)
{
    Vector<GPURenderPassColorAttachment> cas(desc.color_attachments.begin(), desc.color_attachments.end());

    auto ds = desc.depth_stencil_attachment;
    commands.push_back([cas = std::move(cas), ds](SoftDevice& device, ExecutionContext& ctx) {
        ctx.color_attachment = nullptr;
        ctx.color_attachments.clear();
        ctx.depth_stencil_attachment = nullptr;
        ctx.vertex_buffers.clear();
        ctx.index_buffer = {};

        for (const auto& ca : cas) {
            if (!ca.view.valid()) {
                ctx.color_attachments.push_back(nullptr);
                continue;
            }
            auto view = device.texture_views.find(ca.view.to_slotmap_handle<SoftTextureView>());
            if (!view) {
                ctx.color_attachments.push_back(nullptr);
                continue;
            }
            auto tex = device.textures.find(view->texture.to_slotmap_handle<SoftTexture>());
            if (!tex) {
                ctx.color_attachments.push_back(nullptr);
                continue;
            }

            if (ca.load_op == GPULoadOp::CLEAR) {
                tex->clear_color(ca.clear_value, view->base_mip_level, view->base_array_layer);
            }
            ctx.color_attachments.push_back(tex);
            if (!ctx.color_attachment) {
                ctx.color_attachment = tex;
            }
        }

        if (ds.view.valid()) {
            auto view = device.texture_views.find(ds.view.to_slotmap_handle<SoftTextureView>());
            if (view) {
                auto tex = device.textures.find(view->texture.to_slotmap_handle<SoftTexture>());
                if (tex) {
                    bool clear_d = (ds.depth_load_op == GPULoadOp::CLEAR);
                    bool clear_s = (ds.stencil_load_op == GPULoadOp::CLEAR);
                    if (clear_d || clear_s) {
                        tex->clear_depth_stencil(ds.depth_clear_value,
                            ds.stencil_clear_value,
                            clear_d, clear_s);
                    }
                    ctx.depth_stencil_attachment = tex;
                }
            }
        }
    });
}

void SoftCommandBuffer::end_render_pass()
{
    commands.push_back([](SoftDevice&, ExecutionContext& ctx) {
        ctx.color_attachment = nullptr;
        ctx.color_attachments.clear();
        ctx.depth_stencil_attachment = nullptr;
    });
}

void SoftCommandBuffer::set_viewport(float x, float y, float w, float h, float min_d, float max_d)
{
    commands.push_back([=](SoftDevice&, ExecutionContext& ctx) {
        ctx.viewport = SoftViewport{x, y, w, h, min_d, max_d};
    });
}

void SoftCommandBuffer::set_scissor(int32_t x, int32_t y, int32_t w, int32_t h)
{
    commands.push_back([=](SoftDevice&, ExecutionContext& ctx) {
        ctx.scissor = SoftScissor{x, y, w, h};
    });
}

void SoftCommandBuffer::set_render_pipeline(GPURenderPipelineHandle pipeline)
{
    commands.push_back([pipeline](SoftDevice& device, ExecutionContext& ctx) {
        ctx.render_pipeline = device.render_pipelines.find(pipeline.to_slotmap_handle<SoftRenderPipeline>());
    });
}

void SoftCommandBuffer::set_compute_pipeline(GPUComputePipelineHandle pipeline)
{
    commands.push_back([pipeline](SoftDevice& device, ExecutionContext& ctx) {
        ctx.compute_pipeline = device.compute_pipelines.find(pipeline.to_slotmap_handle<SoftComputePipeline>());
    });
}

void SoftCommandBuffer::set_raytracing_pipeline(GPURayTracingPipelineHandle pipeline)
{
    commands.push_back([pipeline](SoftDevice&, ExecutionContext&) {
        // raytracing pipeline marker
    });
}

void SoftCommandBuffer::set_bind_group(uint index, GPUBindGroupHandle bind_group, TypedView<uint> dynamic_offsets)
{
    Vector<uint> offsets(dynamic_offsets.begin(), dynamic_offsets.end());
    commands.push_back([index, bind_group, offsets = std::move(offsets)](SoftDevice&, ExecutionContext& ctx) {
        ctx.bind_groups[index] = SoftBoundBindGroup{bind_group, std::move(offsets)};
    });
}

void SoftCommandBuffer::set_stencil_reference(uint reference)
{
    commands.push_back([reference](SoftDevice&, ExecutionContext& ctx) {
        ctx.stencil_reference = reference;
    });
}

void SoftCommandBuffer::set_immediates(uint offset, uint size, const void* data)
{
    if (!data || size == 0) return;
    Vector<ubyte> bytes(static_cast<const ubyte*>(data), static_cast<const ubyte*>(data) + size);
    commands.push_back([offset, bytes = std::move(bytes)](SoftDevice&, ExecutionContext& ctx) {
        if (offset + bytes.size() > ctx.immediates_data.size()) {
            ctx.immediates_data.resize(offset + bytes.size());
        }
        std::memcpy(ctx.immediates_data.data() + offset, bytes.data(), bytes.size());
    });
}

void SoftCommandBuffer::set_vertex_buffer(uint slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    commands.push_back([slot, buffer, offset, size](SoftDevice&, ExecutionContext& ctx) {
        ctx.vertex_buffers[slot] = SoftBoundVertexBuffer{buffer, offset, size};
    });
}

void SoftCommandBuffer::set_index_buffer(GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size)
{
    commands.push_back([buffer, format, offset, size](SoftDevice&, ExecutionContext& ctx) {
        ctx.index_buffer = SoftBoundIndexBuffer{buffer, format, offset, size};
    });
}

struct SoftDrawParams
{
    Matrix4x4             vp_matrix{1.0f};
    bool                  has_vp = false;
    SoftRasterizerContext r_ctx{};
    usize                 stride     = 24;
    usize                 pos_offset = 0;
    usize                 col_offset = 12;
    usize                 uv_offset  = 12;
    GPUVertexFormat       pos_format = GPUVertexFormat::FLOAT32x3;
    GPUVertexFormat       col_format = GPUVertexFormat::FLOAT32x3;
    bool                  has_col    = false;
    bool                  has_uv     = false;
    const SoftTexture*    bound_tex  = nullptr;
    alignas(16) Array<ubyte, 512> uniform_bytes{};
    alignas(16) Array<ubyte, 512> pb_bytes[8]{};
    Array<SoftHostTextureWrapper, 16> host_textures{};
    uint                              host_tex_count = 0;
    Vector<SoftVertexAttribute>       vb_attrs;
};

static void setup_draw_params(SoftDevice& device, const ExecutionContext& ctx, SoftDrawParams& p)
{
    p.uniform_bytes.fill(0);
    for (auto& pb : p.pb_bytes)
        pb.fill(0);

    p.r_ctx.color_attachment         = ctx.color_attachment;
    p.r_ctx.color_attachments        = ctx.color_attachments;
    p.r_ctx.depth_stencil_attachment = ctx.depth_stencil_attachment;
    p.r_ctx.viewport                 = ctx.viewport;
    p.r_ctx.scissor                  = ctx.scissor;
    p.r_ctx.stencil_reference        = ctx.stencil_reference;

    if (ctx.render_pipeline) {
        p.r_ctx.primitive     = ctx.render_pipeline->primitive;
        p.r_ctx.depth_stencil = ctx.render_pipeline->depth_stencil;
        p.r_ctx.color_targets = ctx.render_pipeline->color_targets;
        if (!ctx.render_pipeline->color_targets.empty()) {
            p.r_ctx.color_target = ctx.render_pipeline->color_targets[0];
        }
    }

    // 1. check bind groups (texture, sampler, uniform buffer)
    const SoftTextureView* bound_view = nullptr;
    const SoftSampler*     bound_samp = nullptr;

    for (const auto& [slot, bound_bg] : ctx.bind_groups) {
        if (!bound_bg.handle.valid()) continue;
        auto bg = device.bind_groups.find(bound_bg.handle.to_slotmap_handle<SoftBindGroup>());
        if (!bg) continue;

        for (const auto& entry : bg->entries) {
            if (entry.type == GPUResourceType::BUFFER && entry.buffer.buffer.valid()) {
                if (slot == 0 && entry.binding == 0 && !p.has_vp) {
                    auto ubuf = device.buffers.find(entry.buffer.buffer.to_slotmap_handle<SoftBuffer>());
                    if (ubuf) {
                        usize dyn_off = bound_bg.dynamic_offsets.empty() ? 0 : bound_bg.dynamic_offsets[0];
                        usize eff_off = entry.buffer.offset + dyn_off;
                        if (ubuf->data.size() >= eff_off + sizeof(Matrix4x4) * 2 &&
                            bound_bg.dynamic_offsets.empty() &&
                            reinterpret_cast<const Matrix4x4*>(ubuf->data.data() + eff_off + sizeof(Matrix4x4))->operator[](3)[3] != 0.0f) {
                            const auto mats = reinterpret_cast<const Matrix4x4*>(ubuf->data.data() + eff_off);
                            p.vp_matrix     = mats[0] * mats[1];
                            p.has_vp        = true;
                        } else if (ubuf->data.size() >= eff_off + sizeof(Matrix4x4)) {
                            p.vp_matrix = *reinterpret_cast<const Matrix4x4*>(ubuf->data.data() + eff_off);
                            p.has_vp    = true;
                        }
                    }
                }
            } else if (entry.type == GPUResourceType::TEXTURE && entry.texture.valid()) {
                auto v = device.texture_views.find(entry.texture.to_slotmap_handle<SoftTextureView>());
                if (v) {
                    bound_view  = v;
                    p.bound_tex = device.textures.find(v->texture.to_slotmap_handle<SoftTexture>());
                }
            } else if (entry.type == GPUResourceType::SAMPLER && entry.sampler.valid()) {
                bound_samp = device.samplers.find(entry.sampler.to_slotmap_handle<SoftSampler>());
            }
        }
    }

    // 2. matrix from immediates (fallback if no camera uniform buffer was bound)
    if (!p.has_vp && !ctx.immediates_data.empty() && ctx.immediates_data.size() >= sizeof(Matrix4x4)) {
        p.vp_matrix = *reinterpret_cast<const Matrix4x4*>(ctx.immediates_data.data());
        p.has_vp    = true;
    }

    auto populate_params = [&](GPUShaderModuleHandle sm_handle) {
        if (!sm_handle.valid()) return;
        auto* sm = device.shader_modules.find(sm_handle.to_slotmap_handle<SoftShaderModule>());
        if (!sm || sm->param_count == 0) return;

        for (uint i = 0; i < sm->param_count; ++i) {
            const auto& param = sm->params[i];

            ubyte* target_buf      = nullptr;
            usize  target_buf_size = 0;

            if (param.block_offset != 0xFFFF) {
                if (param.set >= 8) continue;
                target_buf      = p.pb_bytes[param.set].data();
                target_buf_size = p.pb_bytes[param.set].size();

                if (param.block_offset + sizeof(void*) <= p.uniform_bytes.size()) {
                    const void* pb_ptr = target_buf;
                    std::memcpy(p.uniform_bytes.data() + param.block_offset, &pb_ptr, sizeof(void*));
                }
            } else {
                target_buf      = p.uniform_bytes.data();
                target_buf_size = p.uniform_bytes.size();
            }

            if (param.offset + sizeof(void*) > target_buf_size) continue;

            if (param.is_immediates) {
                if (!ctx.immediates_data.empty()) {
                    const void* ptr = ctx.immediates_data.data();
                    std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                }
            } else {
                auto it_bg = ctx.bind_groups.find(param.set);
                if (it_bg == ctx.bind_groups.end() || !it_bg->second.handle.valid()) continue;
                auto bg = device.bind_groups.find(it_bg->second.handle.to_slotmap_handle<SoftBindGroup>());
                if (!bg) continue;

                for (const auto& entry : bg->entries) {
                    if (entry.binding != param.binding) continue;

                    if (entry.type == GPUResourceType::BUFFER && entry.buffer.buffer.valid()) {
                        auto ubuf = device.buffers.find(entry.buffer.buffer.to_slotmap_handle<SoftBuffer>());
                        if (ubuf) {
                            usize       dyn_off = it_bg->second.dynamic_offsets.empty() ? 0 : it_bg->second.dynamic_offsets[0];
                            usize       eff_off = entry.buffer.offset + dyn_off;
                            const void* ptr     = ubuf->data.data() + eff_off;
                            std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                            if (param.size >= 16) {
                                usize buf_size = (ubuf->data.size() > eff_off) ? (ubuf->data.size() - eff_off) : 0;
                                std::memcpy(target_buf + param.offset + 8, &buf_size, sizeof(usize));
                            }
                        }
                    } else if (entry.type == GPUResourceType::TEXTURE && entry.texture.valid()) {
                        auto v = device.texture_views.find(entry.texture.to_slotmap_handle<SoftTextureView>());
                        if (v) {
                            auto t = device.textures.find(v->texture.to_slotmap_handle<SoftTexture>());
                            if (t && p.host_tex_count < p.host_textures.size()) {
                                uint tex_idx             = p.host_tex_count++;
                                p.host_textures[tex_idx] = SoftHostTextureWrapper(t, v);
                                const void* ptr          = &p.host_textures[tex_idx];
                                std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                            }
                        }
                    } else if (entry.type == GPUResourceType::SAMPLER && entry.sampler.valid()) {
                        auto s = device.samplers.find(entry.sampler.to_slotmap_handle<SoftSampler>());
                        if (s) {
                            const void* ptr = s;
                            std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                        }
                    }
                    break;
                }
            }
        }
    };

    if (ctx.render_pipeline) {
        populate_params(ctx.render_pipeline->vshader_module);
        populate_params(ctx.render_pipeline->fshader_module);
    }

    bool dual_sample = false;
    if (ctx.render_pipeline && ctx.render_pipeline->fshader_module.valid()) {
        auto dev = &device;
        auto sm  = dev->shader_modules.find(ctx.render_pipeline->fshader_module.to_slotmap_handle<SoftShaderModule>());
        if (sm && sm->bytecode.size() >= 20) {
            usize       sample_count = 0;
            const uint* spv          = reinterpret_cast<const uint*>(sm->bytecode.data());
            usize       spv_len      = sm->bytecode.size() / 4;
            for (usize i = 5; i < spv_len;) {
                uint   word = spv[i];
                ushort op   = word & 0xFFFF;
                ushort len  = (word >> 16) & 0xFFFF;
                if (op == 87) { // OpImageSampleImplicitLod
                    sample_count++;
                }
                if (len == 0) break;
                i += len;
            }
            if (sample_count >= 2) {
                dual_sample = true;
            }
        }
    }

    p.r_ctx.fshader_func  = ctx.render_pipeline ? ctx.render_pipeline->fshader_func : nullptr;
    p.r_ctx.depth_offset  = ctx.render_pipeline ? ctx.render_pipeline->depth_offset : 0xFFFF;
    p.r_ctx.uniform_state = p.uniform_bytes.data();

    if (!p.r_ctx.fshader_func && p.bound_tex && bound_view && bound_samp) {
        if (dual_sample) {
            p.r_ctx.fragment_shader = [t = p.bound_tex, v = bound_view, s = bound_samp](const SoftShadedVertex& vert) -> Vector4 {
                Vector4 c0  = s->sample_2d(*t, *v, vert.texcoord.x, vert.texcoord.y);
                Vector4 c1  = s->sample_2d(*t, *v, 1.0f - vert.texcoord.x, 1.0f - vert.texcoord.y);
                Vector3 rgb = Vector3(c0) + Vector3(c1);
                float   a   = (glm::length(rgb) > 0.0f) ? 1.0f : 0.0f;
                return Vector4(rgb, a);
            };
        } else {
            p.r_ctx.fragment_shader = [t = p.bound_tex, v = bound_view, s = bound_samp](const SoftShadedVertex& vert) -> Vector4 {
                return s->sample_2d(*t, *v, vert.texcoord.x, vert.texcoord.y) * vert.color;
            };
        }
    }

    // vertex layout
    p.stride     = 24;
    p.pos_offset = 0;
    p.col_offset = 12;
    p.uv_offset  = 12;

    if (ctx.render_pipeline && !ctx.render_pipeline->vertex_buffers.empty()) {
        const auto& vb = ctx.render_pipeline->vertex_buffers[0];
        p.stride       = vb.array_stride > 0 ? vb.array_stride : 24;
        p.vb_attrs.assign(vb.attributes.begin(), vb.attributes.end());
        for (const auto& attr : vb.attributes) {
            if (attr.shader_location == 0) {
                p.pos_offset = static_cast<usize>(attr.offset);
                p.pos_format = attr.format;
            } else if (attr.shader_location == 1) {
                if (attr.format == GPUVertexFormat::FLOAT32x2) {
                    p.uv_offset = static_cast<usize>(attr.offset);
                    p.has_uv    = true;
                } else {
                    p.col_offset = static_cast<usize>(attr.offset);
                    p.col_format = attr.format;
                    p.has_col    = true;
                }
            } else if (attr.shader_location == 2) {
                p.col_offset = static_cast<usize>(attr.offset);
                p.col_format = attr.format;
                p.has_col    = true;
            }
        }
    }

    if (p.stride == 44) {
        if (!p.has_col && p.bound_tex == nullptr) p.col_offset = 24;
        if (!p.has_uv) p.uv_offset = 36;
    }
}

struct SoftDrawIndirectCommand
{
    uint vertex_count;
    uint instance_count;
    uint first_vertex;
    uint first_instance;
};

struct SoftDrawIndexedIndirectCommand
{
    uint    index_count;
    uint    instance_count;
    uint    first_index;
    int32_t base_vertex;
    uint    first_instance;
};

static void execute_draw(SoftDevice& device, ExecutionContext& ctx, uint vertex_count, uint instance_count, uint first_vertex, uint first_instance)
{
    if (instance_count == 0 || vertex_count == 0) return;

    SoftDrawParams p{};
    setup_draw_params(device, ctx, p);

    auto run_vs = [&](uint vid, SoftShadedVertex& sv, const void* vptr) {
        if (ctx.render_pipeline && ctx.render_pipeline->vshader_func) {
            ShadedVertexOutput vs_out{};
            uint               v_idx = vid;
            invoke_vertex_shader(ctx.render_pipeline->vshader_func, vptr ? vptr : &v_idx, vs_out, p.r_ctx.uniform_state);

            sv.position = vs_out.position;
            std::memcpy(sv.varyings, vs_out.varyings, sizeof(sv.varyings));
            std::memcpy(&sv.texcoord, &vs_out.varyings[0], sizeof(Vector4));
            std::memcpy(&sv.normal, &vs_out.varyings[0], sizeof(Vector4));
        }
    };

    auto it_v     = ctx.vertex_buffers.find(0);
    bool has_vbuf = (it_v != ctx.vertex_buffers.end()) && ctx.render_pipeline && !ctx.render_pipeline->vertex_buffers.empty();
    if (!has_vbuf) {
        if (ctx.render_pipeline && ctx.render_pipeline->vshader_func) {
            for (uint inst = 0; inst < instance_count; ++inst) {
                uint num_tris = vertex_count / 3;
                for (uint t = 0; t < num_tris; ++t) {
                    SoftShadedVertex v0{}, v1{}, v2{};
                    v0.color = v1.color = v2.color = Vector4(1.0f);
                    run_vs(first_vertex + t * 3 + 0, v0, nullptr);
                    run_vs(first_vertex + t * 3 + 1, v1, nullptr);
                    run_vs(first_vertex + t * 3 + 2, v2, nullptr);

                    Vector<SoftRasterTriangle> tris;
                    SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
                    for (const auto& tri : tris) {
                        SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
                    }
                }
            }
        } else if (vertex_count == 3) {
            SoftShadedVertex v0{}, v1{}, v2{};
            v0.position = Vector4(-1.0f, -1.0f, 0.0f, 1.0f);
            v0.texcoord = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            v0.color    = Vector4(1.0f);

            v1.position = Vector4(3.0f, -1.0f, 0.0f, 1.0f);
            v1.texcoord = Vector4(2.0f, 0.0f, 0.0f, 0.0f);
            v1.color    = Vector4(1.0f);

            v2.position = Vector4(-1.0f, 3.0f, 0.0f, 1.0f);
            v2.texcoord = Vector4(0.0f, 2.0f, 0.0f, 0.0f);
            v2.color    = Vector4(1.0f);

            Vector<SoftRasterTriangle> tris;
            SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
            for (const auto& tri : tris) {
                SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
            }
        } else if (vertex_count >= 4) {
            SoftShadedVertex v[4];
            v[0].position = Vector4(-1.0f, -1.0f, 0.0f, 1.0f);
            v[0].texcoord = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            v[1].position = Vector4(1.0f, -1.0f, 0.0f, 1.0f);
            v[1].texcoord = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
            v[2].position = Vector4(1.0f, 1.0f, 0.0f, 1.0f);
            v[2].texcoord = Vector4(1.0f, 1.0f, 0.0f, 0.0f);
            v[3].position = Vector4(-1.0f, 1.0f, 0.0f, 1.0f);
            v[3].texcoord = Vector4(0.0f, 1.0f, 0.0f, 0.0f);
            for (int i = 0; i < 4; ++i)
                v[i].color = Vector4(1.0f);

            Vector<SoftRasterTriangle> tris;
            SoftRasterizer::clip_and_project_triangle(v[0], v[1], v[2], ctx.viewport, tris);
            for (const auto& tri : tris)
                SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
            tris.clear();
            SoftRasterizer::clip_and_project_triangle(v[0], v[2], v[3], ctx.viewport, tris);
            for (const auto& tri : tris)
                SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
        }
        return;
    }

    auto vbuf = device.buffers.find(it_v->second.buffer.to_slotmap_handle<SoftBuffer>());
    if (!vbuf) return;

    const ubyte* vdata = vbuf->data.data() + it_v->second.offset;

    struct StreamAttr
    {
        const ubyte*    base_ptr        = nullptr;
        GPUSize64       stride          = 0;
        GPUSize64       src_offset      = 0;
        GPUSize64       dst_offset      = 0;
        GPUSize64       size            = 0;
        GPUSize64       dst_size        = 0;
        GPUSize64       buf_size        = 0;
        uint            shader_location = 0;
        GPUVertexFormat format          = GPUVertexFormat::FLOAT32x4;
        bool            is_instance     = false;
    };
    SmallVector<StreamAttr, 8> stream_attrs;
    bool                       has_multi_stream_vs = false;

    if (ctx.render_pipeline && ctx.render_pipeline->vshader_func && !ctx.render_pipeline->vertex_buffers.empty()) {
        for (usize s = 0; s < ctx.render_pipeline->vertex_buffers.size(); ++s) {
            const auto& vb   = ctx.render_pipeline->vertex_buffers[s];
            auto        it_s = ctx.vertex_buffers.find(static_cast<uint>(s));
            if (it_s == ctx.vertex_buffers.end()) continue;
            auto buf = device.buffers.find(it_s->second.buffer.to_slotmap_handle<SoftBuffer>());
            if (!buf || it_s->second.offset > buf->data.size()) continue;

            const ubyte* s_ptr      = buf->data.data() + it_s->second.offset;
            GPUSize64    avail_size = buf->data.size() - it_s->second.offset;

            for (const auto& attr : vb.attributes) {
                StreamAttr sa{};
                sa.base_ptr   = s_ptr;
                sa.stride     = vb.array_stride;
                sa.src_offset = attr.offset;
                sa.format     = attr.format;
                sa.size       = get_vertex_format_size(attr.format);
                if (attr.format == GPUVertexFormat::UNORM8x4 || attr.format == GPUVertexFormat::SNORM8x4 ||
                    attr.format == GPUVertexFormat::UNORM8x4_BGRA) {
                    sa.dst_size = sizeof(float) * 4;
                } else if (attr.format == GPUVertexFormat::UNORM8x2 || attr.format == GPUVertexFormat::SNORM8x2) {
                    sa.dst_size = sizeof(float) * 2;
                } else if (attr.format == GPUVertexFormat::UNORM8 || attr.format == GPUVertexFormat::SNORM8) {
                    sa.dst_size = sizeof(float);
                } else {
                    sa.dst_size = sa.size;
                }
                sa.buf_size        = avail_size;
                sa.shader_location = attr.shader_location;
                sa.is_instance     = (vb.step_mode == GPUVertexStepMode::INSTANCE);
                stream_attrs.push_back(sa);
            }
        }
        if (!stream_attrs.empty()) {
            has_multi_stream_vs = true;
            std::sort(stream_attrs.begin(), stream_attrs.end(), [](const StreamAttr& a, const StreamAttr& b) {
                return a.shader_location < b.shader_location;
            });
            GPUSize64 cur_dst = 0;
            for (auto& sa : stream_attrs) {
                sa.dst_offset = cur_dst;
                cur_dst += sa.dst_size;
            }
        }
    }

    auto read_vert = [&](uint idx, uint current_inst) -> SoftShadedVertex {
        SoftShadedVertex sv{};
        const ubyte*     ptr = vdata + (first_vertex + idx) * p.stride;
        if (ctx.render_pipeline && ctx.render_pipeline->vshader_func) {
            if (has_multi_stream_vs) {
                alignas(16) ubyte assembled_v[256] = {};
                uint              vert_idx         = first_vertex + idx;
                for (const auto& sa : stream_attrs) {
                    uint      step_idx = sa.is_instance ? current_inst : vert_idx;
                    GPUSize64 offset   = static_cast<GPUSize64>(step_idx) * sa.stride + sa.src_offset;
                    if (offset + sa.size <= sa.buf_size) {
                        if (sa.format == GPUVertexFormat::UNORM8x4) {
                            const ubyte* col     = sa.base_ptr + offset;
                            float        rgba[4] = {
                                col[0] / 255.0f,
                                col[1] / 255.0f,
                                col[2] / 255.0f,
                                col[3] / 255.0f,
                            };
                            std::memcpy(assembled_v + sa.dst_offset, rgba, sizeof(rgba));
                        } else if (sa.format == GPUVertexFormat::UNORM8x4_BGRA) {
                            const ubyte* col     = sa.base_ptr + offset;
                            float        rgba[4] = {
                                col[2] / 255.0f,
                                col[1] / 255.0f,
                                col[0] / 255.0f,
                                col[3] / 255.0f,
                            };
                            std::memcpy(assembled_v + sa.dst_offset, rgba, sizeof(rgba));
                        } else {
                            std::memcpy(assembled_v + sa.dst_offset, sa.base_ptr + offset, sa.size);
                        }
                    }
                }
                run_vs(vert_idx, sv, assembled_v);
            } else {
                run_vs(first_vertex + idx, sv, ptr);
            }
            return sv;
        }
        Vector4 local_pos(0.0f, 0.0f, 0.0f, 1.0f);
        if (p.pos_format == GPUVertexFormat::FLOAT32x2) {
            const auto pos = reinterpret_cast<const Vector2*>(ptr + p.pos_offset);
            local_pos.x    = pos->x;
            local_pos.y    = pos->y;
        } else {
            const auto pos = reinterpret_cast<const Vector3*>(ptr + p.pos_offset);
            local_pos.x    = pos->x;
            local_pos.y    = pos->y;
            local_pos.z    = pos->z;
        }

        if (p.has_uv || p.bound_tex != nullptr) {
            const auto uv = reinterpret_cast<const Vector2*>(ptr + p.uv_offset);
            sv.texcoord   = Vector4(uv->x, uv->y, 0.0f, 0.0f);
        }
        if (p.has_col) {
            if (p.col_format == GPUVertexFormat::UNORM8x4) {
                const auto col = ptr + p.col_offset;
                sv.color       = Vector4(col[0] / 255.0f, col[1] / 255.0f, col[2] / 255.0f, col[3] / 255.0f);
            } else if (p.col_format == GPUVertexFormat::FLOAT32x4) {
                const auto col = reinterpret_cast<const Vector4*>(ptr + p.col_offset);
                sv.color       = *col;
            } else {
                const auto col = reinterpret_cast<const Vector3*>(ptr + p.col_offset);
                sv.color       = Vector4(col->x, col->y, col->z, 1.0f);
            }
        } else if (p.col_offset + sizeof(Vector3) <= p.stride && p.bound_tex == nullptr) {
            const auto col = reinterpret_cast<const Vector3*>(ptr + p.col_offset);
            sv.color       = Vector4(col->x, col->y, col->z, 1.0f);
        } else {
            sv.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        std::memcpy(&sv.varyings[0], &sv.normal, sizeof(float) * 3);
        std::memcpy(&sv.varyings[3], &sv.texcoord, sizeof(float) * 2);
        for (const auto& attr : p.vb_attrs) {
            if (attr.shader_location >= 1 && attr.shader_location <= 4) {
                uint loc = attr.shader_location - 1;
                if (attr.format == GPUVertexFormat::FLOAT32x4 && attr.offset + 16 <= p.stride) {
                    std::memcpy(&sv.varyings[loc * 4], ptr + attr.offset, 16);
                    if (loc == 0) {
                        std::memcpy(&sv.color, ptr + attr.offset, 16);
                    }
                }
            }
        }

        // secondary vertex buffer streams / instance streams
        if (ctx.render_pipeline && ctx.render_pipeline->vertex_buffers.size() > 1) {
            for (usize s = 1; s < ctx.render_pipeline->vertex_buffers.size(); ++s) {
                const auto& vb_layout = ctx.render_pipeline->vertex_buffers[s];
                auto        it_slot   = ctx.vertex_buffers.find(static_cast<uint>(s));
                if (it_slot == ctx.vertex_buffers.end()) continue;
                auto sbuf = device.buffers.find(it_slot->second.buffer.to_slotmap_handle<SoftBuffer>());
                if (!sbuf) continue;

                uint      step_idx    = (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) ? current_inst : (first_vertex + idx);
                GPUSize64 elem_offset = it_slot->second.offset + static_cast<GPUSize64>(step_idx) * vb_layout.array_stride;
                if (elem_offset + vb_layout.array_stride > sbuf->data.size()) continue;
                const ubyte* elem = sbuf->data.data() + elem_offset;

                for (const auto& attr : vb_layout.attributes) {
                    if (attr.offset + 8 <= vb_layout.array_stride && attr.format == GPUVertexFormat::FLOAT32x2) {
                        const auto v2 = reinterpret_cast<const Vector2*>(elem + attr.offset);
                        if (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) {
                            local_pos.x += v2->x;
                            local_pos.y += v2->y;
                        } else {
                            sv.texcoord = Vector4(v2->x, v2->y, 0.0f, 0.0f);
                        }
                    } else if (attr.offset + 12 <= vb_layout.array_stride && attr.format == GPUVertexFormat::FLOAT32x3) {
                        const auto v3 = reinterpret_cast<const Vector3*>(elem + attr.offset);
                        if (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) {
                            local_pos.x += v3->x;
                            local_pos.y += v3->y;
                            local_pos.z += v3->z;
                        } else {
                            sv.color = Vector4(v3->x, v3->y, v3->z, 1.0f);
                        }
                    } else if (attr.offset + 16 <= vb_layout.array_stride && attr.format == GPUVertexFormat::FLOAT32x4) {
                        const auto v4 = reinterpret_cast<const Vector4*>(elem + attr.offset);
                        sv.color      = *v4;
                        if (attr.shader_location >= 1 && attr.shader_location <= 4) {
                            uint loc = attr.shader_location - 1;
                            std::memcpy(&sv.varyings[loc * 4], v4, 16);
                        }
                    }
                }
            }
        }

        sv.position = p.has_vp ? (p.vp_matrix * local_pos) : local_pos;
        return sv;
    };

    bool is_strip = (p.r_ctx.primitive.topology == GPUPrimitiveTopology::TRIANGLE_STRIP);
    uint num_tris = is_strip ? (vertex_count >= 3 ? vertex_count - 2 : 0) : (vertex_count / 3);

    for (uint inst = 0; inst < instance_count; ++inst) {
        uint current_inst = first_instance + inst;
        for (uint t = 0; t < num_tris; ++t) {
            uint off0, off1, off2;
            if (is_strip) {
                if (t % 2 == 0) {
                    off0 = t;
                    off1 = t + 1;
                    off2 = t + 2;
                } else {
                    off0 = t + 1;
                    off1 = t;
                    off2 = t + 2;
                }
            } else {
                off0 = t * 3 + 0;
                off1 = t * 3 + 1;
                off2 = t * 3 + 2;
            }

            SoftShadedVertex v0 = read_vert(off0, current_inst);
            SoftShadedVertex v1 = read_vert(off1, current_inst);
            SoftShadedVertex v2 = read_vert(off2, current_inst);

            Vector<SoftRasterTriangle> tris;
            SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
            for (const auto& tri : tris) {
                SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
            }
        }
    }
}

static void execute_draw_indexed(SoftDevice& device, ExecutionContext& ctx, uint index_count, uint instance_count, uint first_index, int32_t base_vertex, uint first_instance)
{
    if (instance_count == 0 || index_count == 0) return;
    if (!ctx.color_attachment && ctx.color_attachments.empty() && !ctx.depth_stencil_attachment) return;

    auto it_v0 = ctx.vertex_buffers.find(0);
    auto it_v1 = ctx.vertex_buffers.find(1);
    auto it_v2 = ctx.vertex_buffers.find(2);

    if (it_v0 == ctx.vertex_buffers.end() && (!ctx.render_pipeline || !ctx.render_pipeline->vshader_func)) return;
    auto vbuf = (it_v0 != ctx.vertex_buffers.end()) ? device.buffers.find(it_v0->second.buffer.to_slotmap_handle<SoftBuffer>()) : nullptr;
    if (it_v0 != ctx.vertex_buffers.end() && !vbuf) return;

    auto ibuf = device.buffers.find(ctx.index_buffer.buffer.to_slotmap_handle<SoftBuffer>());
    if (!ibuf) return;

    SoftDrawParams p{};
    setup_draw_params(device, ctx, p);

    auto run_vs = [&](uint vid, SoftShadedVertex& sv, const void* vptr) {
        if (ctx.render_pipeline && ctx.render_pipeline->vshader_func) {
            ShadedVertexOutput vs_out{};
            uint               v_idx = vid;
            invoke_vertex_shader(ctx.render_pipeline->vshader_func, vptr ? vptr : &v_idx, vs_out, p.r_ctx.uniform_state);

            sv.position = vs_out.position;
            std::memcpy(sv.varyings, vs_out.varyings, sizeof(sv.varyings));
            sv.color = Vector4(1.0f);
        }
    };

    const ubyte* vdata = vbuf ? (vbuf->data.data() + it_v0->second.offset) : nullptr;
    const ubyte* idata = ibuf->data.data() + ctx.index_buffer.offset;

    const ubyte* vdata1 = nullptr;
    if (it_v1 != ctx.vertex_buffers.end()) {
        auto vbuf1 = device.buffers.find(it_v1->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (vbuf1) vdata1 = vbuf1->data.data() + it_v1->second.offset;
    }
    const ubyte* vdata2 = nullptr;
    if (it_v2 != ctx.vertex_buffers.end()) {
        auto vbuf2 = device.buffers.find(it_v2->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (vbuf2) vdata2 = vbuf2->data.data() + it_v2->second.offset;
    }

    struct StreamAttr
    {
        const ubyte*    base_ptr        = nullptr;
        GPUSize64       stride          = 0;
        GPUSize64       src_offset      = 0;
        GPUSize64       dst_offset      = 0;
        GPUSize64       size            = 0;
        GPUSize64       dst_size        = 0;
        GPUSize64       buf_size        = 0;
        uint            shader_location = 0;
        GPUVertexFormat format          = GPUVertexFormat::FLOAT32x4;
        bool            is_instance     = false;
    };
    SmallVector<StreamAttr, 8> stream_attrs;
    bool                       has_multi_stream_vs = false;

    if (ctx.render_pipeline && ctx.render_pipeline->vshader_func && !ctx.render_pipeline->vertex_buffers.empty()) {
        for (usize s = 0; s < ctx.render_pipeline->vertex_buffers.size(); ++s) {
            const auto& vb   = ctx.render_pipeline->vertex_buffers[s];
            auto        it_s = ctx.vertex_buffers.find(static_cast<uint>(s));
            if (it_s == ctx.vertex_buffers.end()) continue;
            auto buf = device.buffers.find(it_s->second.buffer.to_slotmap_handle<SoftBuffer>());
            if (!buf || it_s->second.offset > buf->data.size()) continue;

            const ubyte* s_ptr      = buf->data.data() + it_s->second.offset;
            GPUSize64    avail_size = buf->data.size() - it_s->second.offset;

            for (const auto& attr : vb.attributes) {
                StreamAttr sa{};
                sa.base_ptr   = s_ptr;
                sa.stride     = vb.array_stride;
                sa.src_offset = attr.offset;
                sa.format     = attr.format;
                sa.size       = get_vertex_format_size(attr.format);
                if (attr.format == GPUVertexFormat::UNORM8x4 || attr.format == GPUVertexFormat::SNORM8x4 ||
                    attr.format == GPUVertexFormat::UNORM8x4_BGRA) {
                    sa.dst_size = sizeof(float) * 4;
                } else if (attr.format == GPUVertexFormat::UNORM8x2 || attr.format == GPUVertexFormat::SNORM8x2) {
                    sa.dst_size = sizeof(float) * 2;
                } else if (attr.format == GPUVertexFormat::UNORM8 || attr.format == GPUVertexFormat::SNORM8) {
                    sa.dst_size = sizeof(float);
                } else {
                    sa.dst_size = sa.size;
                }
                sa.buf_size        = avail_size;
                sa.shader_location = attr.shader_location;
                sa.is_instance     = (vb.step_mode == GPUVertexStepMode::INSTANCE);
                stream_attrs.push_back(sa);
            }
        }
        if (!stream_attrs.empty()) {
            has_multi_stream_vs = true;
            std::sort(stream_attrs.begin(), stream_attrs.end(), [](const StreamAttr& a, const StreamAttr& b) {
                return a.shader_location < b.shader_location;
            });
            GPUSize64 cur_dst = 0;
            for (auto& sa : stream_attrs) {
                sa.dst_offset = cur_dst;
                cur_dst += sa.dst_size;
            }
        }
    }

    auto read_index = [&](uint idx) -> uint {
        if (ctx.index_buffer.format == GPUIndexFormat::UINT16) {
            return reinterpret_cast<const ushort*>(idata)[idx];
        } else {
            return reinterpret_cast<const uint*>(idata)[idx];
        }
    };

    auto read_vertex = [&](uint vert_idx, uint current_inst) -> SoftShadedVertex {
        SoftShadedVertex sv{};
        if (!vbuf) {
            run_vs(vert_idx, sv, nullptr);
            return sv;
        }

        const ubyte* ptr = vdata + vert_idx * p.stride;
        if (ctx.render_pipeline && ctx.render_pipeline->vshader_func) {
            if (has_multi_stream_vs) {
                alignas(16) ubyte assembled_v[256] = {};
                for (const auto& sa : stream_attrs) {
                    uint      step_idx = sa.is_instance ? current_inst : vert_idx;
                    GPUSize64 offset   = static_cast<GPUSize64>(step_idx) * sa.stride + sa.src_offset;
                    if (offset + sa.size <= sa.buf_size) {
                        if (sa.format == GPUVertexFormat::UNORM8x4) {
                            const ubyte* col     = sa.base_ptr + offset;
                            float        rgba[4] = {
                                col[0] / 255.0f,
                                col[1] / 255.0f,
                                col[2] / 255.0f,
                                col[3] / 255.0f,
                            };
                            std::memcpy(assembled_v + sa.dst_offset, rgba, sizeof(rgba));
                        } else if (sa.format == GPUVertexFormat::UNORM8x4_BGRA) {
                            const ubyte* col     = sa.base_ptr + offset;
                            float        rgba[4] = {
                                col[2] / 255.0f,
                                col[1] / 255.0f,
                                col[0] / 255.0f,
                                col[3] / 255.0f,
                            };
                            std::memcpy(assembled_v + sa.dst_offset, rgba, sizeof(rgba));
                        } else {
                            std::memcpy(assembled_v + sa.dst_offset, sa.base_ptr + offset, sa.size);
                        }
                    }
                }
                run_vs(vert_idx, sv, assembled_v);
            } else {
                run_vs(vert_idx, sv, ptr);
            }
            return sv;
        }

        bool is_split_3d = (vdata1 != nullptr) && (!ctx.render_pipeline || ctx.render_pipeline->vertex_buffers.size() <= 1 ||
                                                      (ctx.render_pipeline->vertex_buffers.size() > 1 && ctx.render_pipeline->vertex_buffers[1].step_mode != GPUVertexStepMode::INSTANCE));

        if (is_split_3d) {
            const auto pos = reinterpret_cast<const Vector3*>(vdata + vert_idx * 12);
            Vector4    local_pos(pos->x, pos->y, pos->z, 1.0f);

            Matrix4x4 model = Matrix4x4(1.0f);
            if (!ctx.immediates_data.empty() && ctx.immediates_data.size() >= sizeof(Matrix4x4)) {
                model             = *reinterpret_cast<const Matrix4x4*>(ctx.immediates_data.data());
                Vector4 world_pos = model * local_pos;
                sv.position       = p.has_vp ? (p.vp_matrix * world_pos) : world_pos;
                sv.extra          = world_pos;
            } else {
                sv.position = p.has_vp ? (p.vp_matrix * local_pos) : local_pos;
                sv.extra    = local_pos;
            }

            const auto norm       = reinterpret_cast<const Vector3*>(vdata1 + vert_idx * 12);
            Vector3    world_norm = glm::mat3(model) * (*norm);
            sv.normal             = Vector4(glm::normalize(world_norm), 0.0f);

            if (vdata2 != nullptr) {
                const auto uv = reinterpret_cast<const Vector2*>(vdata2 + vert_idx * 8);
                sv.texcoord   = Vector4(uv->x, uv->y, 0.0f, 0.0f);
            }
            sv.color = Vector4(1.0f);
            std::memcpy(&sv.varyings[0], &sv.normal, sizeof(float) * 3);
            std::memcpy(&sv.varyings[3], &sv.texcoord, sizeof(float) * 2);
            std::memcpy(&sv.varyings[5], &sv.extra, sizeof(float) * 3);
            return sv;
        }

        ptr = vdata + vert_idx * p.stride;
        Vector4 local_pos(0.0f, 0.0f, 0.0f, 1.0f);
        if (p.pos_format == GPUVertexFormat::FLOAT32x2) {
            const auto pos = reinterpret_cast<const Vector2*>(ptr + p.pos_offset);
            local_pos.x    = pos->x;
            local_pos.y    = pos->y;
        } else {
            const auto pos = reinterpret_cast<const Vector3*>(ptr + p.pos_offset);
            local_pos.x    = pos->x;
            local_pos.y    = pos->y;
            local_pos.z    = pos->z;
        }

        if (p.has_uv || p.bound_tex != nullptr) {
            const auto uv = reinterpret_cast<const Vector2*>(ptr + p.uv_offset);
            sv.texcoord   = Vector4(uv->x, uv->y, 0.0f, 0.0f);
        }
        if (p.has_col) {
            if (p.col_format == GPUVertexFormat::UNORM8x4) {
                const auto col = ptr + p.col_offset;
                sv.color       = Vector4(col[0] / 255.0f, col[1] / 255.0f, col[2] / 255.0f, col[3] / 255.0f);
            } else if (p.col_format == GPUVertexFormat::FLOAT32x4) {
                const auto col = reinterpret_cast<const Vector4*>(ptr + p.col_offset);
                sv.color       = *col;
            } else {
                const auto col = reinterpret_cast<const Vector3*>(ptr + p.col_offset);
                sv.color       = Vector4(col->x, col->y, col->z, 1.0f);
            }
        } else if (p.col_offset + sizeof(Vector3) <= p.stride && p.bound_tex == nullptr) {
            const auto col = reinterpret_cast<const Vector3*>(ptr + p.col_offset);
            sv.color       = Vector4(col->x, col->y, col->z, 1.0f);
        } else {
            sv.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        std::memcpy(&sv.varyings[0], &sv.normal, sizeof(float) * 3);
        std::memcpy(&sv.varyings[3], &sv.texcoord, sizeof(float) * 2);
        std::memcpy(&sv.varyings[5], &sv.extra, sizeof(float) * 3);
        for (const auto& attr : p.vb_attrs) {
            if (attr.shader_location >= 1 && attr.shader_location <= 4) {
                uint loc = attr.shader_location - 1;
                if (attr.format == GPUVertexFormat::FLOAT32x4 && attr.offset + 16 <= p.stride) {
                    std::memcpy(&sv.varyings[loc * 4], ptr + attr.offset, 16);
                    if (loc == 0) {
                        std::memcpy(&sv.color, ptr + attr.offset, 16);
                    }
                }
            }
        }

        // secondary vertex buffer streams / instance streams
        if (ctx.render_pipeline && ctx.render_pipeline->vertex_buffers.size() > 1) {
            for (usize s = 1; s < ctx.render_pipeline->vertex_buffers.size(); ++s) {
                const auto& vb_layout = ctx.render_pipeline->vertex_buffers[s];
                auto        it_slot   = ctx.vertex_buffers.find(static_cast<uint>(s));
                if (it_slot == ctx.vertex_buffers.end()) continue;
                auto sbuf = device.buffers.find(it_slot->second.buffer.to_slotmap_handle<SoftBuffer>());
                if (!sbuf) continue;

                uint      step_idx    = (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) ? current_inst : vert_idx;
                GPUSize64 elem_offset = it_slot->second.offset + static_cast<GPUSize64>(step_idx) * vb_layout.array_stride;
                if (elem_offset + vb_layout.array_stride > sbuf->data.size()) continue;
                const ubyte* elem = sbuf->data.data() + elem_offset;

                for (const auto& attr : vb_layout.attributes) {
                    if (attr.offset + 8 <= vb_layout.array_stride && attr.format == GPUVertexFormat::FLOAT32x2) {
                        const auto v2 = reinterpret_cast<const Vector2*>(elem + attr.offset);
                        if (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) {
                            local_pos.x += v2->x;
                            local_pos.y += v2->y;
                        } else {
                            sv.texcoord = Vector4(v2->x, v2->y, 0.0f, 0.0f);
                        }
                    } else if (attr.offset + 12 <= vb_layout.array_stride && attr.format == GPUVertexFormat::FLOAT32x3) {
                        const auto v3 = reinterpret_cast<const Vector3*>(elem + attr.offset);
                        if (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) {
                            local_pos.x += v3->x;
                            local_pos.y += v3->y;
                            local_pos.z += v3->z;
                        } else {
                            sv.color = Vector4(v3->x, v3->y, v3->z, 1.0f);
                        }
                    } else if (attr.offset + 16 <= vb_layout.array_stride && attr.format == GPUVertexFormat::FLOAT32x4) {
                        const auto v4 = reinterpret_cast<const Vector4*>(elem + attr.offset);
                        sv.color      = *v4;
                        if (attr.shader_location >= 1 && attr.shader_location <= 4) {
                            uint loc = attr.shader_location - 1;
                            std::memcpy(&sv.varyings[loc * 4], v4, 16);
                        }
                    }
                }
            }
        }

        sv.position = p.has_vp ? (p.vp_matrix * local_pos) : local_pos;
        return sv;
    };

    bool is_strip = (p.r_ctx.primitive.topology == GPUPrimitiveTopology::TRIANGLE_STRIP);
    uint num_tris = is_strip ? (index_count >= 3 ? index_count - 2 : 0) : (index_count / 3);
    for (uint inst = 0; inst < instance_count; ++inst) {
        uint current_inst = first_instance + inst;
        for (uint t = 0; t < num_tris; ++t) {
            uint off0, off1, off2;
            if (is_strip) {
                if (t % 2 == 0) {
                    off0 = t;
                    off1 = t + 1;
                    off2 = t + 2;
                } else {
                    off0 = t + 1;
                    off1 = t;
                    off2 = t + 2;
                }
            } else {
                off0 = t * 3 + 0;
                off1 = t * 3 + 1;
                off2 = t * 3 + 2;
            }

            uint i0 = read_index(first_index + off0) + base_vertex;
            uint i1 = read_index(first_index + off1) + base_vertex;
            uint i2 = read_index(first_index + off2) + base_vertex;

            SoftShadedVertex v0 = read_vertex(i0, current_inst);
            SoftShadedVertex v1 = read_vertex(i1, current_inst);
            SoftShadedVertex v2 = read_vertex(i2, current_inst);

            Vector<SoftRasterTriangle> tris;
            SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
            for (const auto& tri : tris) {
                SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
            }
        }
    }
}

void SoftCommandBuffer::draw(uint vertex_count, uint instance_count, uint first_vertex, uint first_instance)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        execute_draw(device, ctx, vertex_count, instance_count, first_vertex, first_instance);
    });
}

void SoftCommandBuffer::draw_indexed(uint index_count, uint instance_count, uint first_index, int32_t base_vertex, uint first_instance)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        execute_draw_indexed(device, ctx, index_count, instance_count, first_index, base_vertex, first_instance);
    });
}

void SoftCommandBuffer::draw_indirect(GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        auto buf = device.buffers.find(indirect_buffer.to_slotmap_handle<SoftBuffer>());
        if (!buf) return;
        for (GPUSize32 i = 0; i < draw_count; ++i) {
            GPUSize64 offset = indirect_offset + i * sizeof(SoftDrawIndirectCommand);
            if (offset + sizeof(SoftDrawIndirectCommand) > buf->data.size()) break;
            const auto cmd = reinterpret_cast<const SoftDrawIndirectCommand*>(buf->data.data() + offset);
            execute_draw(device, ctx, cmd->vertex_count, cmd->instance_count, cmd->first_vertex, cmd->first_instance);
        }
    });
}

void SoftCommandBuffer::draw_indexed_indirect(GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        auto buf = device.buffers.find(indirect_buffer.to_slotmap_handle<SoftBuffer>());
        if (!buf) return;
        for (GPUSize32 i = 0; i < draw_count; ++i) {
            GPUSize64 offset = indirect_offset + i * sizeof(SoftDrawIndexedIndirectCommand);
            if (offset + sizeof(SoftDrawIndexedIndirectCommand) > buf->data.size()) break;
            const auto cmd = reinterpret_cast<const SoftDrawIndexedIndirectCommand*>(buf->data.data() + offset);
            execute_draw_indexed(device, ctx, cmd->index_count, cmd->instance_count, cmd->first_index, cmd->base_vertex, cmd->first_instance);
        }
    });
}

static void execute_dispatch_workgroups(SoftDevice& device, ExecutionContext& ctx, uint x, uint y, uint z)
{
    if (x == 0 || y == 0 || z == 0) return;

    auto it_bg0 = ctx.bind_groups.find(0);
    if (it_bg0 != ctx.bind_groups.end() && it_bg0->second.handle.valid()) {
        auto bg = device.bind_groups.find(it_bg0->second.handle.to_slotmap_handle<SoftBindGroup>());
        if (bg) {
            const auto e_cam  = bg->find_entry(0);
            const auto e_tlas = bg->find_entry(1);
            const auto e_out2 = bg->find_entry(2);
            const auto e_out3 = bg->find_entry(3);
            const auto e_out  = e_out3 ? e_out3 : e_out2;

            if (e_cam && e_tlas && e_out && e_cam->type == GPUResourceType::BUFFER &&
                e_tlas->type == GPUResourceType::ACCELERATION_STRUCTURE &&
                (e_out->type == GPUResourceType::TEXTURE || e_out->type == GPUResourceType::STORAGE_TEXTURE) &&
                e_cam->buffer.buffer.valid() && e_tlas->tlas.valid() && e_out->texture.valid()) {
                auto         ubuf    = device.buffers.find(e_cam->buffer.buffer.to_slotmap_handle<SoftBuffer>());
                auto         tlas    = device.tlases.find(e_tlas->tlas.to_slotmap_handle<SoftTlas>());
                auto         view    = device.texture_views.find(e_out->texture.to_slotmap_handle<SoftTextureView>());
                SoftTexture* out_tex = view ? device.textures.find(view->texture.to_slotmap_handle<SoftTexture>()) : nullptr;

                if (ubuf && tlas && out_tex) {
                    struct CameraLegacyData
                    {
                        Matrix4x4 proj;
                        Matrix4x4 view;
                        Matrix4x4 view_inv;
                        Matrix4x4 proj_inv;
                    };
                    struct CameraUniformsData
                    {
                        Matrix4x4 view;
                        Matrix4x4 proj;
                        Matrix4x4 view_proj;
                        Matrix4x4 inv_view;
                        Matrix4x4 inv_proj;
                    };

                    Matrix4x4 view_inv{1.0f};
                    Matrix4x4 proj_inv{1.0f};
                    bool      valid_cam = false;

                    if (ubuf->data.size() - e_cam->buffer.offset >= sizeof(CameraUniformsData)) {
                        const auto cam = reinterpret_cast<const CameraUniformsData*>(ubuf->data.data() + e_cam->buffer.offset);
                        view_inv       = cam->inv_view;
                        proj_inv       = cam->inv_proj;
                        valid_cam      = true;
                    } else if (ubuf->data.size() - e_cam->buffer.offset >= sizeof(CameraLegacyData)) {
                        const auto cam = reinterpret_cast<const CameraLegacyData*>(ubuf->data.data() + e_cam->buffer.offset);
                        view_inv       = cam->view_inv;
                        proj_inv       = cam->proj_inv;
                        valid_cam      = true;
                    }

                    if (valid_cam) {
                        uint width  = out_tex->extent.width;
                        uint height = out_tex->extent.height;

                        for (uint py = 0; py < height; ++py) {
                            for (uint px = 0; px < width; ++px) {
                                float u     = (static_cast<float>(px) + 0.5f) / static_cast<float>(width);
                                float v     = (static_cast<float>(py) + 0.5f) / static_cast<float>(height);
                                float ndc_x = u * 2.0f - 1.0f;
                                float ndc_y = -(v * 2.0f - 1.0f);

                                Vector4 target = proj_inv * Vector4(ndc_x, ndc_y, 1.0f, 1.0f);
                                Vector3 dir    = Vector3(view_inv * Vector4(glm::normalize(Vector3(target) / target.w), 0.0f));
                                Vector3 orig   = Vector3(view_inv * Vector4(0.0f, 0.0f, 0.0f, 1.0f));

                                SoftRayQuery q;
                                q.trace_ray_inline(*tlas, 0, 0xFF, orig, glm::normalize(dir), 0.001f, 10000.0f, device.blases);
                                if (q.committed_status() == SoftRayQueryStatus::TRIANGLE_HIT) {
                                    Vector2 bary = q.committed_triangle_barycentrics();
                                    Vector4 col(1.0f - bary.x - bary.y, bary.x, bary.y, 1.0f);
                                    out_tex->write_rgba(px, py, col);
                                } else {
                                    out_tex->write_rgba(px, py, Vector4(0.0f, 0.0f, 0.0f, 0.0f));
                                }
                            }
                        }
                    }
                    return;
                }
            }

            if (e_cam && e_tlas && e_out && e_cam->type == GPUResourceType::BUFFER && e_tlas->type == GPUResourceType::BUFFER && e_out->type == GPUResourceType::BUFFER &&
                ctx.immediates_data.size() >= 224) {
                auto b_lights = device.buffers.find(e_cam->buffer.buffer.to_slotmap_handle<SoftBuffer>());
                auto b_grid   = device.buffers.find(e_tlas->buffer.buffer.to_slotmap_handle<SoftBuffer>());
                auto b_index  = device.buffers.find(e_out->buffer.buffer.to_slotmap_handle<SoftBuffer>());

                if (b_lights && b_grid && b_index) {
                    struct alignas(16) LightCullingPush
                    {
                        Matrix4x4 view_matrix;
                        Matrix4x4 proj_matrix;
                        Matrix4x4 inv_proj_matrix;
                        Vector2   screen_resolution;
                        float     near_plane;
                        float     far_plane;
                        uint      cluster_grid_x;
                        uint      cluster_grid_y;
                        uint      cluster_grid_z;
                        uint      num_lights;
                    };
                    const auto push = reinterpret_cast<const LightCullingPush*>(ctx.immediates_data.data());

                    struct ClusterRecord
                    {
                        uint offset;
                        uint count;
                    };
                    struct alignas(16) LightData
                    {
                        Vector3 position;
                        float   range;
                        Vector3 direction;
                        float   intensity;
                        Vector3 color;
                        uint    type;
                        float   spot_inner_angle;
                        float   spot_outer_angle;
                        uint    cast_shadows;
                        uint    profile;
                    };

                    auto       grid_records    = reinterpret_cast<ClusterRecord*>(b_grid->data.data() + e_tlas->buffer.offset);
                    auto       index_list      = reinterpret_cast<uint*>(b_index->data.data() + e_out->buffer.offset);
                    const auto gathered_lights = reinterpret_cast<const LightData*>(b_lights->data.data() + e_cam->buffer.offset);

                    uint  num_lights = push->num_lights;
                    float near_p     = push->near_plane > 0.0f ? push->near_plane : 0.1f;
                    float far_p      = push->far_plane > near_p ? push->far_plane : 100.0f;
                    float screen_w   = push->screen_resolution.x;
                    float screen_h   = push->screen_resolution.y;
                    float log_ratio  = std::log(std::max(far_p, near_p + 0.1f) / std::max(near_p, 0.001f));

                    Matrix4x4 inv_proj = push->inv_proj_matrix;
                    Matrix4x4 view_mat = push->view_matrix;

                    for (uint cz = 0; cz < push->cluster_grid_z; ++cz) {
                        float z_near = near_p * std::exp(log_ratio * (float(cz) / float(push->cluster_grid_z)));
                        float z_far  = near_p * std::exp(log_ratio * (float(cz + 1) / float(push->cluster_grid_z)));

                        for (uint cy = 0; cy < push->cluster_grid_y; ++cy) {
                            for (uint cx = 0; cx < push->cluster_grid_x; ++cx) {
                                uint cluster_index = cx + cy * push->cluster_grid_x + cz * push->cluster_grid_x * push->cluster_grid_y;
                                uint base_offset   = cluster_index * 64;
                                uint visible_count = 0;

                                Vector2 min_pixel = Vector2(float(cx * 16), float(cy * 16));
                                Vector2 max_pixel = glm::min(min_pixel + Vector2(16.0f, 16.0f), Vector2(screen_w, screen_h));

                                Vector2 ndc_min = Vector2(
                                    (min_pixel.x / screen_w) * 2.0f - 1.0f,
                                    1.0f - (max_pixel.y / screen_h) * 2.0f);
                                Vector2 ndc_max = Vector2(
                                    (max_pixel.x / screen_w) * 2.0f - 1.0f,
                                    1.0f - (min_pixel.y / screen_h) * 2.0f);

                                Vector4 c0 = inv_proj * Vector4(ndc_min.x, ndc_min.y, 0.5f, 1.0f);
                                Vector4 c1 = inv_proj * Vector4(ndc_max.x, ndc_min.y, 0.5f, 1.0f);
                                Vector4 c2 = inv_proj * Vector4(ndc_min.x, ndc_max.y, 0.5f, 1.0f);
                                Vector4 c3 = inv_proj * Vector4(ndc_max.x, ndc_max.y, 0.5f, 1.0f);

                                Vector3 ray0 = Vector3(c0) / c0.w;
                                Vector3 ray1 = Vector3(c1) / c1.w;
                                Vector3 ray2 = Vector3(c2) / c2.w;
                                Vector3 ray3 = Vector3(c3) / c3.w;

                                ray0 *= 1.0f / std::max(std::abs(ray0.z), 1e-6f);
                                ray1 *= 1.0f / std::max(std::abs(ray1.z), 1e-6f);
                                ray2 *= 1.0f / std::max(std::abs(ray2.z), 1e-6f);
                                ray3 *= 1.0f / std::max(std::abs(ray3.z), 1e-6f);

                                Vector3 p_near0 = ray0 * z_near;
                                Vector3 p_near1 = ray1 * z_near;
                                Vector3 p_near2 = ray2 * z_near;
                                Vector3 p_near3 = ray3 * z_near;

                                Vector3 p_far0 = ray0 * z_far;
                                Vector3 p_far1 = ray1 * z_far;
                                Vector3 p_far2 = ray2 * z_far;
                                Vector3 p_far3 = ray3 * z_far;

                                Vector3 aabb_min = glm::min(glm::min(glm::min(p_near0, p_near1), glm::min(p_near2, p_near3)),
                                    glm::min(glm::min(p_far0, p_far1), glm::min(p_far2, p_far3)));
                                Vector3 aabb_max = glm::max(glm::max(glm::max(p_near0, p_near1), glm::max(p_near2, p_near3)),
                                    glm::max(glm::max(p_far0, p_far1), glm::max(p_far2, p_far3)));

                                for (uint i = 0; i < num_lights && visible_count < 64; ++i) {
                                    const auto& light = gathered_lights[i];
                                    if (light.type == 0) {
                                        index_list[base_offset + visible_count] = i;
                                        visible_count++;
                                        continue;
                                    }

                                    Vector4 view_pos     = view_mat * Vector4(light.position, 1.0f);
                                    Vector3 light_center = Vector3(view_pos);

                                    Vector3 closest = glm::clamp(light_center, aabb_min, aabb_max);
                                    Vector3 delta   = light_center - closest;
                                    float   dist_sq = glm::dot(delta, delta);

                                    if (dist_sq <= (light.range * light.range)) {
                                        index_list[base_offset + visible_count] = i;
                                        visible_count++;
                                    }
                                }

                                grid_records[cluster_index] = ClusterRecord{
                                    .offset = base_offset,
                                    .count  = visible_count,
                                };
                            }
                        }
                    }
                    return;
                }
            }
        }
    }

    if (ctx.compute_pipeline && ctx.compute_pipeline->cshader_func && ctx.compute_pipeline->compute.module.valid()) {
        auto* sm = device.shader_modules.find(ctx.compute_pipeline->compute.module.to_slotmap_handle<SoftShaderModule>());
        if (sm) {
            Array<ubyte, 4096>                  uniform_bytes{};
            Array<Array<ubyte, 2048>, 8>        pb_bytes{};
            Array<SoftHostTextureWrapper, 16>   host_textures{};
            Array<SoftHostRWTextureWrapper, 16> host_rw_textures{};
            uint                                host_tex_count    = 0;
            uint                                host_rw_tex_count = 0;

            for (uint i = 0; i < sm->param_count; ++i) {
                const auto& param = sm->params[i];

                ubyte* target_buf      = nullptr;
                usize  target_buf_size = 0;

                if (param.block_offset != 0xFFFF) {
                    if (param.set >= pb_bytes.size()) continue;
                    target_buf      = pb_bytes[param.set].data();
                    target_buf_size = pb_bytes[param.set].size();

                    if (param.block_offset + sizeof(void*) <= uniform_bytes.size()) {
                        const void* pb_ptr = target_buf;
                        std::memcpy(uniform_bytes.data() + param.block_offset, &pb_ptr, sizeof(void*));
                    }
                } else {
                    target_buf      = uniform_bytes.data();
                    target_buf_size = uniform_bytes.size();
                }

                if (param.offset + sizeof(void*) > target_buf_size) continue;

                if (param.is_immediates) {
                    if (!ctx.immediates_data.empty()) {
                        const void* ptr = ctx.immediates_data.data();
                        std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                    }
                } else {
                    auto it_bg = ctx.bind_groups.find(param.set);
                    if (it_bg == ctx.bind_groups.end() || !it_bg->second.handle.valid()) continue;
                    auto bg = device.bind_groups.find(it_bg->second.handle.to_slotmap_handle<SoftBindGroup>());
                    if (!bg) continue;

                    for (const auto& entry : bg->entries) {
                        if (entry.binding != param.binding) continue;

                        if (entry.type == GPUResourceType::BUFFER && entry.buffer.buffer.valid()) {
                            auto ubuf = device.buffers.find(entry.buffer.buffer.to_slotmap_handle<SoftBuffer>());
                            if (ubuf) {
                                usize       dyn_off = it_bg->second.dynamic_offsets.empty() ? 0 : it_bg->second.dynamic_offsets[0];
                                usize       eff_off = entry.buffer.offset + dyn_off;
                                const void* ptr     = ubuf->data.data() + eff_off;
                                std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                                if (param.size >= 16) {
                                    usize buf_size = (ubuf->data.size() > eff_off) ? (ubuf->data.size() - eff_off) : 0;
                                    std::memcpy(target_buf + param.offset + 8, &buf_size, sizeof(usize));
                                }
                            }
                        } else if (entry.type == GPUResourceType::TEXTURE && entry.texture.valid()) {
                            auto v = device.texture_views.find(entry.texture.to_slotmap_handle<SoftTextureView>());
                            if (v) {
                                auto t = device.textures.find(v->texture.to_slotmap_handle<SoftTexture>());
                                if (t && host_tex_count < host_textures.size()) {
                                    uint tex_idx           = host_tex_count++;
                                    host_textures[tex_idx] = SoftHostTextureWrapper(t, v);
                                    const void* ptr        = &host_textures[tex_idx];
                                    std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                                }
                            }
                        } else if (entry.type == GPUResourceType::STORAGE_TEXTURE && entry.texture.valid()) {
                            auto v = device.texture_views.find(entry.texture.to_slotmap_handle<SoftTextureView>());
                            if (v) {
                                auto t = device.textures.find(v->texture.to_slotmap_handle<SoftTexture>());
                                if (t && host_rw_tex_count < host_rw_textures.size()) {
                                    uint tex_idx              = host_rw_tex_count++;
                                    host_rw_textures[tex_idx] = SoftHostRWTextureWrapper(t, v);
                                    const void* ptr           = &host_rw_textures[tex_idx];
                                    std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                                }
                            }
                        } else if (entry.type == GPUResourceType::SAMPLER && entry.sampler.valid()) {
                            auto s = device.samplers.find(entry.sampler.to_slotmap_handle<SoftSampler>());
                            if (s) {
                                const void* ptr = s;
                                std::memcpy(target_buf + param.offset, &ptr, sizeof(void*));
                            }
                        }
                        break;
                    }
                }
            }

            ComputeVaryingInput vi{{0, 0, 0}, {x, y, z}};
            auto                func = reinterpret_cast<SlangComputeCallable>(ctx.compute_pipeline->cshader_func);
            func(&vi, nullptr, uniform_bytes.data());

            for (uint i = 0; i < host_rw_tex_count; ++i) {
                host_rw_textures[i].flush();
            }
            return;
        }
    }
}

void SoftCommandBuffer::dispatch_workgroups(uint x, uint y, uint z)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        execute_dispatch_workgroups(device, ctx, x, y, z);
    });
}

void SoftCommandBuffer::dispatch_workgroups_indirect(GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        auto b = device.buffers.find(indirect_buffer.to_slotmap_handle<SoftBuffer>());
        if (b && indirect_offset + 12 <= b->data.size()) {
            const auto args = reinterpret_cast<const uint*>(b->data.data() + indirect_offset);
            uint       x    = args[0];
            uint       y    = args[1];
            uint       z    = args[2];
            execute_dispatch_workgroups(device, ctx, x, y, z);
        }
    });
}

void SoftCommandBuffer::copy_buffer_to_buffer(GPUBufferHandle src, GPUSize64 src_off, GPUBufferHandle dst, GPUSize64 dst_off, GPUSize64 size)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext&) {
        auto s = device.buffers.find(src.to_slotmap_handle<SoftBuffer>());
        auto d = device.buffers.find(dst.to_slotmap_handle<SoftBuffer>());
        if (s && d && size > 0) {
            if (src_off + size <= s->data.size() && dst_off + size <= d->data.size()) {
                std::memcpy(d->data.data() + dst_off, s->data.data() + src_off, static_cast<usize>(size));
            }
        }
    });
}

void SoftCommandBuffer::copy_buffer_to_texture(const GPUTexelCopyBufferInfo& src, const GPUTexelCopyTextureInfo& dst, GPUExtent3D copy_size)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext&) {
        auto s = device.buffers.find(src.buffer.to_slotmap_handle<SoftBuffer>());
        auto d = device.textures.find(dst.texture.to_slotmap_handle<SoftTexture>());
        if (s && d) {
            usize        bpp             = d->pixel_bytes();
            const ubyte* src_bytes       = s->data.data() + src.offset;
            usize        unaligned_pitch = copy_size.width * bpp;
            usize        aligned_pitch   = (unaligned_pitch + 255) & ~255;
            usize        src_row_pitch   = src.bytes_per_row;
            if (src_row_pitch == 0) {
                if (src.offset + aligned_pitch * copy_size.height <= s->data.size()) {
                    src_row_pitch = aligned_pitch;
                } else {
                    src_row_pitch = unaligned_pitch;
                }
            }

            for (uint layer = 0; layer < copy_size.depth; ++layer) {
                uint dst_layer = dst.origin.z + layer;
                for (uint y = 0; y < copy_size.height; ++y) {
                    uint dst_y = dst.origin.y + y;
                    for (uint x = 0; x < copy_size.width; ++x) {
                        uint         dst_x   = dst.origin.x + x;
                        const ubyte* p       = src_bytes + layer * (src_row_pitch * copy_size.height) + y * src_row_pitch + x * bpp;
                        ubyte*       dst_ptr = d->get_texel_ptr(dst_x, dst_y, 0, dst.mip_level, dst_layer);
                        if (dst_ptr) {
                            std::memcpy(dst_ptr, p, bpp);
                        }
                    }
                }
            }
        }
    });
}

void SoftCommandBuffer::copy_texture_to_buffer(const GPUTexelCopyTextureInfo& src, const GPUTexelCopyBufferInfo& dst, const GPUExtent3D& copy_size)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext&) {
        auto s = device.textures.find(src.texture.to_slotmap_handle<SoftTexture>());
        auto d = device.buffers.find(dst.buffer.to_slotmap_handle<SoftBuffer>());
        if (s && d) {
            usize  bpp             = s->pixel_bytes();
            ubyte* dst_bytes       = d->data.data() + dst.offset;
            usize  unaligned_pitch = copy_size.width * bpp;
            usize  aligned_pitch   = (unaligned_pitch + 255) & ~255;
            usize  dst_row_pitch   = dst.bytes_per_row;
            if (dst_row_pitch == 0) {
                if (dst.offset + aligned_pitch * copy_size.height <= d->data.size()) {
                    dst_row_pitch = aligned_pitch;
                } else {
                    dst_row_pitch = unaligned_pitch;
                }
            }

            for (uint layer = 0; layer < copy_size.depth; ++layer) {
                uint src_layer = src.origin.z + layer;
                for (uint y = 0; y < copy_size.height; ++y) {
                    uint src_y = src.origin.y + y;
                    for (uint x = 0; x < copy_size.width; ++x) {
                        uint         src_x   = src.origin.x + x;
                        const ubyte* p       = s->get_texel_ptr(src_x, src_y, 0, src.mip_level, src_layer);
                        ubyte*       dst_ptr = dst_bytes + layer * (dst_row_pitch * copy_size.height) + y * dst_row_pitch + x * bpp;
                        if (p && dst_ptr < d->data.data() + d->data.size()) {
                            std::memcpy(dst_ptr, p, bpp);
                        }
                    }
                }
            }
        }
    });
}

void SoftCommandBuffer::copy_texture_to_texture(const GPUTexelCopyTextureInfo& src, const GPUTexelCopyTextureInfo& dst, const GPUExtent3D& copy_size)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext&) {
        auto s = device.textures.find(src.texture.to_slotmap_handle<SoftTexture>());
        auto d = device.textures.find(dst.texture.to_slotmap_handle<SoftTexture>());
        if (s && d) {
            usize bpp = s->pixel_bytes();
            for (uint layer = 0; layer < copy_size.depth; ++layer) {
                uint src_layer = src.origin.z + layer;
                uint dst_layer = dst.origin.z + layer;
                for (uint y = 0; y < copy_size.height; ++y) {
                    uint src_y = src.origin.y + y;
                    uint dst_y = dst.origin.y + y;
                    for (uint x = 0; x < copy_size.width; ++x) {
                        uint         src_x   = src.origin.x + x;
                        uint         dst_x   = dst.origin.x + x;
                        const ubyte* p       = s->get_texel_ptr(src_x, src_y, 0, src.mip_level, src_layer);
                        ubyte*       dst_ptr = d->get_texel_ptr(dst_x, dst_y, 0, dst.mip_level, dst_layer);
                        if (p && dst_ptr) {
                            std::memcpy(dst_ptr, p, bpp);
                        }
                    }
                }
            }
        }
    });
}

void SoftCommandBuffer::clear_buffer(GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext&) {
        auto b = device.buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
        if (b && size > 0 && offset + size <= b->data.size()) {
            std::memset(b->data.data() + offset, 0, static_cast<usize>(size));
        }
    });
}

void SoftCommandBuffer::clear_texture(GPUTextureHandle texture, const GPUTextureSubresourceRange& range)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext&) {
        auto t = device.textures.find(texture.to_slotmap_handle<SoftTexture>());
        if (t) {
            for (uint m = 0; m < range.mip_level_count; ++m) {
                for (uint l = 0; l < range.array_layers; ++l) {
                    t->clear_color(GPUColor{0, 0, 0, 0}, range.base_mip_level + m, range.base_array_layer + l);
                }
            }
        }
    });
}

void SoftCommandBuffer::build_blases(GPUBufferHandle, GPUBlasBuildEntries entries)
{
    struct StoredBlasEntry
    {
        GPUBlasHandle                   blas;
        Vector<GPUBlasTriangleGeometry> triangles;
    };
    Vector<StoredBlasEntry> stored;
    for (const auto& e : entries) {
        StoredBlasEntry s;
        s.blas = e.blas;
        if (e.geometries.type == GPUBlasType::TRIANGLE) {
            s.triangles.assign(e.geometries.triangles.begin(), e.geometries.triangles.end());
        }
        stored.push_back(std::move(s));
    }

    commands.push_back([stored = std::move(stored)](SoftDevice& device, ExecutionContext&) {
        for (const auto& entry : stored) {
            auto blas = device.blases.find(entry.blas.to_slotmap_handle<SoftBlas>());
            if (blas) {
                GPUBlasBuildEntry b_entry{};
                b_entry.blas                 = entry.blas;
                b_entry.geometries.type      = GPUBlasType::TRIANGLE;
                b_entry.geometries.triangles = TypedView<GPUBlasTriangleGeometry>(entry.triangles.data(), entry.triangles.size());
                blas->build(b_entry, device.buffers);
            }
        }
    });
}

void SoftCommandBuffer::build_tlases(GPUBufferHandle, GPUTlasBuildEntries entries)
{
    struct StoredTlasEntry
    {
        GPUTlasHandle           tlas;
        Vector<GPUTlasInstance> instances;
    };
    Vector<StoredTlasEntry> stored;
    for (const auto& e : entries) {
        StoredTlasEntry s;
        s.tlas = e.tlas;
        s.instances.assign(e.instances.begin(), e.instances.end());
        stored.push_back(std::move(s));
    }

    commands.push_back([stored = std::move(stored)](SoftDevice& device, ExecutionContext&) {
        for (const auto& entry : stored) {
            auto tlas = device.tlases.find(entry.tlas.to_slotmap_handle<SoftTlas>());
            if (tlas) {
                GPUTlasBuildEntry t_entry{};
                t_entry.tlas      = entry.tlas;
                t_entry.instances = TypedView<GPUTlasInstance>(entry.instances.data(), entry.instances.size());
                tlas->build(t_entry, device.blases);
            }
        }
    });
}

void SoftCommandBuffer::execute_commands(SoftDevice& device)
{
    ExecutionContext ctx;

    for (auto& cmd : commands) {
        cmd(device, ctx);
    }
}

void cmd::insert_debug_marker(GPUCommandEncoderHandle, CString) {}
void cmd::push_debug_group(GPUCommandEncoderHandle, CString) {}
void cmd::pop_debug_group(GPUCommandEncoderHandle) {}

void cmd::wait_fence(GPUCommandEncoderHandle, GPUFenceHandle fence, GPUBarrierSyncFlags)
{
    auto* f = get_device()->fences.find(fence.to_slotmap_handle<SoftFence>());
    if (f) f->wait();
}

void cmd::signal_fence(GPUCommandEncoderHandle, GPUFenceHandle fence, GPUBarrierSyncFlags)
{
    auto* f = get_device()->fences.find(fence.to_slotmap_handle<SoftFence>());
    if (f) f->signal();
}

void cmd::begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& descriptor)
{
    get_device()->command(cmdbuffer).begin_render_pass(descriptor);
}

void cmd::end_render_pass(GPUCommandEncoderHandle cmdbuffer)
{
    get_device()->command(cmdbuffer).end_render_pass();
}

void cmd::set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline)
{
    get_device()->command(cmdbuffer).set_render_pipeline(pipeline);
}

void cmd::set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline)
{
    get_device()->command(cmdbuffer).set_compute_pipeline(pipeline);
}

void cmd::set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline)
{
    get_device()->command(cmdbuffer).set_raytracing_pipeline(pipeline);
}

void cmd::set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group, GPUBufferDynamicOffsets offsets)
{
    get_device()->command(cmdbuffer).set_bind_group(index, bind_group, offsets);
}

void cmd::set_immediates(GPUCommandEncoderHandle cmdbuffer, uint offset, uint size, const void* data)
{
    get_device()->command(cmdbuffer).set_immediates(offset, size, data);
}

void cmd::set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size)
{
    get_device()->command(cmdbuffer).set_index_buffer(buffer, format, offset, size);
}

void cmd::set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    get_device()->command(cmdbuffer).set_vertex_buffer(slot, buffer, offset, size);
}

void cmd::draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance)
{
    get_device()->command(cmdbuffer).draw(vertex_count, instance_count, first_vertex, first_instance);
}

void cmd::draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance)
{
    get_device()->command(cmdbuffer).draw_indexed(index_count, instance_count, first_index, base_vertex, first_instance);
}

void cmd::draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    get_device()->command(cmdbuffer).draw_indirect(indirect_buffer, indirect_offset, draw_count);
}

void cmd::draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    get_device()->command(cmdbuffer).draw_indexed_indirect(indirect_buffer, indirect_offset, draw_count);
}

void cmd::dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z)
{
    get_device()->command(cmdbuffer).dispatch_workgroups(x, y, z);
}

void cmd::dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset)
{
    get_device()->command(cmdbuffer).dispatch_workgroups_indirect(indirect_buffer, indirect_offset);
}

void cmd::copy_buffer_to_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle source, GPUSize64 source_offset, GPUBufferHandle destination, GPUSize64 destination_offset, GPUSize64 size)
{
    get_device()->command(cmdbuffer).copy_buffer_to_buffer(source, source_offset, destination, destination_offset, size);
}

void cmd::copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, GPUExtent3D copy_size)
{
    get_device()->command(cmdbuffer).copy_buffer_to_texture(source, destination, copy_size);
}

void cmd::copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size)
{
    get_device()->command(cmdbuffer).copy_texture_to_buffer(source, destination, copy_size);
}

void cmd::copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size)
{
    get_device()->command(cmdbuffer).copy_texture_to_texture(source, destination, copy_size);
}

void cmd::clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    get_device()->command(cmdbuffer).clear_buffer(buffer, offset, size);
}

void cmd::clear_texture(GPUCommandEncoderHandle cmdbuffer, GPUTextureHandle texture, const GPUTextureSubresourceRange& range)
{
    get_device()->command(cmdbuffer).clear_texture(texture, range);
}

void cmd::set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth)
{
    get_device()->command(cmdbuffer).set_viewport(x, y, w, h, min_depth, max_depth);
}

void cmd::set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h)
{
    get_device()->command(cmdbuffer).set_scissor(x, y, w, h);
}

void cmd::set_blend_constant(GPUCommandEncoderHandle, GPUColor) {}

void cmd::set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference)
{
    get_device()->command(cmdbuffer).set_stencil_reference(reference);
}

void cmd::begin_occlusion_query(GPUCommandEncoderHandle, GPUSize32) {}
void cmd::end_occlusion_query(GPUCommandEncoderHandle) {}
void cmd::write_timestamp(GPUCommandEncoderHandle, GPUQuerySetHandle, GPUSize32) {}
void cmd::write_blas_properties(GPUCommandEncoderHandle, GPUQuerySetHandle, GPUSize32, GPUBlasHandle) {}
void cmd::resolve_query_set(GPUCommandEncoderHandle, GPUQuerySetHandle, GPUSize32, GPUSize32, GPUBufferHandle, GPUSize64) {}

void cmd::memory_barrier(GPUCommandEncoderHandle, GPUMemoryBarriers) {}
void cmd::buffer_barrier(GPUCommandEncoderHandle, GPUBufferBarriers) {}
void cmd::texture_barrier(GPUCommandEncoderHandle, GPUTextureBarriers) {}

void cmd::build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUTlasBuildEntries entries)
{
    get_device()->command(cmdbuffer).build_tlases(scratch_buffer, entries);
}

void cmd::build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUBlasBuildEntries entries)
{
    get_device()->command(cmdbuffer).build_blases(scratch_buffer, entries);
}

void cmd::copy_blas(GPUCommandEncoderHandle, GPUBlasHandle, GPUBlasHandle) {}
