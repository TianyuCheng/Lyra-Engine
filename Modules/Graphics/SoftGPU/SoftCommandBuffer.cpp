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
        ctx.color_attachments.clear();
        ctx.color_attachment         = nullptr;
        ctx.depth_stencil_attachment = nullptr;
        ctx.vertex_buffers.clear();
        ctx.index_buffer             = {};

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
        ctx.color_attachments.clear();
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

struct SoftBoundResources
{
    const SoftBuffer*      buffers[4][8]{};
    size_t                 buffer_offsets[4][8]{};
    size_t                 buffer_sizes[4][8]{};
    const SoftTexture*     textures[4][8]{};
    const SoftTextureView* views[4][8]{};
    const SoftSampler*     samplers[4][8]{};
};

struct SoftCameraUniforms
{
    Matrix4x4 proj{1.0f};
    Matrix4x4 view{1.0f};
    Matrix4x4 inv_proj{1.0f};
    Matrix4x4 inv_view{1.0f};
    Vector3   pos{0.0f};
    float     sun_intensity = 1.0f;
    Vector3   sun_dir = Vector3(0.5f, 1.0f, 0.3f);
    float     padding = 0.0f;
    Vector3   sun_color = Vector3(1.0f);
    float     padding2 = 0.0f;
    Matrix4x4 light_view_proj{1.0f};
};

struct SoftSkyboxUniform
{
    Matrix4x4 proj{1.0f};
    Matrix4x4 view{1.0f};
    Matrix4x4 inv_proj{1.0f};
    Matrix4x4 inv_view{1.0f};
    Vector3   camera_pos{0.0f};
    float     sun_intensity      = 110000.0f;
    Vector3   sun_dir            = Vector3(0.3f, 0.6f, 0.4f);
    float     sun_angular_radius = 0.004675f;
    Vector3   sun_color          = Vector3(1.0f);
    float     exposure_scale     = 1.0f / (1.2f * 8192.0f);
    float     halo_size          = 10.0f;
    float     halo_falloff       = 80.0f;
    float     limb_darkening     = 0.6f;
    float     ground_roughness   = 0.8f;
    Vector4   ground_albedo      = Vector4(0.12f, 0.12f, 0.14f, 1.0f);
};

struct SoftInstanceData
{
    Matrix4x4 model{1.0f};
    Vector4   base_color{1.0f};
    float     roughness = 0.5f;
    float     metallic  = 0.0f;
    uint32_t  pos_offset = 0;
    uint32_t  norm_offset = 0;
    uint32_t  uv_offset = 0;
    uint32_t  material_id = 0;
    uint32_t  shading_model = 0;
};

struct AtmosphereParams
{
    float   planet_radius         = 6360.0f;
    float   atmosphere_radius     = 6420.0f;
    float   rayleigh_scale_height = 8.0f;
    float   mie_scale_height      = 1.2f;
    Vector3 rayleigh_scattering   = Vector3(5.802f, 13.558f, 33.100f) * 1e-3f;
    float   mie_scattering        = 3.996e-3f;
    float   mie_extinction        = 4.440e-3f;
    float   mie_asymmetry         = 0.8f;
    float   sun_intensity         = 20.0f;
    float   sun_angular_radius    = 0.004675f;
};

static inline Vector3 tonemap_aces_film(Vector3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    Vector3 num = x * (x * a + b);
    Vector3 den = x * (x * c + d) + e;
    return glm::clamp(num / den, 0.0f, 1.0f);
}

static inline float distribution_ggx(float NoH, float roughness)
{
    float alpha = std::max(roughness * roughness, 0.001f);
    float a = NoH * alpha;
    float k = alpha / (1.0f - NoH * NoH + a * a);
    return k * k * (1.0f / 3.14159265358979323846f);
}

static inline float visibility_smith_ggx_correlated(float NoV, float NoL, float roughness)
{
    float a2 = std::max(std::pow(roughness * roughness, 2.0f), 1e-5f);
    float GGXV = NoL * std::sqrt(NoV * NoV * (1.0f - a2) + a2);
    float GGXL = NoV * std::sqrt(NoL * NoL * (1.0f - a2) + a2);
    return 0.5f / std::max(GGXV + GGXL, 1e-7f);
}

static inline Vector3 fresnel_schlick(float cos_theta, Vector3 F0)
{
    float f = std::pow(std::clamp(1.0f - cos_theta, 0.0f, 1.0f), 5.0f);
    return F0 + (Vector3(1.0f) - F0) * f;
}

static inline Vector3 diffuse_burley(Vector3 diffuse_color, float NoV, float NoL, float LoH, float roughness)
{
    float f90 = 0.5f + 2.0f * roughness * LoH * LoH;
    float light_scatter = 1.0f + (f90 - 1.0f) * std::pow(std::clamp(1.0f - NoL, 0.0f, 1.0f), 5.0f);
    float view_scatter  = 1.0f + (f90 - 1.0f) * std::pow(std::clamp(1.0f - NoV, 0.0f, 1.0f), 5.0f);
    return diffuse_color * ((1.0f / 3.14159265358979323846f) * light_scatter * view_scatter);
}

