import lyra;
import Common.Camera;

[[lyra::binding(0, 0)]]
ConstantBuffer<CameraUniforms> camera;

[[lyra::binding(0, 1)]]
ByteAddressBuffer vertex_buffer;

struct InstanceData
{
    float4x4 model;
    uint     position_offset;
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
    float3 position = vertex_buffer.Load<float3>(instance_data.position_offset + vertex_id * 12);
    float4 world_pos = mul(float4(position, 1.0), instance_data.model);
    output.position  = mul(mul(world_pos, camera.view), camera.proj);
    return output;
}

[shader("fragment")]
void fsmain()
{
}
