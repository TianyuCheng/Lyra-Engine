#include <cstring>

#include "SoftSampler.h"
#include "SoftPipeline.h"
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
    printf("[DEBUG begin_render_pass] cas=%zu ds_valid=%d\n", cas.size(), ds.view.valid());
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

void SoftCommandBuffer::set_bind_group(uint32_t index, GPUBindGroupHandle bind_group, TypedView<uint32_t> dynamic_offsets)
{
    Vector<uint32_t> offsets(dynamic_offsets.begin(), dynamic_offsets.end());
    commands.push_back([index, bind_group, offsets = std::move(offsets)](SoftDevice&, ExecutionContext& ctx) {
        ctx.bind_groups[index] = SoftBoundBindGroup{bind_group, std::move(offsets)};
    });
}

void SoftCommandBuffer::set_stencil_reference(uint32_t reference)
{
    commands.push_back([reference](SoftDevice&, ExecutionContext& ctx) {
        ctx.stencil_reference = reference;
    });
}

void SoftCommandBuffer::set_immediates(uint32_t offset, uint32_t size, const void* data)
{
    if (!data || size == 0) return;
    Vector<uint8_t> bytes(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    commands.push_back([offset, bytes = std::move(bytes)](SoftDevice&, ExecutionContext& ctx) {
        if (offset + bytes.size() > ctx.immediates_data.size()) {
            ctx.immediates_data.resize(offset + bytes.size());
        }
        std::memcpy(ctx.immediates_data.data() + offset, bytes.data(), bytes.size());
    });
}

void SoftCommandBuffer::set_vertex_buffer(uint32_t slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
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
    size_t                pos_offset = 0;
    size_t                col_offset = 12;
    size_t                uv_offset  = 12;
    GPUVertexFormat       pos_format = GPUVertexFormat::FLOAT32x3;
    GPUVertexFormat       col_format = GPUVertexFormat::FLOAT32x3;
    bool                  has_col    = false;
    bool                  has_uv     = false;
    const SoftTexture*    bound_tex  = nullptr;
    alignas(16) Array<uint8_t, 512> uniform_bytes{};
    Array<SoftHostTextureWrapper, 16> host_textures{};
    uint32_t                          host_tex_count = 0;
    Vector<SoftVertexAttribute>       vb_attrs;
};

static void setup_draw_params(SoftDevice& device, const ExecutionContext& ctx, SoftDrawParams& p)
{
    p.uniform_bytes.fill(0);

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
                        size_t dyn_off = bound_bg.dynamic_offsets.empty() ? 0 : bound_bg.dynamic_offsets[0];
                        size_t eff_off = entry.buffer.offset + dyn_off;
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

        for (uint32_t i = 0; i < sm->param_count; ++i) {
            const auto& param = sm->params[i];
            if (param.offset + sizeof(void*) > p.uniform_bytes.size()) continue;

            if (param.is_immediates) {
                if (!ctx.immediates_data.empty()) {
                    const void* ptr = ctx.immediates_data.data();
                    std::memcpy(p.uniform_bytes.data() + param.offset, &ptr, sizeof(void*));
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
                            size_t      dyn_off = it_bg->second.dynamic_offsets.empty() ? 0 : it_bg->second.dynamic_offsets[0];
                            size_t      eff_off = entry.buffer.offset + dyn_off;
                            const void* ptr     = ubuf->data.data() + eff_off;
                            std::memcpy(p.uniform_bytes.data() + param.offset, &ptr, sizeof(void*));
                            if (param.size >= 16) {
                                size_t buf_size = (ubuf->data.size() > eff_off) ? (ubuf->data.size() - eff_off) : 0;
                                std::memcpy(p.uniform_bytes.data() + param.offset + 8, &buf_size, sizeof(size_t));
                            }
                        }
                    } else if (entry.type == GPUResourceType::TEXTURE && entry.texture.valid()) {
                        auto v = device.texture_views.find(entry.texture.to_slotmap_handle<SoftTextureView>());
                        if (v) {
                            auto t = device.textures.find(v->texture.to_slotmap_handle<SoftTexture>());
                            if (t && p.host_tex_count < p.host_textures.size()) {
                                uint32_t tex_idx         = p.host_tex_count++;
                                p.host_textures[tex_idx] = SoftHostTextureWrapper(t, v);
                                const void* ptr          = &p.host_textures[tex_idx];
                                std::memcpy(p.uniform_bytes.data() + param.offset, &ptr, sizeof(void*));
                            }
                        }
                    } else if (entry.type == GPUResourceType::SAMPLER && entry.sampler.valid()) {
                        auto s = device.samplers.find(entry.sampler.to_slotmap_handle<SoftSampler>());
                        if (s) {
                            const void* ptr = s;
                            std::memcpy(p.uniform_bytes.data() + param.offset, &ptr, sizeof(void*));
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
            size_t          sample_count = 0;
            const uint32_t* spv          = reinterpret_cast<const uint32_t*>(sm->bytecode.data());
            size_t          spv_len      = sm->bytecode.size() / 4;
            for (size_t i = 5; i < spv_len;) {
                uint32_t word = spv[i];
                uint16_t op   = word & 0xFFFF;
                uint16_t len  = (word >> 16) & 0xFFFF;
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
                p.pos_offset = static_cast<size_t>(attr.offset);
                p.pos_format = attr.format;
            } else if (attr.shader_location == 1) {
                if (attr.format == GPUVertexFormat::FLOAT32x2) {
                    p.uv_offset = static_cast<size_t>(attr.offset);
                    p.has_uv    = true;
                } else {
                    p.col_offset = static_cast<size_t>(attr.offset);
                    p.col_format = attr.format;
                    p.has_col    = true;
                }
            } else if (attr.shader_location == 2) {
                p.col_offset = static_cast<size_t>(attr.offset);
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
    uint32_t vertex_count;
    uint32_t instance_count;
    uint32_t first_vertex;
    uint32_t first_instance;
};

struct SoftDrawIndexedIndirectCommand
{
    uint32_t index_count;
    uint32_t instance_count;
    uint32_t first_index;
    int32_t  base_vertex;
    uint32_t first_instance;
};

static void execute_draw(SoftDevice& device, ExecutionContext& ctx, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    if (instance_count == 0 || vertex_count == 0) return;

    SoftDrawParams p{};
    setup_draw_params(device, ctx, p);

    auto it_v     = ctx.vertex_buffers.find(0);
    bool has_vbuf = (it_v != ctx.vertex_buffers.end()) && ctx.render_pipeline && !ctx.render_pipeline->vertex_buffers.empty();
    if (!has_vbuf) {
        auto run_vs = [&](uint32_t vid, SoftShadedVertex& sv) {
            if (ctx.render_pipeline && ctx.render_pipeline->vshader_func) {
                struct EpParams
                {
                    uint32_t* in_v;
                    void*     out_v;
                };
                struct VSOut
                {
                    Vector4 pos;
                    float   varyings[16];
                } vs_out{};
                uint32_t v_idx = vid;
                EpParams ep    = {&v_idx, &vs_out};

                using ComputeFunc = void (*)(void*, void*, void*);
                auto func         = reinterpret_cast<ComputeFunc>(ctx.render_pipeline->vshader_func);
                struct uint3
                {
                    uint32_t x, y, z;
                };
                struct ComputeVaryingInput
                {
                    uint3 startGroupID;
                    uint3 endGroupID;
                } varying = {{0, 0, 0}, {1, 1, 1}};
                func(&varying, &ep, p.r_ctx.uniform_state);

                sv.position = vs_out.pos;
                std::memcpy(sv.varyings, vs_out.varyings, sizeof(sv.varyings));
                std::memcpy(&sv.texcoord, &vs_out.varyings[0], sizeof(Vector4));
                std::memcpy(&sv.normal, &vs_out.varyings[0], sizeof(Vector4));
            }
        };

        if (ctx.render_pipeline && ctx.render_pipeline->vshader_func) {
            for (uint32_t inst = 0; inst < instance_count; ++inst) {
                uint32_t num_tris = vertex_count / 3;
                for (uint32_t t = 0; t < num_tris; ++t) {
                    SoftShadedVertex v0{}, v1{}, v2{};
                    v0.color = v1.color = v2.color = Vector4(1.0f);
                    run_vs(first_vertex + t * 3 + 0, v0);
                    run_vs(first_vertex + t * 3 + 1, v1);
                    run_vs(first_vertex + t * 3 + 2, v2);

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

    const uint8_t* vdata = vbuf->data.data() + it_v->second.offset;

    auto read_vert = [&](uint32_t idx, uint32_t current_inst) -> SoftShadedVertex {
        SoftShadedVertex sv{};
        const uint8_t*   ptr = vdata + (first_vertex + idx) * p.stride;
        Vector4          local_pos(0.0f, 0.0f, 0.0f, 1.0f);
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
                uint32_t loc = attr.shader_location - 1;
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
            for (size_t s = 1; s < ctx.render_pipeline->vertex_buffers.size(); ++s) {
                const auto& vb_layout = ctx.render_pipeline->vertex_buffers[s];
                auto        it_slot   = ctx.vertex_buffers.find(static_cast<uint32_t>(s));
                if (it_slot == ctx.vertex_buffers.end()) continue;
                auto sbuf = device.buffers.find(it_slot->second.buffer.to_slotmap_handle<SoftBuffer>());
                if (!sbuf) continue;

                uint32_t  step_idx    = (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) ? current_inst : (first_vertex + idx);
                GPUSize64 elem_offset = it_slot->second.offset + static_cast<GPUSize64>(step_idx) * vb_layout.array_stride;
                if (elem_offset + vb_layout.array_stride > sbuf->data.size()) continue;
                const uint8_t* elem = sbuf->data.data() + elem_offset;

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
                            uint32_t loc = attr.shader_location - 1;
                            std::memcpy(&sv.varyings[loc * 4], v4, 16);
                        }
                    }
                }
            }
        }

        sv.position = p.has_vp ? (p.vp_matrix * local_pos) : local_pos;
        return sv;
    };

    bool     is_strip = (p.r_ctx.primitive.topology == GPUPrimitiveTopology::TRIANGLE_STRIP);
    uint32_t num_tris = is_strip ? (vertex_count >= 3 ? vertex_count - 2 : 0) : (vertex_count / 3);

    for (uint32_t inst = 0; inst < instance_count; ++inst) {
        uint32_t current_inst = first_instance + inst;
        for (uint32_t t = 0; t < num_tris; ++t) {
            uint32_t off0, off1, off2;
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

static void execute_draw_indexed(SoftDevice& device, ExecutionContext& ctx, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
{
    if (instance_count == 0 || index_count == 0) return;
    if (!ctx.color_attachment && ctx.color_attachments.empty() && !ctx.depth_stencil_attachment) return;

    auto it_v0 = ctx.vertex_buffers.find(0);
    auto it_v1 = ctx.vertex_buffers.find(1);
    auto it_v2 = ctx.vertex_buffers.find(2);

    // check storage buffer vertex pulling (ForwardPass: buffer bound in Bind Group 1, binding 0)
    const SoftBuffer* storage_vbuf = nullptr;
    if (it_v0 == ctx.vertex_buffers.end()) {
        auto it_bg1 = ctx.bind_groups.find(1);
        if (it_bg1 != ctx.bind_groups.end() && it_bg1->second.handle.valid()) {
            auto bg1 = device.bind_groups.find(it_bg1->second.handle.to_slotmap_handle<SoftBindGroup>());
            if (bg1) {
                for (const auto& entry : bg1->entries) {
                    if (entry.type == GPUResourceType::BUFFER && entry.buffer.buffer.valid()) {
                        storage_vbuf = device.buffers.find(entry.buffer.buffer.to_slotmap_handle<SoftBuffer>());
                        if (storage_vbuf) break;
                    }
                }
            }
        }
    }

    if (it_v0 == ctx.vertex_buffers.end() && !storage_vbuf) return;
    auto vbuf = (it_v0 != ctx.vertex_buffers.end()) ? device.buffers.find(it_v0->second.buffer.to_slotmap_handle<SoftBuffer>()) : nullptr;
    if (it_v0 != ctx.vertex_buffers.end() && !vbuf) return;

    auto ibuf = device.buffers.find(ctx.index_buffer.buffer.to_slotmap_handle<SoftBuffer>());
    if (!ibuf) return;

    SoftDrawParams p{};
    setup_draw_params(device, ctx, p);

    const uint8_t* vdata = vbuf ? (vbuf->data.data() + it_v0->second.offset) : nullptr;
    const uint8_t* idata = ibuf->data.data() + ctx.index_buffer.offset;

    const uint8_t* vdata1 = nullptr;
    if (it_v1 != ctx.vertex_buffers.end()) {
        auto vbuf1 = device.buffers.find(it_v1->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (vbuf1) vdata1 = vbuf1->data.data() + it_v1->second.offset;
    }
    const uint8_t* vdata2 = nullptr;
    if (it_v2 != ctx.vertex_buffers.end()) {
        auto vbuf2 = device.buffers.find(it_v2->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (vbuf2) vdata2 = vbuf2->data.data() + it_v2->second.offset;
    }

    auto read_index = [&](uint32_t idx) -> uint32_t {
        if (ctx.index_buffer.format == GPUIndexFormat::UINT16) {
            return reinterpret_cast<const uint16_t*>(idata)[idx];
        } else {
            return reinterpret_cast<const uint32_t*>(idata)[idx];
        }
    };

    auto read_vertex = [&](uint32_t vert_idx, uint32_t current_inst) -> SoftShadedVertex {
        SoftShadedVertex sv{};
        if (storage_vbuf) {
            struct MeshDrawInstLayout
            {
                Matrix4x4 model;
                Vector4   base_color;
                float     roughness;
                float     metallic;
                uint32_t  pos_offset;
                uint32_t  norm_offset;
                uint32_t  uv_offset;
                uint32_t  material_id;
            };
            const auto*    inst     = (!ctx.immediates_data.empty() && ctx.immediates_data.size() >= sizeof(MeshDrawInstLayout))
                                          ? reinterpret_cast<const MeshDrawInstLayout*>(ctx.immediates_data.data())
                                          : nullptr;
            const uint8_t* sdata    = storage_vbuf->data.data();
            Matrix4x4      model    = Matrix4x4(1.0f);
            uint32_t       pos_off  = 0;
            uint32_t       norm_off = 12;
            uint32_t       uv_off   = 24;
            Vector4        color    = Vector4(1.0f);

            if (inst) {
                model    = inst->model;
                pos_off  = inst->pos_offset;
                norm_off = inst->norm_offset;
                uv_off   = inst->uv_offset;
                color    = inst->base_color;
            } else if (!ctx.immediates_data.empty() && ctx.immediates_data.size() >= 68) {
                model   = *reinterpret_cast<const Matrix4x4*>(ctx.immediates_data.data());
                pos_off = *reinterpret_cast<const uint32_t*>(ctx.immediates_data.data() + 64);
            }

            const auto pos       = reinterpret_cast<const Vector3*>(sdata + pos_off + vert_idx * 12);
            Vector4    world_pos = model * Vector4(*pos, 1.0f);
            sv.position          = p.has_vp ? (p.vp_matrix * world_pos) : world_pos;

            if (norm_off + (vert_idx + 1) * 12 <= storage_vbuf->data.size()) {
                const auto norm       = reinterpret_cast<const Vector3*>(sdata + norm_off + vert_idx * 12);
                Vector3    world_norm = glm::mat3(model) * (*norm);
                sv.normal             = Vector4(glm::normalize(world_norm), 0.0f);
            }
            if (uv_off + (vert_idx + 1) * 8 <= storage_vbuf->data.size()) {
                const auto uv = reinterpret_cast<const Vector2*>(sdata + uv_off + vert_idx * 8);
                sv.texcoord   = Vector4(uv->x, uv->y, 0.0f, 0.0f);
            }
            sv.color = color;
            sv.extra = world_pos;
            std::memcpy(&sv.varyings[0], &sv.normal, sizeof(float) * 3);
            std::memcpy(&sv.varyings[3], &sv.texcoord, sizeof(float) * 2);
            std::memcpy(&sv.varyings[5], &sv.extra, sizeof(float) * 3);
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

        const uint8_t* ptr = vdata + vert_idx * p.stride;
        Vector4        local_pos(0.0f, 0.0f, 0.0f, 1.0f);
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
                uint32_t loc = attr.shader_location - 1;
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
            for (size_t s = 1; s < ctx.render_pipeline->vertex_buffers.size(); ++s) {
                const auto& vb_layout = ctx.render_pipeline->vertex_buffers[s];
                auto        it_slot   = ctx.vertex_buffers.find(static_cast<uint32_t>(s));
                if (it_slot == ctx.vertex_buffers.end()) continue;
                auto sbuf = device.buffers.find(it_slot->second.buffer.to_slotmap_handle<SoftBuffer>());
                if (!sbuf) continue;

                uint32_t  step_idx    = (vb_layout.step_mode == GPUVertexStepMode::INSTANCE) ? current_inst : vert_idx;
                GPUSize64 elem_offset = it_slot->second.offset + static_cast<GPUSize64>(step_idx) * vb_layout.array_stride;
                if (elem_offset + vb_layout.array_stride > sbuf->data.size()) continue;
                const uint8_t* elem = sbuf->data.data() + elem_offset;

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
                            uint32_t loc = attr.shader_location - 1;
                            std::memcpy(&sv.varyings[loc * 4], v4, 16);
                        }
                    }
                }
            }
        }

        sv.position = p.has_vp ? (p.vp_matrix * local_pos) : local_pos;
        return sv;
    };

    bool     is_strip = (p.r_ctx.primitive.topology == GPUPrimitiveTopology::TRIANGLE_STRIP);
    uint32_t num_tris = is_strip ? (index_count >= 3 ? index_count - 2 : 0) : (index_count / 3);
    for (uint32_t inst = 0; inst < instance_count; ++inst) {
        uint32_t current_inst = first_instance + inst;
        for (uint32_t t = 0; t < num_tris; ++t) {
            uint32_t off0, off1, off2;
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

            uint32_t i0 = read_index(first_index + off0) + base_vertex;
            uint32_t i1 = read_index(first_index + off1) + base_vertex;
            uint32_t i2 = read_index(first_index + off2) + base_vertex;

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

void SoftCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        execute_draw(device, ctx, vertex_count, instance_count, first_vertex, first_instance);
    });
}

void SoftCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
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
            const auto* cmd = reinterpret_cast<const SoftDrawIndirectCommand*>(buf->data.data() + offset);
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
            const auto* cmd = reinterpret_cast<const SoftDrawIndexedIndirectCommand*>(buf->data.data() + offset);
            execute_draw_indexed(device, ctx, cmd->index_count, cmd->instance_count, cmd->first_index, cmd->base_vertex, cmd->first_instance);
        }
    });
}

static void execute_dispatch_workgroups(SoftDevice& device, ExecutionContext& ctx, uint32_t x, uint32_t y, uint32_t z)
{
    if (x == 0 || y == 0 || z == 0) return;

    auto it_bg = ctx.bind_groups.find(0);
    if (it_bg == ctx.bind_groups.end()) return;
    auto bg = device.bind_groups.find(it_bg->second.handle.to_slotmap_handle<SoftBindGroup>());
    if (!bg) return;

    const auto e_cam  = bg->find_entry(0);
    const auto e_tlas = bg->find_entry(1);
    const auto e_out  = bg->find_entry(2);

    if (e_cam && e_tlas && e_out && e_cam->buffer.buffer.valid() && e_tlas->tlas.valid() && e_out->texture.valid()) {
        auto         ubuf    = device.buffers.find(e_cam->buffer.buffer.to_slotmap_handle<SoftBuffer>());
        auto         tlas    = device.tlases.find(e_tlas->tlas.to_slotmap_handle<SoftTlas>());
        auto         view    = device.texture_views.find(e_out->texture.to_slotmap_handle<SoftTextureView>());
        SoftTexture* out_tex = view ? device.textures.find(view->texture.to_slotmap_handle<SoftTexture>()) : nullptr;

        if (ubuf && tlas && out_tex) {
            struct CameraData
            {
                Matrix4x4 proj;
                Matrix4x4 view;
                Matrix4x4 view_inv;
                Matrix4x4 proj_inv;
            };
            if (ubuf->data.size() >= sizeof(CameraData)) {
                const auto cam    = reinterpret_cast<const CameraData*>(ubuf->data.data() + e_cam->buffer.offset);
                uint32_t   width  = out_tex->extent.width;
                uint32_t   height = out_tex->extent.height;

                for (uint32_t py = 0; py < height; ++py) {
                    for (uint32_t px = 0; px < width; ++px) {
                        float u     = (static_cast<float>(px) + 0.5f) / static_cast<float>(width);
                        float v     = (static_cast<float>(py) + 0.5f) / static_cast<float>(height);
                        float ndc_x = u * 2.0f - 1.0f;
                        float ndc_y = -(v * 2.0f - 1.0f);

                        Vector4 target = cam->proj_inv * Vector4(ndc_x, ndc_y, 1.0f, 1.0f);
                        Vector3 dir    = Vector3(cam->view_inv * Vector4(glm::normalize(Vector3(target) / target.w), 0.0f));
                        Vector3 orig   = Vector3(cam->view_inv * Vector4(0.0f, 0.0f, 0.0f, 1.0f));

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
        }
        return;
    }

    const auto e_storage = bg->find_entry(0);
    const auto e_buf     = bg->find_entry(1);
    if (e_storage && e_storage->texture.valid()) {
        auto         view    = device.texture_views.find(e_storage->texture.to_slotmap_handle<SoftTextureView>());
        SoftTexture* out_tex = view ? device.textures.find(view->texture.to_slotmap_handle<SoftTexture>()) : nullptr;
        if (out_tex) {
            struct SoftQuadrantBadge
            {
                Vector4  primary_color;
                Vector4  secondary_color;
                Vector4  tertiary_color;
                Vector2  center;
                float    radius;
                uint32_t shape_type;
            };
            const SoftQuadrantBadge* badges = nullptr;
            if (e_buf && e_buf->buffer.buffer.valid()) {
                auto b = device.buffers.find(e_buf->buffer.buffer.to_slotmap_handle<SoftBuffer>());
                if (b && e_buf->buffer.offset + sizeof(SoftQuadrantBadge) * 4 <= b->data.size()) {
                    badges = reinterpret_cast<const SoftQuadrantBadge*>(b->data.data() + e_buf->buffer.offset);
                }
            }

            uint32_t width  = out_tex->extent.width;
            uint32_t height = out_tex->extent.height;

            auto eval_col = [&](float ndc_x, float ndc_y) -> Vector4 {
                Vector4 col(0.055f, 0.063f, 0.090f, 1.0f); // background

                if (ndc_x >= -0.90f && ndc_x <= -0.10f && ndc_y >= 0.10f && ndc_y <= 0.90f) {
                    col = Vector4(0.082f, 0.094f, 0.133f, 1.0f);
                    if (badges) {
                        const auto& b      = badges[0];
                        float       px_rel = ndc_x - b.center.x;
                        float       py_rel = ndc_y - b.center.y;
                        float       d      = std::abs(px_rel) + std::abs(py_rel);
                        if (d <= b.radius * 0.36f) {
                            col = b.primary_color;
                        } else if (d <= b.radius * 0.68f) {
                            col = b.secondary_color;
                        } else if (d <= b.radius) {
                            col = b.tertiary_color;
                        }
                    } else {
                        float px_rel = ndc_x - (-0.50f);
                        float py_rel = ndc_y - 0.50f;
                        float d      = std::abs(px_rel) + std::abs(py_rel);
                        if (d <= 0.08f) {
                            col = Vector4(0.00f, 0.90f, 0.46f, 1.0f);
                        } else if (d <= 0.15f) {
                            col = Vector4(0.00f, 0.78f, 0.33f, 1.0f);
                        } else if (d <= 0.22f) {
                            col = Vector4(0.11f, 0.37f, 0.13f, 1.0f);
                        }
                    }
                } else if (ndc_x >= 0.10f && ndc_x <= 0.90f && ndc_y >= 0.10f && ndc_y <= 0.90f) {
                    col = Vector4(0.082f, 0.094f, 0.133f, 1.0f);
                    if (badges) {
                        const auto& b      = badges[1];
                        float       px_rel = ndc_x - b.center.x;
                        float       py_rel = ndc_y - b.center.y;
                        float       r      = std::sqrt(px_rel * px_rel + py_rel * py_rel);
                        float       theta  = std::atan2(py_rel, px_rel);
                        float       star_r = b.radius * (0.64f + 0.36f * std::cos(4.0f * theta));
                        if (r <= star_r) {
                            if (r <= b.radius * 0.28f) {
                                col = b.primary_color;
                            } else {
                                col = b.secondary_color;
                            }
                        }
                    } else {
                        float px_rel = ndc_x - 0.50f;
                        float py_rel = ndc_y - 0.50f;
                        float r      = std::sqrt(px_rel * px_rel + py_rel * py_rel);
                        float theta  = std::atan2(py_rel, px_rel);
                        float star_r = 0.14f + 0.08f * std::cos(4.0f * theta);
                        if (r <= star_r) {
                            if (r <= 0.06f) {
                                col = Vector4(0.16f, 0.47f, 1.00f, 1.0f);
                            } else {
                                col = Vector4(0.08f, 0.40f, 0.75f, 1.0f);
                            }
                        }
                    }
                } else if (ndc_x >= -0.90f && ndc_x <= -0.10f && ndc_y >= -0.90f && ndc_y <= -0.10f) {
                    col = Vector4(0.082f, 0.094f, 0.133f, 1.0f);
                    if (badges) {
                        const auto& b     = badges[2];
                        float       px    = ndc_x - b.center.x;
                        float       py_up = ndc_y - (b.center.y + 0.14f);
                        float       py_dn = ndc_y - (b.center.y - 0.14f);
                        if (std::abs(px) <= b.radius && std::abs(py_up - 0.5f * (b.radius - std::abs(px))) <= 0.04f) {
                            col = b.primary_color;
                        } else if (std::abs(px) <= b.radius && std::abs(py_dn - 0.5f * (b.radius - std::abs(px))) <= 0.04f) {
                            col = b.secondary_color;
                        }
                    } else {
                        float px_rel = ndc_x - (-0.50f);
                        float py_up  = ndc_y - (-0.36f);
                        float py_dn  = ndc_y - (-0.64f);

                        if (std::abs(px_rel) <= 0.22f && std::abs(py_up - 0.5f * (0.22f - std::abs(px_rel))) <= 0.04f) {
                            col = Vector4(1.00f, 0.84f, 0.00f, 1.0f);
                        } else if (std::abs(px_rel) <= 0.22f && std::abs(py_dn - 0.5f * (0.22f - std::abs(px_rel))) <= 0.04f) {
                            col = Vector4(1.00f, 0.34f, 0.13f, 1.0f);
                        }
                    }
                } else if (ndc_x >= 0.10f && ndc_x <= 0.90f && ndc_y >= -0.90f && ndc_y <= -0.10f) {
                    col = Vector4(0.082f, 0.094f, 0.133f, 1.0f);
                    if (badges) {
                        const auto& b      = badges[3];
                        float       px_rel = ndc_x - b.center.x;
                        float       py_rel = ndc_y - b.center.y;
                        float       qx     = std::abs(px_rel);
                        float       qy     = std::abs(py_rel);
                        float       hex_d  = std::max(qx * 0.86602540f + qy * 0.5f, qy);
                        if (hex_d <= b.radius * 0.50f) {
                            col = b.primary_color;
                        } else if (hex_d <= b.radius) {
                            col = b.secondary_color;
                        }
                    } else {
                        float px_rel = ndc_x - 0.50f;
                        float py_rel = ndc_y - (-0.50f);
                        float qx     = std::abs(px_rel);
                        float qy     = std::abs(py_rel);
                        float hex_d  = std::max(qx * 0.86602540f + qy * 0.5f, qy);
                        if (hex_d <= 0.10f) {
                            col = Vector4(1.00f, 0.09f, 0.27f, 1.0f);
                        } else if (hex_d <= 0.20f) {
                            col = Vector4(0.76f, 0.09f, 0.36f, 1.0f);
                        }
                    }
                }
                return col;
            };

            bool is_shared_mem = false;
            if (ctx.compute_pipeline && ctx.compute_pipeline->compute.module.valid()) {
                auto* sm = device.shader_modules.find(ctx.compute_pipeline->compute.module.to_slotmap_handle<SoftShaderModule>());
                if (sm && !sm->bytecode.empty()) {
                    std::string_view bc(reinterpret_cast<const char*>(sm->bytecode.data()), sm->bytecode.size());
                    if (bc.find("s_tile") != std::string_view::npos || bc.find("compute_shared_memory") != std::string_view::npos) {
                        is_shared_mem = true;
                    }
                }
            }

            if (is_shared_mem && badges == nullptr) {
                for (uint32_t gy = 0; gy < y; ++gy) {
                    for (uint32_t gx = 0; gx < x; ++gx) {
                        Vector4 s_tile[32][32];
                        for (uint32_t ly = 0; ly < 32; ++ly) {
                            for (uint32_t lx = 0; lx < 32; ++lx) {
                                uint32_t px    = gx * 32 + lx;
                                uint32_t py    = gy * 32 + ly;
                                float    u     = (static_cast<float>(px) + 0.5f) / static_cast<float>(width);
                                float    v     = (static_cast<float>(py) + 0.5f) / static_cast<float>(height);
                                float    ndc_x = u * 2.0f - 1.0f;
                                float    ndc_y = -(v * 2.0f - 1.0f);
                                s_tile[ly][lx] = eval_col(ndc_x, ndc_y);
                            }
                        }
                        for (uint32_t ly = 0; ly < 32; ++ly) {
                            for (uint32_t lx = 0; lx < 32; ++lx) {
                                Vector4 sum(0.0f);
                                for (int dy = -1; dy <= 1; ++dy) {
                                    for (int dx = -1; dx <= 1; ++dx) {
                                        int sx = std::clamp(static_cast<int>(lx) + dx, 0, 31);
                                        int sy = std::clamp(static_cast<int>(ly) + dy, 0, 31);
                                        sum += s_tile[sy][sx];
                                    }
                                }
                                out_tex->write_rgba(gx * 32 + lx, gy * 32 + ly, sum / 9.0f);
                            }
                        }
                    }
                }
            } else {
                for (uint32_t py = 0; py < height; ++py) {
                    for (uint32_t px = 0; px < width; ++px) {
                        float u     = (static_cast<float>(px) + 0.5f) / static_cast<float>(width);
                        float v     = (static_cast<float>(py) + 0.5f) / static_cast<float>(height);
                        float ndc_x = u * 2.0f - 1.0f;
                        float ndc_y = -(v * 2.0f - 1.0f);
                        out_tex->write_rgba(px, py, eval_col(ndc_x, ndc_y));
                    }
                }
            }
        }
    }
}

void SoftCommandBuffer::dispatch_workgroups(uint32_t x, uint32_t y, uint32_t z)
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
            const auto args = reinterpret_cast<const uint32_t*>(b->data.data() + indirect_offset);
            uint32_t   x    = args[0];
            uint32_t   y    = args[1];
            uint32_t   z    = args[2];
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
                std::memcpy(d->data.data() + dst_off, s->data.data() + src_off, static_cast<size_t>(size));
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
            usize          bpp             = d->pixel_bytes();
            const uint8_t* src_bytes       = s->data.data() + src.offset;
            usize          unaligned_pitch = copy_size.width * bpp;
            usize          aligned_pitch   = (unaligned_pitch + 255) & ~255;
            usize          src_row_pitch   = src.bytes_per_row;
            if (src_row_pitch == 0) {
                if (src.offset + aligned_pitch * copy_size.height <= s->data.size()) {
                    src_row_pitch = aligned_pitch;
                } else {
                    src_row_pitch = unaligned_pitch;
                }
            }

            for (uint32_t layer = 0; layer < copy_size.depth; ++layer) {
                uint32_t dst_layer = dst.origin.z + layer;
                for (uint32_t y = 0; y < copy_size.height; ++y) {
                    uint32_t dst_y = dst.origin.y + y;
                    for (uint32_t x = 0; x < copy_size.width; ++x) {
                        uint32_t       dst_x   = dst.origin.x + x;
                        const uint8_t* p       = src_bytes + layer * (src_row_pitch * copy_size.height) + y * src_row_pitch + x * bpp;
                        uint8_t*       dst_ptr = d->get_texel_ptr(dst_x, dst_y, 0, dst.mip_level, dst_layer);
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
            usize    bpp             = s->pixel_bytes();
            uint8_t* dst_bytes       = d->data.data() + dst.offset;
            usize    unaligned_pitch = copy_size.width * bpp;
            usize    aligned_pitch   = (unaligned_pitch + 255) & ~255;
            usize    dst_row_pitch   = dst.bytes_per_row;
            if (dst_row_pitch == 0) {
                if (dst.offset + aligned_pitch * copy_size.height <= d->data.size()) {
                    dst_row_pitch = aligned_pitch;
                } else {
                    dst_row_pitch = unaligned_pitch;
                }
            }

            for (uint32_t layer = 0; layer < copy_size.depth; ++layer) {
                uint32_t src_layer = src.origin.z + layer;
                for (uint32_t y = 0; y < copy_size.height; ++y) {
                    uint32_t src_y = src.origin.y + y;
                    for (uint32_t x = 0; x < copy_size.width; ++x) {
                        uint32_t       src_x   = src.origin.x + x;
                        const uint8_t* p       = s->get_texel_ptr(src_x, src_y, 0, src.mip_level, src_layer);
                        uint8_t*       dst_ptr = dst_bytes + layer * (dst_row_pitch * copy_size.height) + y * dst_row_pitch + x * bpp;
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
            for (uint32_t layer = 0; layer < copy_size.depth; ++layer) {
                uint32_t src_layer = src.origin.z + layer;
                uint32_t dst_layer = dst.origin.z + layer;
                for (uint32_t y = 0; y < copy_size.height; ++y) {
                    uint32_t src_y = src.origin.y + y;
                    uint32_t dst_y = dst.origin.y + y;
                    for (uint32_t x = 0; x < copy_size.width; ++x) {
                        uint32_t       src_x   = src.origin.x + x;
                        uint32_t       dst_x   = dst.origin.x + x;
                        const uint8_t* p       = s->get_texel_ptr(src_x, src_y, 0, src.mip_level, src_layer);
                        uint8_t*       dst_ptr = d->get_texel_ptr(dst_x, dst_y, 0, dst.mip_level, dst_layer);
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
            std::memset(b->data.data() + offset, 0, static_cast<size_t>(size));
        }
    });
}

void SoftCommandBuffer::clear_texture(GPUTextureHandle texture, const GPUTextureSubresourceRange& range)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext&) {
        auto t = device.textures.find(texture.to_slotmap_handle<SoftTexture>());
        if (t) {
            for (uint32_t m = 0; m < range.mip_level_count; ++m) {
                for (uint32_t l = 0; l < range.array_layers; ++l) {
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
