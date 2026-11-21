#include "SoftRasterizer.h"
#if defined(__AVX2__)
#include <immintrin.h>
#define LYRA_USE_AVX2 1
#endif

using namespace lyra;
using namespace lyra::softgpu;

static SoftShadedVertex interpolate_vertex(const SoftShadedVertex& a, const SoftShadedVertex& b, float t)
{
    SoftShadedVertex v;
    v.position = glm::mix(a.position, b.position, t);
    v.color    = glm::mix(a.color, b.color, t);
    v.texcoord = glm::mix(a.texcoord, b.texcoord, t);
    v.normal   = glm::mix(a.normal, b.normal, t);
    v.extra    = glm::mix(a.extra, b.extra, t);
    for (int k = 0; k < 16; ++k) {
        v.varyings[k] = a.varyings[k] + t * (b.varyings[k] - a.varyings[k]);
    }
    return v;
}

static void project_vertex(SoftShadedVertex& v, const SoftViewport& vp)
{
    v.inv_w = (std::abs(v.position.w) > 1e-8f) ? (1.0f / v.position.w) : 1.0f;
    Vector3 ndc(v.position.x * v.inv_w, v.position.y * v.inv_w, v.position.z * v.inv_w);

    v.screen.x = (ndc.x + 1.0f) * 0.5f * vp.width + vp.x;
    v.screen.y = (-ndc.y + 1.0f) * 0.5f * vp.height + vp.y;
    v.screen.z = vp.min_depth + ndc.z * (vp.max_depth - vp.min_depth);
}

void SoftRasterizer::clip_and_project_triangle(const SoftShadedVertex& v0, const SoftShadedVertex& v1, const SoftShadedVertex& v2,
    const SoftViewport& vp, Vector<SoftRasterTriangle>& out_triangles)
{
    // clip against near plane W >= 0.0001f
    constexpr float near_plane = 0.0001f;

    const SoftShadedVertex* in_verts[3] = {&v0, &v1, &v2};
    bool                    inside[3]   = {
        v0.position.w >= near_plane,
        v1.position.w >= near_plane,
        v2.position.w >= near_plane,
    };

    int in_count = inside[0] + inside[1] + inside[2];
    if (in_count == 0) return; // completely behind near plane

    if (in_count == 3) {
        SoftRasterTriangle tri;
        tri.v0 = v0;
        tri.v1 = v1;
        tri.v2 = v2;
        project_vertex(tri.v0, vp);
        project_vertex(tri.v1, vp);
        project_vertex(tri.v2, vp);
        out_triangles.push_back(tri);
        return;
    }

    if (in_count == 1) {
        // 1 vertex inside, 2 outside -> 1 triangle
        int in_idx = inside[0] ? 0 : (inside[1] ? 1 : 2);
        int out0   = (in_idx + 1) % 3;
        int out1   = (in_idx + 2) % 3;

        float t0 = (near_plane - in_verts[in_idx]->position.w) / (in_verts[out0]->position.w - in_verts[in_idx]->position.w);
        float t1 = (near_plane - in_verts[in_idx]->position.w) / (in_verts[out1]->position.w - in_verts[in_idx]->position.w);

        SoftRasterTriangle tri;
        tri.v0 = *in_verts[in_idx];
        tri.v1 = interpolate_vertex(*in_verts[in_idx], *in_verts[out0], t0);
        tri.v2 = interpolate_vertex(*in_verts[in_idx], *in_verts[out1], t1);

        project_vertex(tri.v0, vp);
        project_vertex(tri.v1, vp);
        project_vertex(tri.v2, vp);
        out_triangles.push_back(tri);
    } else if (in_count == 2) {
        // 2 vertices inside, 1 outside -> quad split into 2 triangles
        int out_idx = !inside[0] ? 0 : (!inside[1] ? 1 : 2);
        int in0     = (out_idx + 1) % 3;
        int in1     = (out_idx + 2) % 3;

        float t0 = (near_plane - in_verts[in0]->position.w) / (in_verts[out_idx]->position.w - in_verts[in0]->position.w);
        float t1 = (near_plane - in_verts[in1]->position.w) / (in_verts[out_idx]->position.w - in_verts[in1]->position.w);

        SoftShadedVertex new0 = interpolate_vertex(*in_verts[in0], *in_verts[out_idx], t0);
        SoftShadedVertex new1 = interpolate_vertex(*in_verts[in1], *in_verts[out_idx], t1);

        SoftRasterTriangle tri1;
        tri1.v0 = *in_verts[in0];
        tri1.v1 = *in_verts[in1];
        tri1.v2 = new0;

        SoftRasterTriangle tri2;
        tri2.v0 = new0;
        tri2.v1 = *in_verts[in1];
        tri2.v2 = new1;

        project_vertex(tri1.v0, vp);
        project_vertex(tri1.v1, vp);
        project_vertex(tri1.v2, vp);

        project_vertex(tri2.v0, vp);
        project_vertex(tri2.v1, vp);
        project_vertex(tri2.v2, vp);

        out_triangles.push_back(tri1);
        out_triangles.push_back(tri2);
    }
}

