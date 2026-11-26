import lyra;
import Water.Ocean;
import Common.Tonemapping;

public struct OceanPassUniform
{
    public float4x4 proj;
    public float4x4 view;
    public float4x4 inv_proj;
    public float4x4 inv_view;
    public float4   camera_pos_and_time;      // xyz = camera position, w = elapsed time
    public float4   sun_dir_and_intensity;    // xyz = sun direction vector, w = sun intensity
    public float4   sun_color_and_foam_scale; // xyz = sun color, w = foam factor scale
    public float4   water_params;             // x = water depth, y = water clarity, z = wave speed, w = roughness
    public float4   deep_color;               // deep ocean color
    public float4   shallow_color;            // wave crest shallow color
    public GerstnerWave waves[4];             // 4 Gerstner wave descriptors
};

[[lyra::binding(0, 0)]]
public ConstantBuffer<OceanPassUniform> ocean_cb;

public struct VertexInput
{
    public float3 position : POSITION;
    public float2 uv       : TEXCOORD0;
};

public struct VertexOutput
{
    public float4 position    : SV_Position;
    public float3 world_pos   : TEXCOORD0;
    public float3 normal      : NORMAL;
    public float3 tangent     : TANGENT;
    public float3 bitangent   : BITANGENT;
    public float2 uv          : TEXCOORD1;
    public float  foam_factor : FOAM;
};

[shader("vertex")]
public VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;

    float time = ocean_cb.camera_pos_and_time.w * ocean_cb.water_params.z;
    WaterSurfacePoint pt = evaluate_gerstner_waves(input.position.xz, time, ocean_cb.waves);

    float3 world_pos = pt.position;
    world_pos.y += input.position.y;

    output.world_pos   = world_pos;
    output.position    = mul(mul(float4(world_pos, 1.0), ocean_cb.view), ocean_cb.proj);
    output.normal      = pt.normal;
    output.tangent     = pt.tangent;
    output.bitangent   = pt.bitangent;
    output.uv          = input.uv;
    output.foam_factor = pt.foam_factor;

    return output;
}

[shader("fragment")]
public float4 fsmain(VertexOutput input) : SV_Target
{
    float3 V = normalize(ocean_cb.camera_pos_and_time.xyz - input.world_pos);

    WaterSurfacePoint pt;
    pt.position    = input.world_pos;
    pt.normal      = normalize(input.normal);
    pt.tangent     = normalize(input.tangent);
    pt.bitangent   = normalize(input.bitangent);
    pt.foam_factor = clamp(input.foam_factor * ocean_cb.sun_color_and_foam_scale.w, 0.0, 1.0);

    OceanShadingContext ctx;
    ctx.view_dir      = V;
    ctx.sun_dir       = normalize(ocean_cb.sun_dir_and_intensity.xyz);
    ctx.sun_color     = ocean_cb.sun_color_and_foam_scale.xyz;
    ctx.sun_intensity = ocean_cb.sun_dir_and_intensity.w;
    ctx.water_depth   = ocean_cb.water_params.x;
    ctx.water_clarity = ocean_cb.water_params.y;
    ctx.roughness     = ocean_cb.water_params.w;
    ctx.deep_color    = ocean_cb.deep_color;
    ctx.shallow_color = ocean_cb.shallow_color;

    float3 color = evaluate_ocean_surface(pt, ctx);

    // aerial perspective: blend distant water into the horizon haze
    float dist = length(ocean_cb.camera_pos_and_time.xyz - input.world_pos);
    float fog  = 1.0 - exp(-dist * 0.008);
    color      = lerp(color, OCEAN_HORIZON_SKY, fog);
    color = tonemap_aces_film(color);

    return float4(color, 1.0);
}
