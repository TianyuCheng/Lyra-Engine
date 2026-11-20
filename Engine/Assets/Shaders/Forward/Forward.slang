module Forward;

import lyra;
import Common.ShadingModel;
import Common.BSDF;
import Common.Tonemapping;
import Shading.BuiltinShading;

public struct Camera
{
    public float4x4 proj;
    public float4x4 view;
    public float4x4 inv_proj;
    public float4x4 inv_view;
    public float3   pos;
    public float    sun_intensity;
    public float3   sun_dir;
    public float    padding;
    public float3   sun_color;
    public float    padding2;
    public float4x4 light_view_proj;
};

[[lyra::binding(0, 0)]]
public ConstantBuffer<Camera> camera;

[[lyra::binding(1, 0)]]
public Texture2D<float> shadow_map;

[[lyra::binding(2, 0)]]
public SamplerState shadow_sampler;

[[lyra::binding(0, 1)]]
public ByteAddressBuffer vertex_buffer;

public struct InstanceData
{
    public float4x4 model;
    public float4   base_color;
    public float    roughness;
    public float    metallic;
    public uint     pos_offset;
    public uint     norm_offset;
    public uint     uv_offset;
    public uint     material_id;
    public uint     padding0;
    public uint     padding1;
};

[[lyra::immediates]]
public ConstantBuffer<InstanceData> instance_data;

public struct VertexOutput
{
    public float4 position  : SV_Position;
    public float3 normal    : NORMAL;
    public float2 uv        : TEXCOORD0;
    public float3 world_pos : TEXCOORD1;
};

public VertexOutput forward_vsmain(uint vertex_id : SV_VertexID)
{
    VertexOutput output;

    // pull vertex attributes directly from byte-address storage buffer
    float3 position = vertex_buffer.Load<float3>(instance_data.pos_offset + vertex_id * 12);
    float3 normal   = vertex_buffer.Load<float3>(instance_data.norm_offset + vertex_id * 12);
    float2 uv       = vertex_buffer.Load<float2>(instance_data.uv_offset + vertex_id * 8);

    float4 world_pos = mul(float4(position, 1.0), instance_data.model);
    output.world_pos = world_pos.xyz;
    output.position  = mul(mul(world_pos, camera.view), camera.proj);
    output.normal    = mul(normal, (float3x3)instance_data.model);
    output.uv        = uv;
    return output;
}

public SurfaceData extract_surface_data(VertexOutput input)
{
    SurfaceData surf;
    surf.world_pos  = input.world_pos;
    surf.normal     = normalize(input.normal);
    surf.uv         = input.uv;
    surf.base_color = instance_data.base_color;
    surf.roughness  = instance_data.roughness;
    surf.metallic   = instance_data.metallic;
    surf.clear_coat = 0.0;
    surf.view_dir   = normalize(camera.pos - input.world_pos);
    return surf;
}

public LightingContext extract_lighting_context()
{
    LightingContext light;
    light.sun_dir       = normalize(camera.sun_dir);
    light.sun_color     = camera.sun_color;
    light.sun_intensity = camera.sun_intensity;
    return light;
}

public float evaluate_shadow(float3 world_pos)
{
    float4 light_clip = mul(float4(world_pos, 1.0), camera.light_view_proj);
    if (light_clip.w <= 0.0) return 1.0;
    float3 shadow_coords = light_clip.xyz / light_clip.w;

    #if defined(__spirv__) || defined(LYRA_VULKAN)
    float2 shadow_uv = float2(shadow_coords.x * 0.5 + 0.5, shadow_coords.y * 0.5 + 0.5);
    #else
    float2 shadow_uv = float2(shadow_coords.x * 0.5 + 0.5, 0.5 - shadow_coords.y * 0.5);
    #endif

    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0) {
        return 1.0;
    }

    float current_depth = shadow_coords.z;
    if (current_depth > 1.0) return 1.0;

    float shadow = 0.0;
    float bias = 0.002;
    float2 texel_size = float2(1.0 / 2048.0, 1.0 / 2048.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float depth_sample = shadow_map.SampleLevel(shadow_sampler, shadow_uv + float2(x, y) * texel_size, 0).r;
            shadow += (current_depth - bias <= depth_sample) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

public float4 evaluate_forward_shading<TModel : IShadingModel>(VertexOutput input)
{
    SurfaceData     surf  = extract_surface_data(input);
    LightingContext light = extract_lighting_context();
    float shadow = evaluate_shadow(surf.world_pos);

    PBRMaterial mat;
    mat.base_color           = surf.base_color.rgb;
    mat.metallic             = surf.metallic;
    mat.roughness            = max(surf.roughness, 0.04);
    mat.reflectance          = 0.5;
    mat.clear_coat.factor    = 0.0;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint      = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    float3 light_radiance = light.sun_color * light.sun_intensity;
    float3 direct = mat.evaluate(surf.view_dir, light.sun_dir, surf.normal) * light_radiance * shadow;
    float3 ambient = surf.base_color.rgb * max(light.sun_intensity * 0.15, 0.04);
    float3 final_color = tonemap_aces_film(direct + ambient);

    return float4(final_color, surf.base_color.a);
}

// ---------------------------------------------------------------------------
// Specialized entry points
// ---------------------------------------------------------------------------

[shader("vertex")]
public VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    return forward_vsmain(vertex_id);
}

[shader("fragment")]
public float4 fs_pbr(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<PBRShadingModel>(input);
}

[shader("fragment")]
public float4 fs_npr(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<NPRShadingModel>(input);
}

[shader("fragment")]
public float4 fs_unlit(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<UnlitShadingModel>(input);
}

// Default forward fragment entry point (evaluates PBR)
[shader("fragment")]
public float4 fsmain(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<PBRShadingModel>(input);
}