static bool test_depth(float src_depth, float dst_depth, GPUCompareFunction comp)
{
    switch (comp) {
        case GPUCompareFunction::NEVER:
            return false;
        case GPUCompareFunction::LESS:
            return src_depth < dst_depth;
        case GPUCompareFunction::EQUAL:
            return std::abs(src_depth - dst_depth) < 1e-6f;
        case GPUCompareFunction::LESS_EQUAL:
            return src_depth <= dst_depth + 1e-5f;
        case GPUCompareFunction::GREATER:
            return src_depth > dst_depth;
        case GPUCompareFunction::NOT_EQUAL:
            return std::abs(src_depth - dst_depth) >= 1e-6f;
        case GPUCompareFunction::GREATER_EQUAL:
            return src_depth >= dst_depth - 1e-5f;
        case GPUCompareFunction::ALWAYS:
            return true;
        default:
            return true;
    }
}

static bool test_stencil(uint8_t ref, uint8_t val, uint8_t read_mask, GPUCompareFunction comp)
{
    uint8_t r = ref & read_mask;
    uint8_t v = val & read_mask;
    switch (comp) {
        case GPUCompareFunction::NEVER:
            return false;
        case GPUCompareFunction::LESS:
            return r < v;
        case GPUCompareFunction::EQUAL:
            return r == v;
        case GPUCompareFunction::LESS_EQUAL:
            return r <= v;
        case GPUCompareFunction::GREATER:
            return r > v;
        case GPUCompareFunction::NOT_EQUAL:
            return r != v;
        case GPUCompareFunction::GREATER_EQUAL:
            return r >= v;
        case GPUCompareFunction::ALWAYS:
            return true;
        default:
            return true;
    }
}

static uint8_t apply_stencil_op(GPUStencilOperation op, uint8_t current, uint8_t ref, uint8_t write_mask)
{
    uint8_t result = current;
    switch (op) {
        case GPUStencilOperation::KEEP:
            result = current;
            break;
        case GPUStencilOperation::ZERO:
            result = 0;
            break;
        case GPUStencilOperation::REPLACE:
            result = ref;
            break;
        case GPUStencilOperation::INVERT:
            result = ~current;
            break;
        case GPUStencilOperation::INCREMENT_CLAMP:
            result = (current == 255) ? 255 : (current + 1);
            break;
        case GPUStencilOperation::DECREMENT_CLAMP:
            result = (current == 0) ? 0 : (current - 1);
            break;
        case GPUStencilOperation::INCREMENT_WRAP:
            result = current + 1;
            break;
        case GPUStencilOperation::DECREMENT_WRAP:
            result = current - 1;
            break;
    }
    return static_cast<uint8_t>((current & ~write_mask) | (result & write_mask));
}

