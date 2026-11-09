import lyra;
import Common.BSDF;
import Common.Tonemapping;

// Deferred lighting & hybrid ray tracing composition pass
Texture2D<float4> gbuffer_albedo;
Texture2D<float4> gbuffer_normal;
Texture2D<uint4>  gbuffer_material;
Texture2D<float>  gbuffer_depth;

struct FragmentOutput
{
    float4 lit_color : SV_Target;
};

// extensible hook for user custom material evaluator
float3 evaluate_custom_material(uint shading_model, uint material_id, float3 albedo, float3 N, float3 V)
{
    return albedo;
}

[shader("fragment")]
FragmentOutput fsmain(float4 pos : SV_Position, float2 uv : TEXCOORD0)
{
    int2 pixel = int2(pos.xy);

    float4 albedo_metallic = gbuffer_albedo.Load(int3(pixel, 0));
    float4 normal_rough    = gbuffer_normal.Load(int3(pixel, 0));
    uint4  mat_info        = gbuffer_material.Load(int3(pixel, 0));
    float  depth           = gbuffer_depth.Load(int3(pixel, 0));

    if (depth >= 1.0) {
        discard;
    }

    uint shading_model = mat_info.r;

    // 1. Unlit shading model
    if (shading_model == 1) {
        FragmentOutput out_unlit;
        out_unlit.lit_color = float4(albedo_metallic.rgb, 1.0);
        return out_unlit;
    }

    float3 N = normalize(normal_rough.rgb * 2.0 - 1.0);
    float3 V = float3(0.0, 0.0, 1.0); // view direction in view-space
    float3 L = normalize(float3(0.5, 1.0, 0.3));

    // 2. Custom shading model
    if (shading_model >= 4) {
        FragmentOutput out_custom;
        out_custom.lit_color = float4(evaluate_custom_material(shading_model, mat_info.g, albedo_metallic.rgb, N, V), 1.0);
        return out_custom;
    }

    // 3. PBR evaluation
    PBRMaterial mat;
    mat.base_color   = albedo_metallic.rgb;
    mat.metallic     = albedo_metallic.a;
    mat.roughness    = max(normal_rough.a, 0.04);
    mat.reflectance  = 0.5;
    mat.clear_coat.factor    = 0.0;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint      = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    float3 light_radiance = float3(2.5, 2.4, 2.2);
    float3 direct = mat.evaluate(V, L, N) * light_radiance;
    float3 ambient = mat.base_color * 0.04;

    float3 final_color = tonemap_aces_film(direct + ambient);

    FragmentOutput output;
    output.lit_color = float4(final_color, 1.0);
    return output;
}