static inline Vector3 evaluate_pbr(
    Vector3 V, Vector3 L, Vector3 N,
    Vector3 base_color, float roughness, float metallic, float reflectance = 0.5f)
{
    float NoV = std::max(glm::dot(N, V), 0.0f);
    float NoL = std::max(glm::dot(N, L), 0.0f);
    if (NoV <= 0.0f || NoL <= 0.0f) {
        return Vector3(0.0f);
    }

    Vector3 H = glm::normalize(V + L);
    float NoH = std::max(glm::dot(N, H), 0.0f);
    float VoH = std::max(glm::dot(V, H), 0.0f);
    float LoH = std::max(glm::dot(L, H), 0.0f);

    Vector3 diffuse_color = base_color * (1.0f - metallic);
    Vector3 dielectric_f0 = Vector3(0.16f * reflectance * reflectance);
    Vector3 F0 = glm::mix(dielectric_f0, base_color, metallic);

    float D = distribution_ggx(NoH, roughness);
    float V_term = visibility_smith_ggx_correlated(NoV, NoL, roughness);
    Vector3 F = fresnel_schlick(VoH, F0);
    Vector3 base_specular = D * V_term * F;

    Vector3 base_diffuse = diffuse_burley(diffuse_color, NoV, NoL, LoH, roughness);
    return (base_diffuse + base_specular) * NoL;
}

static inline float evaluate_shadow_map(
    const SoftTexture* shadow_map,
    const SoftSampler* shadow_sampler,
    const SoftTextureView* shadow_view,
    Vector3 world_pos,
    const Matrix4x4& light_view_proj)
{
    if (!shadow_map || !shadow_view || !shadow_sampler) return 1.0f;

    Vector4 light_clip = Vector4(world_pos, 1.0f) * light_view_proj;
    if (light_clip.w <= 0.0f) return 1.0f;
    Vector3 shadow_coords = Vector3(light_clip) / light_clip.w;

    Vector2 shadow_uv = Vector2(shadow_coords.x * 0.5f + 0.5f, 0.5f - shadow_coords.y * 0.5f);

    if (shadow_uv.x < 0.0f || shadow_uv.x > 1.0f || shadow_uv.y < 0.0f || shadow_uv.y > 1.0f) {
        return 1.0f;
    }

    float current_depth = shadow_coords.z;
    if (current_depth > 1.0f) return 1.0f;

    float shadow = 0.0f;
    float bias = 0.002f;
    float texel_size = 1.0f / 2048.0f;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            Vector2 uv = shadow_uv + Vector2(x, y) * texel_size;
            Vector4 sample = shadow_sampler->sample_2d(*shadow_map, *shadow_view, uv.x, uv.y);
            float depth_sample = sample.r;
            shadow += (current_depth - bias <= depth_sample) ? 1.0f : 0.0f;
        }
    }
    return shadow / 9.0f;
}

static inline float rayleigh_phase(float cos_theta)
{
    return (3.0f / (16.0f * 3.14159265358979323846f)) * (1.0f + cos_theta * cos_theta);
}

static inline float mie_phase(float cos_theta, float g)
{
    float g2 = g * g;
    float num = 3.0f * (1.0f - g2) * (1.0f + cos_theta * cos_theta);
    float denom = (8.0f * 3.14159265358979323846f) * (2.0f + g2) * std::pow(std::max(1.0f + g2 - 2.0f * g * cos_theta, 1e-4f), 1.5f);
    return num / denom;
}

static inline bool intersect_atmosphere(Vector3 ro, Vector3 rd, float planet_radius, float atmosphere_radius, float& out_t_min, float& out_t_max)
{
    out_t_min = 0.0f;
    out_t_max = 0.0f;
    float b = glm::dot(ro, rd);
    float c = glm::dot(ro, ro) - atmosphere_radius * atmosphere_radius;
    float d = b * b - c;
    if (d < 0.0f) return false;
    float sqrt_d = std::sqrt(d);
    float t0 = -b - sqrt_d;
    float t1 = -b + sqrt_d;
    if (t1 < 0.0f) return false;
    out_t_min = std::max(t0, 0.0f);
    out_t_max = t1;

    float c_ground = glm::dot(ro, ro) - planet_radius * planet_radius;
    float d_ground = b * b - c_ground;
    if (d_ground >= 0.0f) {
        float t_ground = -b - std::sqrt(d_ground);
        if (t_ground > 0.0f) {
            out_t_max = std::min(out_t_max, t_ground);
        }
    }
    return out_t_max > out_t_min;
}

