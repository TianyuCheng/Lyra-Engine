import lyra;
import Common.Tonemapping;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct TonemapParams
{
    float exposure;
    uint  operator_type;     // 0 = ACES, 1 = Reinhard, 2 = AgX, 3 = Uncharted2
    float bloom_intensity;   // bloom multiplier
    uint  use_auto_exposure; // 0 = manual, 1 = read from exposure_texture
};

[[lyra::immediates]]
ConstantBuffer<TonemapParams> params;

[[lyra::binding(0, 0)]]
Texture2D<float4> hdr_texture;

[[lyra::binding(1, 0)]]
SamplerState linear_sampler;

[[lyra::binding(2, 0)]]
Texture2D<float4> bloom_texture;

[[lyra::binding(3, 0)]]
Texture2D<float4> exposure_texture;

[shader("vertex")]
VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    static const float2 quad_vertices[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0,  1.0)
    };

    float2 p = quad_vertices[vertex_id];

    VertexOutput output;
    output.position = float4(p, 0.0, 1.0);
    output.uv       = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float4 hdr_sample   = hdr_texture.Sample(linear_sampler, input.uv);
    float3 bloom_sample = bloom_texture.Sample(linear_sampler, input.uv).rgb;

    // Composite bloom in linear HDR space before tonemapping
    float3 hdr_color = hdr_sample.rgb + bloom_sample * params.bloom_intensity;

    // Determine exposure scale
    float exposure = max(params.exposure, 0.001);
    if (params.use_auto_exposure != 0) {
        float auto_scale = exposure_texture.Load(int3(0, 0, 0)).r;
        if (auto_scale > 0.0) {
            exposure = auto_scale;
        }
    }

    float3 color = hdr_color * exposure;

    float3 mapped = color;
    switch (params.operator_type) {
        case 0:
            mapped = tonemap_aces_film(color);
            break;
        case 1:
            mapped = tonemap_reinhard(color);
            break;
        case 2:
            mapped = tonemap_agx(color);
            break;
        case 3:
            mapped = tonemap_uncharted2(color);
            break;
        default:
            mapped = tonemap_aces_film(color);
            break;
    }

    float3 srgb = linear_to_srgb(mapped);
    return float4(srgb, hdr_sample.a);
}
