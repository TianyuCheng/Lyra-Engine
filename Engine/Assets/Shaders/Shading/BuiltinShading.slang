module BuiltinShading;

import Common.ShadingModel;
import Common.BSDF;
import Common.Tonemapping;

public struct PBRShadingModel : IShadingModel
{
    public float4 evaluate(SurfaceData surface, LightingContext lighting)
    {
        float3 light_radiance = lighting.sun_color * lighting.sun_intensity;

        PBRMaterial mat;
        mat.base_color           = surface.base_color.rgb;
        mat.metallic             = surface.metallic;
        mat.roughness            = max(surface.roughness, 0.05);
        mat.reflectance          = 0.5;
        mat.clear_coat.factor    = surface.clear_coat;
        mat.clear_coat.roughness = 0.04;
        mat.clear_coat.tint      = float3(0.0);
        mat.clear_coat.thickness = 0.0;

        // direct analytical lighting
        float3 direct = mat.evaluate(surface.view_dir, lighting.sun_dir, surface.normal) * light_radiance;

        // ambient indirect illumination
        float3 ambient = surface.base_color.rgb * 0.04;

        float3 final_color = direct + ambient;

        // ACES filmic tonemapping
        final_color = tonemap_aces_film(final_color);

        return float4(final_color, surface.base_color.a);
    }
};

public struct NPRShadingModel : IShadingModel
{
    public float4 evaluate(SurfaceData surface, LightingContext lighting)
    {
        float  NdotL    = dot(surface.normal, lighting.sun_dir);
        float  npr_diff = smoothstep(0.0, 0.05, NdotL) * 0.5 + smoothstep(0.5, 0.55, NdotL) * 0.5;
        float3 lit      = surface.base_color.rgb * max(npr_diff, 0.15) * lighting.sun_color;
        return float4(tonemap_aces_film(lit), surface.base_color.a);
    }
};

public struct UnlitShadingModel : IShadingModel
{
    public float4 evaluate(SurfaceData surface, LightingContext lighting)
    {
        return surface.base_color;
    }
};
