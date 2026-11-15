import lyra;

struct Camera
{
    float4x4 proj;
    float4x4 view;
    float4x4 inv_proj;
    float4x4 inv_view;
    float3   pos;
    float    padding;
};

ConstantBuffer<Camera> camera;

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
    output.position  = mul(mul(world_pos, camera.view), camera.proj);
    return output;
}

[shader("fragment")]
void fsmain()
{
}
