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
    commands.push_back([cas = std::move(cas), ds](SoftDevice& device, ExecutionContext& ctx) {
        ctx.color_attachment         = nullptr;
        ctx.depth_stencil_attachment = nullptr;

        for (const auto& ca : cas) {
            if (!ca.view.valid()) continue;
            auto view = device.texture_views.find(ca.view.to_slotmap_handle<SoftTextureView>());
            if (!view) continue;
            auto tex = device.textures.find(view->texture.to_slotmap_handle<SoftTexture>());
            if (!tex) continue;

            if (ca.load_op == GPULoadOp::CLEAR) {
                tex->clear_color(ca.clear_value, view->base_mip_level, view->base_array_layer);
            }
            ctx.color_attachment = tex;
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
        ctx.color_attachment         = nullptr;
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
};

static SoftDrawParams setup_draw_params(SoftDevice& device, const ExecutionContext& ctx)
{
    SoftDrawParams p{};
    p.r_ctx.color_attachment         = ctx.color_attachment;
    p.r_ctx.depth_stencil_attachment = ctx.depth_stencil_attachment;
    p.r_ctx.viewport                 = ctx.viewport;
    p.r_ctx.scissor                  = ctx.scissor;
    p.r_ctx.stencil_reference        = ctx.stencil_reference;

    if (ctx.render_pipeline) {
        p.r_ctx.primitive     = ctx.render_pipeline->primitive;
        p.r_ctx.depth_stencil = ctx.render_pipeline->depth_stencil;
        if (!ctx.render_pipeline->color_targets.empty()) {
            p.r_ctx.color_target = ctx.render_pipeline->color_targets[0];
        }
    }

    // 1. matrix from immediates
    if (!ctx.immediates_data.empty() && ctx.immediates_data.size() >= sizeof(Matrix4x4)) {
        p.vp_matrix = *reinterpret_cast<const Matrix4x4*>(ctx.immediates_data.data());
        p.has_vp    = true;
    }

    // 2. check bind groups (texture, sampler, uniform buffer)
    const SoftTextureView* bound_view = nullptr;
    const SoftSampler*     bound_samp = nullptr;

    for (const auto& [slot, bound_bg] : ctx.bind_groups) {
        if (!bound_bg.handle.valid()) continue;
        auto bg = device.bind_groups.find(bound_bg.handle.to_slotmap_handle<SoftBindGroup>());
        if (!bg) continue;

        for (const auto& entry : bg->entries) {
            if (entry.type == GPUResourceType::BUFFER && entry.buffer.buffer.valid() && !p.has_vp) {
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

    if (p.bound_tex && bound_view && bound_samp) {
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

    p.r_ctx.fshader_func = ctx.render_pipeline ? ctx.render_pipeline->fshader_func : nullptr;

    // vertex layout
    p.stride     = 24;
    p.pos_offset = 0;
    p.col_offset = 12;
    p.uv_offset  = 12;

    if (ctx.render_pipeline && !ctx.render_pipeline->vertex_buffers.empty()) {
        const auto& vb = ctx.render_pipeline->vertex_buffers[0];
        p.stride       = vb.array_stride > 0 ? vb.array_stride : 24;
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

    return p;
}

void SoftCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        if (!ctx.color_attachment && !ctx.depth_stencil_attachment) return;

        auto it_v = ctx.vertex_buffers.find(0);
        if (it_v == ctx.vertex_buffers.end()) return;
        auto vbuf = device.buffers.find(it_v->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (!vbuf) return;

        SoftDrawParams p     = setup_draw_params(device, ctx);
        const uint8_t* vdata = vbuf->data.data() + it_v->second.offset;

        uint32_t num_tris = vertex_count / 3;
        for (uint32_t t = 0; t < num_tris; ++t) {
            auto read_vert = [&](uint32_t idx) -> SoftShadedVertex {
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
                sv.position = p.has_vp ? (p.vp_matrix * local_pos) : local_pos;

                if (p.has_uv || p.bound_tex != nullptr) {
                    const auto uv = reinterpret_cast<const Vector2*>(ptr + p.uv_offset);
                    sv.texcoord   = Vector4(uv->x, uv->y, 0.0f, 0.0f);
                }
                if (p.has_col) {
                    if (p.col_format == GPUVertexFormat::UNORM8x4) {
                        const auto col = ptr + p.col_offset;
                        sv.color       = Vector4(col[0] / 255.0f, col[1] / 255.0f, col[2] / 255.0f, col[3] / 255.0f);
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
                return sv;
            };

            SoftShadedVertex v0 = read_vert(t * 3 + 0);
            SoftShadedVertex v1 = read_vert(t * 3 + 1);
            SoftShadedVertex v2 = read_vert(t * 3 + 2);

            Vector<SoftRasterTriangle> tris;
            SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
            for (const auto& tri : tris) {
                SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
            }
        }
    });
}

void SoftCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        if (!ctx.color_attachment && !ctx.depth_stencil_attachment) return;

        auto it_v = ctx.vertex_buffers.find(0);
        if (it_v == ctx.vertex_buffers.end()) return;
        auto vbuf = device.buffers.find(it_v->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (!vbuf) return;

        auto ibuf = device.buffers.find(ctx.index_buffer.buffer.to_slotmap_handle<SoftBuffer>());
        if (!ibuf) return;

        SoftDrawParams p = setup_draw_params(device, ctx);

        const uint8_t* vdata = vbuf->data.data() + it_v->second.offset;
        const uint8_t* idata = ibuf->data.data() + ctx.index_buffer.offset;

        auto read_index = [&](uint32_t idx) -> uint32_t {
            if (ctx.index_buffer.format == GPUIndexFormat::UINT16) {
                return reinterpret_cast<const uint16_t*>(idata)[idx];
            } else {
                return reinterpret_cast<const uint32_t*>(idata)[idx];
            }
        };

        auto read_vertex = [&](uint32_t vert_idx) -> SoftShadedVertex {
            SoftShadedVertex sv{};
            const uint8_t*   ptr = vdata + vert_idx * p.stride;
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
            sv.position = p.has_vp ? (p.vp_matrix * local_pos) : local_pos;

            if (p.has_uv || p.bound_tex != nullptr) {
                const auto uv = reinterpret_cast<const Vector2*>(ptr + p.uv_offset);
                sv.texcoord   = Vector4(uv->x, uv->y, 0.0f, 0.0f);
            }
            if (p.has_col) {
                if (p.col_format == GPUVertexFormat::UNORM8x4) {
                    const auto col = ptr + p.col_offset;
                    sv.color       = Vector4(col[0] / 255.0f, col[1] / 255.0f, col[2] / 255.0f, col[3] / 255.0f);
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
            return sv;
        };

        uint32_t num_tris = index_count / 3;
        for (uint32_t t = 0; t < num_tris; ++t) {
            uint32_t i0 = read_index(first_index + t * 3 + 0) + base_vertex;
            uint32_t i1 = read_index(first_index + t * 3 + 1) + base_vertex;
            uint32_t i2 = read_index(first_index + t * 3 + 2) + base_vertex;

            SoftShadedVertex v0 = read_vertex(i0);
            SoftShadedVertex v1 = read_vertex(i1);
            SoftShadedVertex v2 = read_vertex(i2);

            Vector<SoftRasterTriangle> tris;
            SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
            for (const auto& tri : tris) {
                SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
            }
        }
    });
}

void SoftCommandBuffer::dispatch_workgroups(uint32_t x, uint32_t y, uint32_t z)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
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
