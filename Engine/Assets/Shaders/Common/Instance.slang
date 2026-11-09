module Instance;

public struct InstanceData
{
    public float4x4 model;
    public float4x4 prev_model;
    public float3   bounds_center;
    public float    bounds_radius;
    public uint     material_id;
    public uint     custom_index;
    public float2   padding;
};