static inline Vector2 integrate_optical_depth(const AtmosphereParams& params, Vector3 p, Vector3 sun_dir, int step_count = 8)
{
    float b_ground = glm::dot(p, sun_dir);
    float c_ground = glm::dot(p, p) - params.planet_radius * params.planet_radius;
    float d_ground = b_ground * b_ground - c_ground;
    if (d_ground >= 0.0f) {
        float t_ground = -b_ground - std::sqrt(d_ground);
        if (t_ground > 0.0f) {
            return Vector2(1e9f, 1e9f);
        }
    }

    float t_min, t_max;
    if (!intersect_atmosphere(p, sun_dir, params.planet_radius, params.atmosphere_radius, t_min, t_max)) {
        return Vector2(0.0f);
    }

    float step_size = t_max / static_cast<float>(step_count);
    Vector2 optical_depth(0.0f);
    for (int i = 0; i < step_count; ++i) {
        Vector3 sample_p = p + sun_dir * ((static_cast<float>(i) + 0.5f) * step_size);
        float h = std::max(glm::length(sample_p) - params.planet_radius, 0.0f);
        optical_depth.x += std::exp(-h / params.rayleigh_scale_height) * step_size;
        optical_depth.y += std::exp(-h / params.mie_scale_height) * step_size;
    }
    return optical_depth;
}

static inline Vector3 evaluate_sky_radiance(const AtmosphereParams& params, Vector3 ro, Vector3 rd, Vector3 sun_dir, int num_steps = 16)
{
    float t_min, t_max;
    if (!intersect_atmosphere(ro, rd, params.planet_radius, params.atmosphere_radius, t_min, t_max)) {
        return Vector3(0.0f);
    }

    float step_size = (t_max - t_min) / static_cast<float>(num_steps);
    float cos_theta = glm::dot(rd, sun_dir);
    float p_rayleigh = rayleigh_phase(cos_theta);
    float p_mie = mie_phase(cos_theta, params.mie_asymmetry);
    float p_multi = 2.5f / (4.0f * 3.14159265358979323846f);

    Vector3 acc_rayleigh(0.0f);
    Vector3 acc_mie(0.0f);
    Vector2 optical_depth_view(0.0f);

    for (int i = 0; i < num_steps; ++i) {
        Vector3 sample_p = ro + rd * (t_min + (static_cast<float>(i) + 0.5f) * step_size);
        float h = std::max(glm::length(sample_p) - params.planet_radius, 0.0f);

        float d_r = std::exp(-h / params.rayleigh_scale_height) * step_size;
        float d_m = std::exp(-h / params.mie_scale_height) * step_size;
        optical_depth_view += Vector2(d_r, d_m);

        Vector2 optical_depth_sun = integrate_optical_depth(params, sample_p, sun_dir, 6);
        Vector3 tau = params.rayleigh_scattering * (optical_depth_view.x + optical_depth_sun.x) +
                      params.mie_extinction * (optical_depth_view.y + optical_depth_sun.y);
        Vector3 transmittance = glm::exp(-tau);

        acc_rayleigh += d_r * transmittance;
        acc_mie += d_m * transmittance;
    }

    return (acc_rayleigh * params.rayleigh_scattering * (p_rayleigh + p_multi) +
            acc_mie * params.mie_scattering * p_mie) * params.sun_intensity;
}

static inline Vector3 render_physical_sun(
    const AtmosphereParams& params,
    Vector3 view_dir,
    Vector3 sun_dir,
    Vector3 sun_transmittance,
    float halo_size = 10.0f,
    float halo_falloff = 80.0f,
    float limb_darkening = 0.6f)
{
    float cos_theta = glm::dot(view_dir, sun_dir);
    float theta     = std::acos(std::clamp(cos_theta, -1.0f, 1.0f));
    float r_sun     = std::max(params.sun_angular_radius, 1e-4f);

    constexpr float PI = 3.14159265358979323846f;
    float solid_angle = std::max(2.0f * PI * (1.0f - std::cos(r_sun)), 1e-8f);
    float mean_luminance = params.sun_intensity / solid_angle;
    float I0 = mean_luminance / std::max(1.0f - limb_darkening / 3.0f, 0.01f);

    Vector3 result(0.0f);
    if (theta <= r_sun) {
        float r = theta / r_sun;
        float mu = std::sqrt(std::max(0.0f, 1.0f - r * r));
        float limb = 1.0f - limb_darkening * (1.0f - mu);
        float edge_aa = std::clamp((r_sun - theta) / (r_sun * 0.04f), 0.0f, 1.0f);
        result += Vector3(I0 * limb * edge_aa);
    }

    float r_halo = r_sun * std::max(halo_size, 1.0f);
    if (theta < r_halo) {
        float halo_norm = (theta - r_sun * 0.5f) / (r_halo - r_sun * 0.5f);
        halo_norm = std::clamp(halo_norm, 0.0f, 1.0f);
        float halo_weight = std::pow(1.0f - halo_norm, std::max(halo_falloff * 0.1f, 1.0f));
        float halo_radiance = (mean_luminance * 0.002f) * halo_weight;
        result += Vector3(halo_radiance);
    }

    return result * sun_transmittance;
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

    bool                  is_skybox            = false;
    bool                  is_deferred_lighting = false;
    bool                  is_forward           = false;
    bool                  is_gbuffer           = false;
    bool                  is_shadow_map        = false;

    SoftSkyboxUniform     skybox_uniform{};
    SoftCameraUniforms    camera_uniforms{};
    SoftInstanceData      instance_data{};
    Matrix4x4             light_view_proj{1.0f};
    const SoftBuffer*     storage_vertex_buffer = nullptr;
    size_t                storage_vertex_offset = 0;
};