static Vector4 apply_blend(const Vector4& src, const Vector4& dst, const GPUBlendState& blend)
{
    auto get_factor = [](GPUBlendFactor factor, const Vector4& s, const Vector4& d) -> Vector4 {
        switch (factor) {
            case GPUBlendFactor::ZERO:
                return Vector4(0.0f);
            case GPUBlendFactor::ONE:
                return Vector4(1.0f);
            case GPUBlendFactor::SRC:
                return s;
            case GPUBlendFactor::ONE_MINUS_SRC:
                return Vector4(1.0f) - s;
            case GPUBlendFactor::SRC_ALPHA:
                return Vector4(s.a);
            case GPUBlendFactor::ONE_MINUS_SRC_ALPHA:
                return Vector4(1.0f - s.a);
            case GPUBlendFactor::DST:
                return d;
            case GPUBlendFactor::ONE_MINUS_DST:
                return Vector4(1.0f) - d;
            case GPUBlendFactor::DST_ALPHA:
                return Vector4(d.a);
            case GPUBlendFactor::ONE_MINUS_DST_ALPHA:
                return Vector4(1.0f - d.a);
            default:
                return Vector4(1.0f);
        }
    };

    Vector4 src_rgb_f = get_factor(blend.color.src_factor, src, dst);
    Vector4 dst_rgb_f = get_factor(blend.color.dst_factor, src, dst);
    Vector4 src_a_f   = get_factor(blend.alpha.src_factor, src, dst);
    Vector4 dst_a_f   = get_factor(blend.alpha.dst_factor, src, dst);

    Vector3 rgb;
    if (blend.color.operation == GPUBlendOperation::ADD) {
        rgb = Vector3(src) * Vector3(src_rgb_f) + Vector3(dst) * Vector3(dst_rgb_f);
    } else if (blend.color.operation == GPUBlendOperation::SUBTRACT) {
        rgb = Vector3(src) * Vector3(src_rgb_f) - Vector3(dst) * Vector3(dst_rgb_f);
    } else if (blend.color.operation == GPUBlendOperation::REVERSE_SUBTRACT) {
        rgb = Vector3(dst) * Vector3(dst_rgb_f) - Vector3(src) * Vector3(src_rgb_f);
    } else if (blend.color.operation == GPUBlendOperation::MIN) {
        rgb = glm::min(Vector3(src), Vector3(dst));
    } else if (blend.color.operation == GPUBlendOperation::MAX) {
        rgb = glm::max(Vector3(src), Vector3(dst));
    } else {
        rgb = Vector3(src);
    }

    float alpha;
    if (blend.alpha.operation == GPUBlendOperation::ADD) {
        alpha = src.a * src_a_f.a + dst.a * dst_a_f.a;
    } else if (blend.alpha.operation == GPUBlendOperation::SUBTRACT) {
        alpha = src.a * src_a_f.a - dst.a * dst_a_f.a;
    } else if (blend.alpha.operation == GPUBlendOperation::REVERSE_SUBTRACT) {
        alpha = dst.a * dst_a_f.a - src.a * src_a_f.a;
    } else if (blend.alpha.operation == GPUBlendOperation::MIN) {
        alpha = std::min(src.a, dst.a);
    } else if (blend.alpha.operation == GPUBlendOperation::MAX) {
        alpha = std::max(src.a, dst.a);
    } else {
        alpha = src.a;
    }

    return Vector4(rgb, std::clamp(alpha, 0.0f, 1.0f));
}

