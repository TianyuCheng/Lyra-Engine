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
    uint  operator_type; // 0 = ACES, 1 = Reinhard, 2 = AgX, 3 = Uncharted2
    float2 padding;
};

[[lyra::immediates]]
ConstantBuffer<TonemapParams> params;

[[lyra::binding(0, 0)]]
Texture2D<float4> hdr_texture;

[[lyra::binding(1, 0)]]
SamplerState linear_sampler;

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
    output.uv       = p * 0.5 + 0.5;
#if defined(__spirv__) || defined(LYRA_VULKAN)
    output.uv.y = 1.0 - output.uv.y;
#endif
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float4 hdr_sample = hdr_texture.Sample(linear_sampler, input.uv);
    float3 color      = hdr_sample.rgb * max(params.exposure, 0.001);

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