static SoftDrawParams setup_draw_params(SoftDevice& device, const ExecutionContext& ctx)
{
    SoftDrawParams p{};
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

    // 1. matrix and attributes from immediates
    if (!ctx.immediates_data.empty() && ctx.immediates_data.size() >= sizeof(Matrix4x4)) {
        p.instance_data.model = *reinterpret_cast<const Matrix4x4*>(ctx.immediates_data.data());
        p.vp_matrix           = p.instance_data.model;
        p.has_vp              = true;

        if (ctx.immediates_data.size() >= 80) {
            p.instance_data.base_color = *reinterpret_cast<const Vector4*>(ctx.immediates_data.data() + 64);
        }
        if (ctx.immediates_data.size() >= 88) {
            p.instance_data.roughness = *reinterpret_cast<const float*>(ctx.immediates_data.data() + 80);
            p.instance_data.metallic  = *reinterpret_cast<const float*>(ctx.immediates_data.data() + 84);
        }
        if (ctx.immediates_data.size() > 80 && ctx.immediates_data.size() <= 88) {
            p.instance_data.pos_offset = *reinterpret_cast<const uint32_t*>(ctx.immediates_data.data() + 64);
        } else if (ctx.immediates_data.size() >= 100) {
            p.instance_data.pos_offset  = *reinterpret_cast<const uint32_t*>(ctx.immediates_data.data() + 88);
            p.instance_data.norm_offset = *reinterpret_cast<const uint32_t*>(ctx.immediates_data.data() + 92);
            p.instance_data.uv_offset   = *reinterpret_cast<const uint32_t*>(ctx.immediates_data.data() + 96);
            if (ctx.immediates_data.size() >= 104) {
                p.instance_data.material_id = *reinterpret_cast<const uint32_t*>(ctx.immediates_data.data() + 100);
            }
        }
    }

    // 2. gather bound resources across all bind groups
    SoftBoundResources res{};
    for (const auto& [slot, bound_bg] : ctx.bind_groups) {
        if (slot >= 4 || !bound_bg.handle.valid()) continue;
        auto bg = device.bind_groups.find(bound_bg.handle.to_slotmap_handle<SoftBindGroup>());
        if (!bg) continue;

        for (const auto& entry : bg->entries) {
            if (entry.binding >= 8) continue;
            if (entry.type == GPUResourceType::BUFFER && entry.buffer.buffer.valid()) {
                auto buf = device.buffers.find(entry.buffer.buffer.to_slotmap_handle<SoftBuffer>());
                if (buf) {
                    size_t dyn_off = bound_bg.dynamic_offsets.empty() ? 0 : bound_bg.dynamic_offsets[0];
                    res.buffers[slot][entry.binding]        = buf;
                    res.buffer_offsets[slot][entry.binding] = entry.buffer.offset + dyn_off;
                    res.buffer_sizes[slot][entry.binding]   = entry.buffer.size;
                }
            } else if (entry.type == GPUResourceType::TEXTURE && entry.texture.valid()) {
                auto v = device.texture_views.find(entry.texture.to_slotmap_handle<SoftTextureView>());
                if (v) {
                    res.views[slot][entry.binding]    = v;
                    res.textures[slot][entry.binding] = device.textures.find(v->texture.to_slotmap_handle<SoftTexture>());
                }
            } else if (entry.type == GPUResourceType::SAMPLER && entry.sampler.valid()) {
                res.samplers[slot][entry.binding] = device.samplers.find(entry.sampler.to_slotmap_handle<SoftSampler>());
            }
        }
    }

    // 3. Detect active pipeline pass

    // A. GBufferPass: 3 color targets (MRT)
    if (ctx.render_pipeline && ctx.render_pipeline->color_targets.size() == 3) {
        p.is_gbuffer = true;
        if (res.buffers[0][0]) {
            const uint8_t* bptr        = res.buffers[0][0]->data.data() + res.buffer_offsets[0][0];
            p.camera_uniforms.proj     = *reinterpret_cast<const Matrix4x4*>(bptr + 0);
            p.camera_uniforms.view     = *reinterpret_cast<const Matrix4x4*>(bptr + 64);
            p.camera_uniforms.inv_proj = *reinterpret_cast<const Matrix4x4*>(bptr + 128);
            p.camera_uniforms.inv_view = *reinterpret_cast<const Matrix4x4*>(bptr + 192);
            p.camera_uniforms.pos      = *reinterpret_cast<const Vector3*>(bptr + 256);
        }

        p.r_ctx.fragment_shader_mrt = [inst = p.instance_data](const SoftShadedVertex& vert, Vector4* out_colors, uint32_t max_targets) {
            if (max_targets > 0) {
                out_colors[0] = Vector4(inst.base_color.r, inst.base_color.g, inst.base_color.b, inst.metallic);
            }
            if (max_targets > 1) {
                Vector3 n = glm::normalize(Vector3(vert.normal));
                out_colors[1] = Vector4(n * 0.5f + 0.5f, inst.roughness);
            }
            if (max_targets > 2) {
                out_colors[2] = Vector4(static_cast<float>(inst.pos_offset), static_cast<float>(inst.material_id), 0.0f, 0.0f);
            }
        };
    }
    // B. DeferredLightingPass: textures 0..3 bound at Set 0, Camera at binding 4
    else if (res.textures[0][0] && res.textures[0][1] && res.textures[0][2] && res.textures[0][3] && res.buffers[0][4]) {
        p.is_deferred_lighting = true;
        const uint8_t* bptr    = res.buffers[0][4]->data.data() + res.buffer_offsets[0][4];
        p.camera_uniforms      = *reinterpret_cast<const SoftCameraUniforms*>(bptr);

        auto albedo_tex  = res.textures[0][0];
        auto normal_tex  = res.textures[0][1];
        auto mat_tex     = res.textures[0][2];
        auto depth_tex   = res.textures[0][3];
        auto shadow_tex  = res.textures[0][5];
        auto shadow_view = res.views[0][5];
        auto shadow_samp = res.samplers[0][6];
        auto cam         = p.camera_uniforms;

        p.r_ctx.fragment_shader = [albedo_tex, normal_tex, mat_tex, depth_tex, shadow_tex, shadow_view, shadow_samp, cam, vp = p.r_ctx.viewport](const SoftShadedVertex& vert) -> Vector4 {
            int32_t px = static_cast<int32_t>(vert.screen.x);
            int32_t py = static_cast<int32_t>(vert.screen.y);
            float   depth = depth_tex->read_depth(px, py);
            if (depth >= 1.0f) {
                return Vector4(0.0f);
            }

            Vector4 albedo_metallic = albedo_tex->read_rgba(px, py);
            Vector4 normal_rough    = normal_tex->read_rgba(px, py);
            Vector4 mat_info        = mat_tex->read_rgba(px, py);

            if (static_cast<uint32_t>(mat_info.r) == 1) {
                return Vector4(Vector3(albedo_metallic), 1.0f);
            }

            float ndc_x = (static_cast<float>(px) + 0.5f) / vp.width * 2.0f - 1.0f;
            float ndc_y = 1.0f - (static_cast<float>(py) + 0.5f) / vp.height * 2.0f;
            Vector4 clip_pos(ndc_x, ndc_y, depth, 1.0f);

            Vector4 view_pos = clip_pos * cam.inv_proj;
            view_pos /= std::max(view_pos.w, 1e-6f);
            Vector4 world_pos = Vector4(Vector3(view_pos), 1.0f) * cam.inv_view;

            Vector3 N = glm::normalize(Vector3(normal_rough) * 2.0f - 1.0f);
            Vector3 V = glm::normalize(cam.pos - Vector3(world_pos));
            Vector3 L = glm::normalize(cam.sun_dir);

            float   roughness  = std::max(normal_rough.a, 0.04f);
            float   metallic   = albedo_metallic.a;
            Vector3 base_color = Vector3(albedo_metallic);

            float   shadow         = evaluate_shadow_map(shadow_tex, shadow_samp, shadow_view, Vector3(world_pos), cam.light_view_proj);
            Vector3 light_radiance = cam.sun_color * cam.sun_intensity;
            Vector3 direct         = evaluate_pbr(V, L, N, base_color, roughness, metallic) * light_radiance * shadow;
            Vector3 ambient        = base_color * std::max(cam.sun_intensity * 0.15f, 0.04f);
            Vector3 final_color    = tonemap_aces_film(direct + ambient);

            return Vector4(final_color, 1.0f);
        };
    }
    // C. SkyboxPass: SkyboxUniform at Set 0 binding 0, depth compare LESS_EQUAL, depth write false
    else if (res.buffers[0][0] && res.buffer_offsets[0][0] + sizeof(SoftSkyboxUniform) <= res.buffers[0][0]->data.size() &&
             ctx.render_pipeline && ctx.render_pipeline->depth_stencil.depth_compare == GPUCompareFunction::LESS_EQUAL &&
             !ctx.render_pipeline->depth_stencil.depth_write_enabled) {
        p.is_skybox         = true;
        const uint8_t* bptr = res.buffers[0][0]->data.data() + res.buffer_offsets[0][0];
        p.skybox_uniform    = *reinterpret_cast<const SoftSkyboxUniform*>(bptr);

        p.r_ctx.fragment_shader = [sky = p.skybox_uniform](const SoftShadedVertex& vert) -> Vector4 {
            Vector3 dir = glm::normalize(Vector3(vert.texcoord));
            AtmosphereParams atmo{};

            float exposed_sun_intensity = std::max(sky.sun_intensity * sky.exposure_scale, 0.01f);
            atmo.sun_intensity          = std::clamp(exposed_sun_intensity * 1.8f, 5.0f, 40.0f);
            atmo.sun_angular_radius     = sky.sun_angular_radius;
            Vector3 sun_dir             = glm::normalize(sky.sun_dir);

            float   cam_alt_km = std::max(sky.camera_pos.y * 0.001f, 0.0f);
            Vector3 camera_pos(0.0f, atmo.planet_radius + 0.001f + cam_alt_km, 0.0f);

            Vector2 optical_depth_sun = integrate_optical_depth(atmo, camera_pos, sun_dir, 16);
            Vector3 tau_sun           = atmo.rayleigh_scattering * optical_depth_sun.x + atmo.mie_extinction * optical_depth_sun.y;
            Vector3 sun_transmittance = glm::exp(-tau_sun);

            Vector3 horizon_dir = glm::normalize(Vector3(dir.x, 0.002f, dir.z));
            Vector3 horizon_sky = evaluate_sky_radiance(atmo, camera_pos, horizon_dir, sun_dir, 16);

            Vector3 color;
            if (dir.y >= 0.0f) {
                color = evaluate_sky_radiance(atmo, camera_pos, dir, sun_dir, 16);
                color += render_physical_sun(atmo, dir, sun_dir, sun_transmittance, sky.halo_size, sky.halo_falloff, sky.limb_darkening) * sky.sun_color;
            } else {
                float   sun_elev       = std::max(sun_dir.y, 0.0f);
                Vector3 sun_irradiance = sky.sun_color * (sun_elev * exposed_sun_intensity) * sun_transmittance;
                Vector3 sky_ambient    = horizon_sky * 0.25f;
                Vector3 ground_radiance = (Vector3(sky.ground_albedo) / 3.14159265358979323846f) * sun_irradiance + Vector3(sky.ground_albedo) * sky_ambient;
                float   haze           = std::exp(-6.0f * std::max(-dir.y, 0.0f));
                color                  = glm::mix(ground_radiance, horizon_sky, haze);
            }

            color = tonemap_aces_film(color);
            return Vector4(color, 1.0f);
        };
    }
    // D. ForwardPass: Camera at Set 0 binding 0, shadow map at 1, shadow sampler at 2, storage buffer at Set 1 binding 0
    else if (res.buffers[0][0] && res.textures[0][1] && res.samplers[0][2] && res.buffers[1][0]) {
        p.is_forward              = true;
        p.storage_vertex_buffer   = res.buffers[1][0];
        p.storage_vertex_offset   = res.buffer_offsets[1][0];
        const uint8_t* bptr       = res.buffers[0][0]->data.data() + res.buffer_offsets[0][0];
        p.camera_uniforms         = *reinterpret_cast<const SoftCameraUniforms*>(bptr);

        auto shadow_tex  = res.textures[0][1];
        auto shadow_view = res.views[0][1];
        auto shadow_samp = res.samplers[0][2];
        auto cam         = p.camera_uniforms;
        auto inst        = p.instance_data;

        p.r_ctx.fragment_shader = [shadow_tex, shadow_view, shadow_samp, cam, inst](const SoftShadedVertex& vert) -> Vector4 {
            Vector3 world_pos = Vector3(vert.extra);
            Vector3 N         = glm::normalize(Vector3(vert.normal));
            Vector3 V         = glm::normalize(cam.pos - world_pos);
            Vector3 L         = glm::normalize(cam.sun_dir);

            float   roughness  = std::max(inst.roughness, 0.04f);
            float   metallic   = inst.metallic;
            Vector3 base_color = Vector3(inst.base_color);

            float   shadow         = evaluate_shadow_map(shadow_tex, shadow_samp, shadow_view, world_pos, cam.light_view_proj);
            Vector3 light_radiance = cam.sun_color * cam.sun_intensity;
            Vector3 direct         = evaluate_pbr(V, L, N, base_color, roughness, metallic) * light_radiance * shadow;
            Vector3 ambient        = base_color * std::max(cam.sun_intensity * 0.15f, 0.04f);
            Vector3 final_color    = tonemap_aces_film(direct + ambient);

            return Vector4(final_color, inst.base_color.a);
        };
    }
    // E. ShadowMapPass: depth only pass with Set 0 light_view_proj and Set 1 vertex storage buffer
    else if (ctx.render_pipeline && ctx.render_pipeline->color_targets.empty() && ctx.render_pipeline->depth_stencil.depth_write_enabled &&
             res.buffers[0][0] && res.buffers[1][0]) {
        p.is_shadow_map         = true;
        p.storage_vertex_buffer = res.buffers[1][0];
        p.storage_vertex_offset = res.buffer_offsets[1][0];
        const uint8_t* bptr     = res.buffers[0][0]->data.data() + res.buffer_offsets[0][0];
        p.light_view_proj       = *reinterpret_cast<const Matrix4x4*>(bptr);
    }
    // F. TonemappingPass: HDR texture at Set 0 binding 0, sampler at 1, TonemapParams immediates
    else if (res.textures[0][0] && res.samplers[0][1] && ctx.immediates_data.size() >= sizeof(float) * 2) {
        auto  hdr_tex  = res.textures[0][0];
        auto  hdr_view = res.views[0][0];
        auto  lin_samp = res.samplers[0][1];
        float exposure = *reinterpret_cast<const float*>(ctx.immediates_data.data());

        p.r_ctx.fragment_shader = [hdr_tex, hdr_view, lin_samp, exposure](const SoftShadedVertex& vert) -> Vector4 {
            Vector4 hdr_sample = lin_samp->sample_2d(*hdr_tex, *hdr_view, vert.texcoord.x, vert.texcoord.y);
            Vector3 color      = Vector3(hdr_sample) * std::max(exposure, 0.001f);
            return Vector4(tonemap_aces_film(color), 1.0f);
        };
    }
    // G. Fallback for all existing RHI tests
    else {
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
    }

    return p;
}

void SoftCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    commands.push_back([=](SoftDevice& device, ExecutionContext& ctx) {
        if (!ctx.color_attachment && ctx.color_attachments.empty() && !ctx.depth_stencil_attachment) return;

        SoftDrawParams p = setup_draw_params(device, ctx);

        auto it_v   = ctx.vertex_buffers.find(0);
        bool has_vb = (ctx.render_pipeline && !ctx.render_pipeline->vertex_buffers.empty()) &&
                      (it_v != ctx.vertex_buffers.end());
        if (!has_vb) {
            // Procedural quad draw (SkyboxPass, DeferredLightingPass, TonemappingPass, etc.)
            static const Vector2 quad_vertices[6] = {
                Vector2(-1.0f, -1.0f),
                Vector2( 1.0f, -1.0f),
                Vector2( 1.0f,  1.0f),
                Vector2(-1.0f, -1.0f),
                Vector2( 1.0f,  1.0f),
                Vector2(-1.0f,  1.0f)
            };

            uint32_t num_tris = vertex_count / 3;
            for (uint32_t t = 0; t < num_tris; ++t) {
                auto read_quad_vert = [&](uint32_t idx) -> SoftShadedVertex {
                    SoftShadedVertex sv{};
                    uint32_t vert_id = (first_vertex + idx) % 6;
                    Vector2 pt = quad_vertices[vert_id];

                    if (p.is_skybox) {
                        sv.position = Vector4(pt.x, pt.y, 1.0f, 1.0f);
                        Vector4 p_far_view = Vector4(pt.x, pt.y, 1.0f, 1.0f) * p.skybox_uniform.inv_proj;
                        p_far_view /= p_far_view.w;
                        Vector3 p_far_world = Vector3(Vector4(Vector3(p_far_view), 1.0f) * p.skybox_uniform.inv_view);
                        Vector3 ray_dir = glm::normalize(p_far_world - p.skybox_uniform.camera_pos);
                        sv.texcoord = Vector4(ray_dir, 0.0f);
                    } else {
                        sv.position = Vector4(pt.x, pt.y, 0.0f, 1.0f);
                        sv.texcoord = Vector4(pt.x * 0.5f + 0.5f, 0.5f - pt.y * 0.5f, 0.0f, 0.0f);
                    }
                    return sv;
                };

                SoftShadedVertex v0 = read_quad_vert(t * 3 + 0);
                SoftShadedVertex v1 = read_quad_vert(t * 3 + 1);
                SoftShadedVertex v2 = read_quad_vert(t * 3 + 2);

                Vector<SoftRasterTriangle> tris;
                SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
                for (const auto& tri : tris) {
                    SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
                }
            }
            return;
        }

        auto vbuf = device.buffers.find(it_v->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (!vbuf) return;

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
        printf("[DEBUG draw_indexed] idx_count=%u, has_vb0=%d, col_att=%p, col_atts=%zu, ds=%p\n",
            index_count, (int)(ctx.vertex_buffers.find(0) != ctx.vertex_buffers.end()),
            (void*)ctx.color_attachment, ctx.color_attachments.size(), (void*)ctx.depth_stencil_attachment);
        if (!ctx.color_attachment && ctx.color_attachments.empty() && !ctx.depth_stencil_attachment) return;

        auto ibuf = device.buffers.find(ctx.index_buffer.buffer.to_slotmap_handle<SoftBuffer>());
        if (!ibuf) return;

        SoftDrawParams p = setup_draw_params(device, ctx);

        const uint8_t* idata = ibuf->data.data() + ctx.index_buffer.offset;
        auto read_index = [&](uint32_t idx) -> uint32_t {
            if (ctx.index_buffer.format == GPUIndexFormat::UINT16) {
                return reinterpret_cast<const uint16_t*>(idata)[idx];
            } else {
                return reinterpret_cast<const uint32_t*>(idata)[idx];
            }
        };

        // Case A: Programmable Vertex Pulling from Storage Buffer (ForwardPass & ShadowMapPass)
        if (ctx.vertex_buffers.find(0) == ctx.vertex_buffers.end()) {
            if (!p.storage_vertex_buffer) return;
            const uint8_t* sdata = p.storage_vertex_buffer->data.data() + p.storage_vertex_offset;

            auto read_pulled_vert = [&](uint32_t vert_idx) -> SoftShadedVertex {
                SoftShadedVertex sv{};
                Vector3 local_pos = *reinterpret_cast<const Vector3*>(sdata + p.instance_data.pos_offset + vert_idx * 12);
                Vector4 world_pos = Vector4(local_pos, 1.0f) * p.instance_data.model;
                sv.extra = world_pos;

                if (p.is_shadow_map) {
                    sv.position = world_pos * p.light_view_proj;
                } else {
                    Vector3 local_norm = *reinterpret_cast<const Vector3*>(sdata + p.instance_data.norm_offset + vert_idx * 12);
                    Vector2 local_uv = *reinterpret_cast<const Vector2*>(sdata + p.instance_data.uv_offset + vert_idx * 8);

                    sv.position = (world_pos * p.camera_uniforms.view) * p.camera_uniforms.proj;
                    Matrix3x3 normal_matrix(p.instance_data.model);
                    sv.normal = Vector4(local_norm * normal_matrix, 0.0f);
                    sv.texcoord = Vector4(local_uv.x, local_uv.y, 0.0f, 0.0f);
                    sv.color = p.instance_data.base_color;
                }
                return sv;
            };

            uint32_t num_tris = index_count / 3;
            for (uint32_t t = 0; t < num_tris; ++t) {
                uint32_t i0 = read_index(first_index + t * 3 + 0) + base_vertex;
                uint32_t i1 = read_index(first_index + t * 3 + 1) + base_vertex;
                uint32_t i2 = read_index(first_index + t * 3 + 2) + base_vertex;

                SoftShadedVertex v0 = read_pulled_vert(i0);
                SoftShadedVertex v1 = read_pulled_vert(i1);
                SoftShadedVertex v2 = read_pulled_vert(i2);

                Vector<SoftRasterTriangle> tris;
                SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
                for (const auto& tri : tris) {
                    SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
                }
            }
            return;
        }

        // Case B: Multi-slot vertex buffer streams (GBufferPass)
        if (p.is_gbuffer && ctx.vertex_buffers.find(1) != ctx.vertex_buffers.end()) {
            auto it_pos = ctx.vertex_buffers.find(0);
            auto it_norm = ctx.vertex_buffers.find(1);
            auto it_uv = ctx.vertex_buffers.find(2);

            auto b_pos = device.buffers.find(it_pos->second.buffer.to_slotmap_handle<SoftBuffer>());
            auto b_norm = device.buffers.find(it_norm->second.buffer.to_slotmap_handle<SoftBuffer>());
            auto b_uv = (it_uv != ctx.vertex_buffers.end()) ? device.buffers.find(it_uv->second.buffer.to_slotmap_handle<SoftBuffer>()) : nullptr;

            if (!b_pos || !b_norm) return;

            const uint8_t* pdata = b_pos->data.data() + it_pos->second.offset;
            const uint8_t* ndata = b_norm->data.data() + it_norm->second.offset;
            const uint8_t* udata = b_uv ? (b_uv->data.data() + it_uv->second.offset) : nullptr;

            auto read_gbuffer_vert = [&](uint32_t vert_idx) -> SoftShadedVertex {
                SoftShadedVertex sv{};
                Vector3 local_pos = *reinterpret_cast<const Vector3*>(pdata + vert_idx * sizeof(Vector3));
                Vector3 local_norm = *reinterpret_cast<const Vector3*>(ndata + vert_idx * sizeof(Vector3));
                Vector2 local_uv = udata ? *reinterpret_cast<const Vector2*>(udata + vert_idx * sizeof(Vector2)) : Vector2(0.0f);

                Vector4 world_pos = Vector4(local_pos, 1.0f) * p.instance_data.model;
                sv.position = (world_pos * p.camera_uniforms.view) * p.camera_uniforms.proj;
                Matrix3x3 normal_matrix(p.instance_data.model);
                sv.normal = Vector4(local_norm * normal_matrix, 0.0f);
                sv.texcoord = Vector4(local_uv.x, local_uv.y, 0.0f, 0.0f);
                sv.extra = world_pos;
                sv.color = p.instance_data.base_color;
                return sv;
            };

            uint32_t num_tris = index_count / 3;
            for (uint32_t t = 0; t < num_tris; ++t) {
                uint32_t i0 = read_index(first_index + t * 3 + 0) + base_vertex;
                uint32_t i1 = read_index(first_index + t * 3 + 1) + base_vertex;
                uint32_t i2 = read_index(first_index + t * 3 + 2) + base_vertex;

                SoftShadedVertex v0 = read_gbuffer_vert(i0);
                SoftShadedVertex v1 = read_gbuffer_vert(i1);
                SoftShadedVertex v2 = read_gbuffer_vert(i2);

                Vector<SoftRasterTriangle> tris;
                SoftRasterizer::clip_and_project_triangle(v0, v1, v2, ctx.viewport, tris);
                for (const auto& tri : tris) {
                    SoftRasterizer::rasterize_triangle(tri, p.r_ctx);
                }
            }
            return;
        }

        // Case C: Standard single vertex buffer (Existing RHI tests)
        auto it_v = ctx.vertex_buffers.find(0);
        if (it_v == ctx.vertex_buffers.end()) return;
        auto vbuf = device.buffers.find(it_v->second.buffer.to_slotmap_handle<SoftBuffer>());
        if (!vbuf) return;

        const uint8_t* vdata = vbuf->data.data() + it_v->second.offset;

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