void SoftRasterizer::rasterize_triangle(const SoftRasterTriangle& tri, const SoftRasterizerContext& ctx)
{
    const auto& v0 = tri.v0;
    const auto& v1 = tri.v1;
    const auto& v2 = tri.v2;

    // backface culling
    float signed_area = (v1.screen.x - v0.screen.x) * (v2.screen.y - v0.screen.y) -
                        (v1.screen.y - v0.screen.y) * (v2.screen.x - v0.screen.x);

    bool is_screen_ccw = (signed_area < 0.0f);
    bool is_front      = is_screen_ccw;
    if (ctx.primitive.front_face == GPUFrontFace::CW) is_front = !is_front;

    if (ctx.primitive.cull_mode == GPUCullMode::BACK && !is_front) return;
    if (ctx.primitive.cull_mode == GPUCullMode::FRONT && is_front) return;

    auto is_top_left = [](const Vector2& a, const Vector2& b, bool is_ccw) -> bool {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        if (is_ccw) {
            return (dy > 0.0f) || (dy == 0.0f && dx < 0.0f);
        } else {
            return (dy < 0.0f) || (dy == 0.0f && dx > 0.0f);
        }
    };

    bool tl0 = is_top_left(Vector2(v1.screen.x, v1.screen.y), Vector2(v2.screen.x, v2.screen.y), is_screen_ccw);
    bool tl1 = is_top_left(Vector2(v2.screen.x, v2.screen.y), Vector2(v0.screen.x, v0.screen.y), is_screen_ccw);
    bool tl2 = is_top_left(Vector2(v0.screen.x, v0.screen.y), Vector2(v1.screen.x, v1.screen.y), is_screen_ccw);

    float inv_area = 1.0f / signed_area;

    // bounding box
    float min_x_f = std::min({v0.screen.x, v1.screen.x, v2.screen.x});
    float max_x_f = std::max({v0.screen.x, v1.screen.x, v2.screen.x});
    float min_y_f = std::min({v0.screen.y, v1.screen.y, v2.screen.y});
    float max_y_f = std::max({v0.screen.y, v1.screen.y, v2.screen.y});

    int32_t sc_x0 = ctx.scissor.x;
    int32_t sc_x1 = ctx.scissor.x + ctx.scissor.width;
    int32_t sc_y0 = ctx.scissor.y;
    int32_t sc_y1 = ctx.scissor.y + ctx.scissor.height;

    int32_t min_x = std::max(sc_x0, static_cast<int32_t>(std::floor(min_x_f)));
    int32_t max_x = std::min(sc_x1 - 1, static_cast<int32_t>(std::ceil(max_x_f)));
    int32_t min_y = std::max(sc_y0, static_cast<int32_t>(std::floor(min_y_f)));
    int32_t max_y = std::min(sc_y1 - 1, static_cast<int32_t>(std::ceil(max_y_f)));

    if (min_x > max_x || min_y > max_y) return;

    // linear edge equation setup:
    // w0(px, py) = A0 * px + B0 * py + C0
    // w1(px, py) = A1 * px + B1 * py + C1
    // w2(px, py) = A2 * px + B2 * py + C2
    float A0 = (v1.screen.y - v2.screen.y) * inv_area;
    float B0 = (v2.screen.x - v1.screen.x) * inv_area;
    float C0 = (v1.screen.x * v2.screen.y - v1.screen.y * v2.screen.x) * inv_area;

    float A1 = (v2.screen.y - v0.screen.y) * inv_area;
    float B1 = (v0.screen.x - v2.screen.x) * inv_area;
    float C1 = (v2.screen.x * v0.screen.y - v2.screen.y * v0.screen.x) * inv_area;

    float A2 = (v0.screen.y - v1.screen.y) * inv_area;
    float B2 = (v1.screen.x - v0.screen.x) * inv_area;
    float C2 = (v0.screen.x * v1.screen.y - v0.screen.y * v1.screen.x) * inv_area;

    float len0 = std::hypot(A0, B0);
    float len1 = std::hypot(A1, B1);
    float len2 = std::hypot(A2, B2);

    constexpr float edge_px_eps = 0.002f;
    float           bias0       = (tl0 ? -edge_px_eps : edge_px_eps) * len0;
    float           bias1       = (tl1 ? -edge_px_eps : edge_px_eps) * len1;
    float           bias2       = (tl2 ? -edge_px_eps : edge_px_eps) * len2;

    auto process_pixel = [&](int32_t x, int32_t y, float px, float py, float w0, float w1, float w2, float z, float w) {
        // depth test
        bool depth_passed = true;
        if (ctx.depth_stencil_attachment && ctx.depth_stencil.depth_compare != GPUCompareFunction::ALWAYS) {
            float current_depth = ctx.depth_stencil_attachment->read_depth(x, y);
            if (!test_depth(z, current_depth, ctx.depth_stencil.depth_compare)) {
                depth_passed = false;
            }
        }

        // stencil test
        bool    stencil_passed = true;
        uint8_t curr_stencil   = ctx.depth_stencil_attachment ? ctx.depth_stencil_attachment->read_stencil(x, y) : 0;
        if (ctx.depth_stencil_attachment && ctx.depth_stencil.stencil_front.compare != GPUCompareFunction::ALWAYS) {
            stencil_passed = test_stencil(static_cast<uint8_t>(ctx.stencil_reference),
                curr_stencil,
                static_cast<uint8_t>(ctx.depth_stencil.stencil_read_mask),
                ctx.depth_stencil.stencil_front.compare);
        }

        if (!stencil_passed) {
            if (ctx.depth_stencil_attachment && ctx.depth_stencil.stencil_write_mask != 0 &&
                ctx.depth_stencil.stencil_front.fail_op != GPUStencilOperation::KEEP) {
                uint8_t new_val = apply_stencil_op(ctx.depth_stencil.stencil_front.fail_op,
                    curr_stencil,
                    static_cast<uint8_t>(ctx.stencil_reference),
                    static_cast<uint8_t>(ctx.depth_stencil.stencil_write_mask));
                ctx.depth_stencil_attachment->write_stencil(x, y, new_val);
            }
            return;
        }

        if (!depth_passed) {
            if (ctx.depth_stencil_attachment && ctx.depth_stencil.stencil_write_mask != 0 &&
                ctx.depth_stencil.stencil_front.depth_fail_op != GPUStencilOperation::KEEP) {
                uint8_t new_val = apply_stencil_op(ctx.depth_stencil.stencil_front.depth_fail_op,
                    curr_stencil,
                    static_cast<uint8_t>(ctx.stencil_reference),
                    static_cast<uint8_t>(ctx.depth_stencil.stencil_write_mask));
                ctx.depth_stencil_attachment->write_stencil(x, y, new_val);
            }
            return;
        }

        // write stencil
        if (ctx.depth_stencil_attachment && ctx.depth_stencil.stencil_write_mask != 0 &&
            ctx.depth_stencil.stencil_front.pass_op != GPUStencilOperation::KEEP) {
            uint8_t new_val = apply_stencil_op(ctx.depth_stencil.stencil_front.pass_op,
                curr_stencil,
                static_cast<uint8_t>(ctx.stencil_reference),
                static_cast<uint8_t>(ctx.depth_stencil.stencil_write_mask));
            ctx.depth_stencil_attachment->write_stencil(x, y, new_val);
        }

        // write depth
        if (ctx.depth_stencil_attachment && ctx.depth_stencil.depth_write_enabled) {
            ctx.depth_stencil_attachment->write_depth(x, y, z);
        }

        // write color with write mask
        bool has_colors = !ctx.color_attachments.empty() || ctx.color_attachment != nullptr;
        if (has_colors) {
            SoftShadedVertex interp;
            interp.position = Vector4(px, py, z, w);
            interp.screen   = Vector3(px, py, z);
            interp.color    = (w0 * v0.color * v0.inv_w + w1 * v1.color * v1.inv_w + w2 * v2.color * v2.inv_w) * w;
            interp.texcoord = (w0 * v0.texcoord * v0.inv_w + w1 * v1.texcoord * v1.inv_w + w2 * v2.texcoord * v2.inv_w) * w;
            interp.normal   = (w0 * v0.normal * v0.inv_w + w1 * v1.normal * v1.inv_w + w2 * v2.normal * v2.inv_w) * w;
            interp.extra    = (w0 * v0.extra * v0.inv_w + w1 * v1.extra * v1.inv_w + w2 * v2.extra * v2.inv_w) * w;
            for (int k = 0; k < 16; ++k) {
                interp.varyings[k] = (w0 * v0.varyings[k] * v0.inv_w + w1 * v1.varyings[k] * v1.inv_w + w2 * v2.varyings[k] * v2.inv_w) * w;
            }

            Vector4 out_colors[8] = {interp.color, Vector4(0.0f), Vector4(0.0f), Vector4(0.0f), Vector4(0.0f), Vector4(0.0f), Vector4(0.0f), Vector4(0.0f)};
            if (ctx.fragment_shader_mrt) {
                ctx.fragment_shader_mrt(interp, out_colors, 8);
            } else if (ctx.fshader_func) {
                struct uint3
                {
                    uint32_t x, y, z;
                };
                struct ComputeVaryingInput
                {
                    uint3 startGroupID;
                    uint3 endGroupID;
                } varying = {{0, 0, 0}, {1, 1, 1}};

                struct FragInput
                {
                    Vector4 pos;
                    float   varyings[16];
                } fin;
                fin.pos = interp.position;
                std::memcpy(fin.varyings, interp.varyings, sizeof(interp.varyings));

                Vector4 f_out[8] = {};
                struct EpParams
                {
                    const void* in_v;
                    void*       out_c;
                } ep = {&fin, f_out};

                using ComputeFunc = void (*)(ComputeVaryingInput*, void*, void*);
                auto func         = reinterpret_cast<ComputeFunc>(ctx.fshader_func);
                func(&varying, &ep, ctx.uniform_state);
                for (int i = 0; i < 8; ++i) {
                    out_colors[i] = f_out[i];
                }
            } else if (ctx.fragment_shader) {
                out_colors[0] = ctx.fragment_shader(interp);
            }

            if (!ctx.color_attachments.empty()) {
                for (size_t i = 0; i < ctx.color_attachments.size(); ++i) {
                    auto* tex = ctx.color_attachments[i];
                    if (!tex) continue;
                    const auto& ct = (i < ctx.color_targets.size()) ? ctx.color_targets[i] : ctx.color_target;
                    if (ct.write_mask.value == 0) continue;

                    Vector4 frag_color = out_colors[i];
                    Vector4 out_color  = frag_color;
                    if (ct.blend_enable) {
                        Vector4 dst = tex->read_rgba(x, y);
                        out_color   = apply_blend(frag_color, dst, ct.blend);
                    }

                    if (ct.write_mask == GPUColorWrite::ALL) {
                        if (tex->format == GPUTextureFormat::RGBA32UINT ||
                            tex->format == GPUTextureFormat::RGBA32SINT ||
                            tex->format == GPUTextureFormat::RGBA32FLOAT) {
                            uint8_t* ptr = tex->get_texel_ptr(x, y);
                            if (ptr) std::memcpy(ptr, &out_colors[i], 16);
                        } else {
                            tex->write_rgba(x, y, out_color);
                        }
                    } else {
                        Vector4 cur = tex->read_rgba(x, y);
                        if (ct.write_mask.contains(GPUColorWrite::RED)) cur.r = out_color.r;
                        if (ct.write_mask.contains(GPUColorWrite::GREEN)) cur.g = out_color.g;
                        if (ct.write_mask.contains(GPUColorWrite::BLUE)) cur.b = out_color.b;
                        if (ct.write_mask.contains(GPUColorWrite::ALPHA)) cur.a = out_color.a;
                        tex->write_rgba(x, y, cur);
                    }
                }
            } else if (ctx.color_attachment && ctx.color_target.write_mask.value != 0) {
                Vector4 frag_color = out_colors[0];
                Vector4 out_color  = frag_color;
                if (ctx.color_target.blend_enable) {
                    Vector4 dst = ctx.color_attachment->read_rgba(x, y);
                    out_color   = apply_blend(frag_color, dst, ctx.color_target.blend);
                }

                if (ctx.color_target.write_mask == GPUColorWrite::ALL) {
                    ctx.color_attachment->write_rgba(x, y, out_color);
                } else {
                    Vector4 cur = ctx.color_attachment->read_rgba(x, y);
                    if (ctx.color_target.write_mask.contains(GPUColorWrite::RED)) cur.r = out_color.r;
                    if (ctx.color_target.write_mask.contains(GPUColorWrite::GREEN)) cur.g = out_color.g;
                    if (ctx.color_target.write_mask.contains(GPUColorWrite::BLUE)) cur.b = out_color.b;
                    if (ctx.color_target.write_mask.contains(GPUColorWrite::ALPHA)) cur.a = out_color.a;
                    ctx.color_attachment->write_rgba(x, y, cur);
                }
            }
        }
    };

    for (int32_t y = min_y; y <= max_y; ++y) {
        float py     = y + 0.5f;
        float w0_row = B0 * py + C0;
        float w1_row = B1 * py + C1;
        float w2_row = B2 * py + C2;

        int32_t x = min_x;
#if LYRA_USE_AVX2
        // AVX2 8-wide SIMD rasterization
        for (; x + 7 <= max_x; x += 8) {
            float  px0     = x + 0.5f;
            __m256 v_steps = _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
            __m256 v_px    = _mm256_add_ps(_mm256_set1_ps(px0), v_steps);

            __m256 v_w0 = _mm256_add_ps(_mm256_set1_ps(w0_row), _mm256_mul_ps(_mm256_set1_ps(A0), v_px));
            __m256 v_w1 = _mm256_add_ps(_mm256_set1_ps(w1_row), _mm256_mul_ps(_mm256_set1_ps(A1), v_px));
            __m256 v_w2 = _mm256_add_ps(_mm256_set1_ps(w2_row), _mm256_mul_ps(_mm256_set1_ps(A2), v_px));

            __m256 v_bias0 = _mm256_set1_ps(bias0);
            __m256 v_bias1 = _mm256_set1_ps(bias1);
            __m256 v_bias2 = _mm256_set1_ps(bias2);
            __m256 m0      = _mm256_cmp_ps(v_w0, v_bias0, _CMP_GE_OQ);
            __m256 m1      = _mm256_cmp_ps(v_w1, v_bias1, _CMP_GE_OQ);
            __m256 m2      = _mm256_cmp_ps(v_w2, v_bias2, _CMP_GE_OQ);
            __m256 in_mask = _mm256_and_ps(_mm256_and_ps(m0, m1), m2);

            int mask = _mm256_movemask_ps(in_mask);
            if (mask == 0) continue; // all 8 pixels outside triangle

            __m256 v_inv_w = _mm256_add_ps(_mm256_add_ps(
                                               _mm256_mul_ps(v_w0, _mm256_set1_ps(v0.inv_w)),
                                               _mm256_mul_ps(v_w1, _mm256_set1_ps(v1.inv_w))),
                _mm256_mul_ps(v_w2, _mm256_set1_ps(v2.inv_w)));

            __m256 v_w = _mm256_div_ps(_mm256_set1_ps(1.0f), v_inv_w);

            __m256 v_z_raw = _mm256_add_ps(_mm256_add_ps(
                                               _mm256_mul_ps(v_w0, _mm256_set1_ps(v0.screen.z * v0.inv_w)),
                                               _mm256_mul_ps(v_w1, _mm256_set1_ps(v1.screen.z * v1.inv_w))),
                _mm256_mul_ps(v_w2, _mm256_set1_ps(v2.screen.z * v2.inv_w)));

            __m256 v_z = _mm256_mul_ps(v_z_raw, v_w);

            alignas(32) float ar_w0[8];
            alignas(32) float ar_w1[8];
            alignas(32) float ar_w2[8];
            alignas(32) float ar_z[8];
            alignas(32) float ar_w[8];

            _mm256_storeu_ps(ar_w0, v_w0);
            _mm256_storeu_ps(ar_w1, v_w1);
            _mm256_storeu_ps(ar_w2, v_w2);
            _mm256_storeu_ps(ar_z, v_z);
            _mm256_storeu_ps(ar_w, v_w);

            for (int k = 0; k < 8; ++k) {
                if (mask & (1 << k)) {
                    process_pixel(x + k, y, px0 + k, py, ar_w0[k], ar_w1[k], ar_w2[k], ar_z[k], ar_w[k]);
                }
            }
        }
#endif

        // scalar fallback for remaining pixels
        for (; x <= max_x; ++x) {
            float px = x + 0.5f;
            float w0 = A0 * px + w0_row;
            float w1 = A1 * px + w1_row;
            float w2 = A2 * px + w2_row;

            if (w0 >= bias0 && w1 >= bias1 && w2 >= bias2) {
                float z_inv = w0 * v0.inv_w + w1 * v1.inv_w + w2 * v2.inv_w;
                float w     = (std::abs(z_inv) > 1e-8f) ? (1.0f / z_inv) : 1.0f;
                float z     = (w0 * v0.screen.z * v0.inv_w + w1 * v1.screen.z * v1.inv_w + w2 * v2.screen.z * v2.inv_w) * w;
                process_pixel(x, y, px, py, w0, w1, w2, z, w);
            }
        }
    }
}

void SoftRasterizer::rasterize_triangles(std::span<const SoftRasterTriangle> triangles, const SoftRasterizerContext& ctx)
{
    for (const auto& tri : triangles) {
        rasterize_triangle(tri, ctx);
    }
}
