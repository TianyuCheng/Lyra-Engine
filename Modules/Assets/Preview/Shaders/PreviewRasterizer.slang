import lyra;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct PreviewDrawConstants
{
    float4x4 vp;
    float4   base_color;
    float4   view_dir_pad;
};

[[lyra::immediates]]
ConstantBuffer<PreviewDrawConstants> u_pc;

[[lyra::binding(0, 0)]]
Texture2D<float4> u_albedo;

[[lyra::binding(1, 0)]]
SamplerState u_sampler;

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0), u_pc.vp);
    output.normal   = input.normal;
    output.uv       = input.uv;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float3 norm = normalize(input.normal);
    float3 view_dir = normalize(u_pc.view_dir_pad.xyz);
    if (dot(norm, view_dir) < 0.0) {
        norm = -norm;
    }

    float3 light_key  = normalize(float3(0.5, 0.8, 0.6));
    float3 light_fill = normalize(float3(-0.5, -0.2, -0.5));

    float diff_key  = max(0.0, dot(norm, light_key)) * 0.70;
    float diff_fill = max(0.0, dot(norm, light_fill)) * 0.25;
    float ambient   = 0.25;
    float light     = ambient + diff_key + diff_fill;

    float rim = pow(1.0 - max(0.0, dot(norm, view_dir)), 3.0) * 0.15;

    float4 tex_sample = u_albedo.Sample(u_sampler, input.uv);
    float3 albedo     = u_pc.base_color.rgb * tex_sample.rgb;
    float3 final_color = saturate(albedo * light + float3(rim, rim, rim));
    return float4(final_color, u_pc.base_color.a * tex_sample.a);
}
