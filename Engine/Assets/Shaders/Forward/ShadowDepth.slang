import lyra;

struct ShadowCascadeData
{
    float4x4 light_view_proj;
};

ConstantBuffer<ShadowCascadeData> cascade_data;

struct InstanceData
{
    float4x4 model;
};

[[lyra::immediates]]
ConstantBuffer<InstanceData> instance_data;

struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    float4 world_pos = mul(float4(input.position, 1.0), instance_data.model);
    output.position  = mul(world_pos, cascade_data.light_view_proj);
    return output;
}

[shader("fragment")]
void fsmain()
{
}
