module BuiltinShading;

import Common.ShadingModel;
import Common.BSDF;
import Common.Tonemapping;

public struct StandardShadingModel : IShadingModel
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

        // ambient indirect illumination (diffuse skylight contribution + ambient floor)
        float3 ambient = surface.base_color.rgb * max(lighting.sun_intensity * 0.03, 0.04);

        float3 final_color = direct + ambient;

        // ACES filmic tonemapping
        final_color = tonemap_aces_film(final_color);

        return float4(final_color, surface.base_color.a);
    }
};

public struct ClearcoatShadingModel : IShadingModel
{
    public float4 evaluate(SurfaceData surface, LightingContext lighting)
    {
        float3 light_radiance = lighting.sun_color * lighting.sun_intensity;

        PBRMaterial mat;
        mat.base_color           = surface.base_color.rgb;
        mat.metallic             = surface.metallic;
        mat.roughness            = max(surface.roughness, 0.05);
        mat.reflectance          = 0.5;
        mat.clear_coat.factor    = max(surface.clear_coat, 0.5);
        mat.clear_coat.roughness = 0.04;
        mat.clear_coat.tint      = float3(0.0);
        mat.clear_coat.thickness = 0.0;

        float3 direct = mat.evaluate(surface.view_dir, lighting.sun_dir, surface.normal) * light_radiance;
        float3 ambient = surface.base_color.rgb * max(lighting.sun_intensity * 0.03, 0.04);
        float3 final_color = direct + ambient;
        final_color = tonemap_aces_film(final_color);

        return float4(final_color, surface.base_color.a);
    }
};

public struct SubsurfaceShadingModel : IShadingModel
{
    public float4 evaluate(SurfaceData surface, LightingContext lighting)
    {
        float3 light_radiance = lighting.sun_color * lighting.sun_intensity;

        PBRMaterial mat;
        mat.base_color           = surface.base_color.rgb;
        mat.metallic             = 0.0;
        mat.roughness            = max(surface.roughness, 0.1);
        mat.reflectance          = 0.5;
        mat.clear_coat.factor    = 0.0;
        mat.clear_coat.roughness = 0.04;
        mat.clear_coat.tint      = float3(0.0);
        mat.clear_coat.thickness = 0.0;

        float3 direct = mat.evaluate(surface.view_dir, lighting.sun_dir, surface.normal) * light_radiance;
        float3 ambient = surface.base_color.rgb * max(lighting.sun_intensity * 0.05, 0.06);
        float3 final_color = direct + ambient;
        final_color = tonemap_aces_film(final_color);

        return float4(final_color, surface.base_color.a);
    }
};

public struct UnlitShadingModel : IShadingModel
{
    public float4 evaluate(SurfaceData surface, LightingContext lighting)
    {
        return surface.base_color;
    }
};
