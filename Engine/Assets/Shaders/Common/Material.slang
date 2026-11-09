module Material;

public enum ShadingModel : uint
{
    PBR_METALLIC_ROUGHNESS = 0,
    UNLIT                  = 1,
    TOON_CEL               = 2,
    SUBSURFACE             = 3,
    CUSTOM                 = 4,
};

public struct MaterialData
{
    public float4 base_color_factor;
    public float3 emissive_factor;
    public float  roughness_factor;
    public float  metallic_factor;
    public uint   shading_model;
    public uint   base_color_texture_idx;
    public uint   normal_texture_idx;
    public uint   roughness_metallic_texture_idx;
    public uint   custom_params_offset;
    public float3 custom_data;
};

// interface for custom material shading evaluation
public interface IMaterialShader
{
    // TODO: evaluate custom material shading
    float4 evaluate(MaterialData material, float3 world_pos, float3 normal, float3 view_dir);
};
