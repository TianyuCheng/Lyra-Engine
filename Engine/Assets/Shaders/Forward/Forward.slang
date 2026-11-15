import lyra;
import Common.BSDF;
import Common.Tonemapping;

struct Camera
{
    float4x4 proj;
    float4x4 view;
    float4x4 inv_proj;
    float4x4 inv_view;
    float3   pos;
    float    sun_intensity;
    float3   sun_dir;
    float    padding;
    float3   sun_color;
    float    padding2;
};

[[lyra::group(0)]] [[lyra::binding(0)]]
ConstantBuffer<Camera> camera;

[[lyra::group(1)]] [[lyra::binding(0)]]
ByteAddressBuffer vertex_buffer;

struct InstanceData
{
    float4x4 model;
    float4   color;
    float4   params; // x = roughness, y = metallic, z = clear_coat, w = asfloat(shading_model)
    uint     pos_offset;
    uint     norm_offset;
    uint     uv_offset;
    uint     padding;
};

[[lyra::immediates]]
ConstantBuffer<InstanceData> instance_data;

struct VertexOutput
{
    float4 position  : SV_Position;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    float3 world_pos : TEXCOORD1;
};

// extensible hook for user custom material evaluator
float4 evaluate_custom_shading(VertexOutput input, float3 N, float3 V)
{
    return instance_data.color;
}

[shader("vertex")]
VertexOutput vsmain(uint vertex_id : SV_VertexID)
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

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    uint shading_model = asuint(instance_data.params.w);

    // 1. Unlit shading model
    if (shading_model == 1) {
        return instance_data.color;
    }

    float3 N = normalize(input.normal);
    float3 V = normalize(camera.pos - input.world_pos);

    // 2. Custom user material shading model
    if (shading_model >= 4) {
        return evaluate_custom_shading(input, N, V);
    }

    // 3. Toon / Cel shading model
    if (shading_model == 2) {
        float3 L = normalize(camera.sun_dir);
        float NdotL = dot(N, L);
        float toon_diff = smoothstep(0.0, 0.05, NdotL) * 0.5 + smoothstep(0.5, 0.55, NdotL) * 0.5;
        float3 lit = instance_data.color.rgb * max(toon_diff, 0.15) * camera.sun_color;
        return float4(tonemap_aces_film(lit), instance_data.color.a);
    }

    // 4. Default PBR shading model (evaluating IBSDF)
    float3 L = normalize(camera.sun_dir);
    float3 light_radiance = camera.sun_color * camera.sun_intensity;

    PBRMaterial mat;
    mat.base_color   = instance_data.color.rgb;
    mat.metallic     = instance_data.params.y;
    mat.roughness    = max(instance_data.params.x, 0.05);
    mat.reflectance  = 0.5;
    mat.clear_coat.factor    = instance_data.params.z;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint      = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    // direct analytical lighting
    float3 direct = mat.evaluate(V, L, N) * light_radiance;

    // ambient indirect illumination
    float3 ambient = mat.base_color * 0.04;

    float3 final_color = direct + ambient;

    // ACES filmic tonemapping
    final_color = tonemap_aces_film(final_color);

    return float4(final_color, instance_data.color.a);
}
