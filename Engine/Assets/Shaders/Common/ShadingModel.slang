module ShadingModel;

public struct SurfaceData
{
    public float3 world_pos;
    public float3 normal;
    public float2 uv;
    public float3 view_dir;
    public float4 base_color;
    public float  roughness;
    public float  metallic;
    public float  clear_coat;
    public float4 custom_data;
};

public struct LightingContext
{
    public float3 sun_dir;
    public float3 sun_color;
    public float  sun_intensity;
};

public interface IShadingModel
{
    float4 evaluate(SurfaceData surface, LightingContext lighting);
};
