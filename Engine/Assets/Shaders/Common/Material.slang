module Material;

public enum ShadingModel : uint
{
    STANDARD   = 0,
    CLEARCOAT  = 1,
    SUBSURFACE = 2,
    UNLIT      = 3,
    CUSTOM     = 4,
};

// 16-byte aligned unified material parameters layout (std140/std430, 80 bytes total)
public struct MaterialParams
{
    public float4 base_color;
    public float  metallic;
    public float  roughness;
    public float  clear_coat;
    public float  clear_coat_roughness;
    public float4 emissive;
    public float4 custom_data;
    public uint   shading_model_id;
    public float  alpha_cutoff;
    public uint   flags;
    public uint   custom_params_offset;
};

// interface for custom material shading evaluation
public interface IMaterialShader
{
    // TODO: evaluate custom material shading
    float4 evaluate(MaterialParams material, float3 world_pos, float3 normal, float3 view_dir);
};
