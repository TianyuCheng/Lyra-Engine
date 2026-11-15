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

[[lyra::group(0)]] [[lyra::binding(0)]]
ConstantBuffer<Camera> camera;

[[lyra::group(1)]] [[lyra::binding(0)]]
ByteAddressBuffer vertex_buffer;

struct InstanceData
{
    float4x4 model;
    uint     pos_offset;
    uint     padding0;
    uint     padding1;
    uint     padding2;
};

[[lyra::immediates]]
ConstantBuffer<InstanceData> instance_data;

struct VertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    float3 position = vertex_buffer.Load<float3>(instance_data.pos_offset + vertex_id * 12);
    float4 world_pos = mul(float4(position, 1.0), instance_data.model);
    output.position  = mul(mul(world_pos, camera.view), camera.proj);
    return output;
}

[shader("fragment")]
void fsmain()
{
}
