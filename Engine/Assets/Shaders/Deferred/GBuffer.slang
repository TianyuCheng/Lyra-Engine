import lyra;

// TODO: G-Buffer rasterization shader for deferred hybrid rendering
// Supports standard PBR and user custom material shading IDs

struct Camera
{
    float4x4 proj;
    float4x4 view;
    float3   pos;
    float    padding;
};

ConstantBuffer<Camera> camera;

struct InstanceData
{
    float4x4 model;
    float4   base_color;
    float    roughness;
    float    metallic;
    uint     shading_model; // 0 = PBR, 1 = Unlit, 2 = Toon, 3 = Subsurface, 4 = Custom
    uint     material_id;
};

[[lyra::immediates]]
ConstantBuffer<InstanceData> instance_data;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput
{
    float4 position  : SV_Position;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    float3 world_pos : TEXCOORD1;
};

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    float4 world_pos = mul(instance_data.model, float4(input.position, 1.0));
    output.world_pos = world_pos.xyz;
    output.position  = mul(camera.proj, mul(camera.view, world_pos));
    output.normal    = mul((float3x3)instance_data.model, input.normal);
    output.uv        = input.uv;
    return output;
}

struct GBufferOutput
{
    float4 albedo_metallic  : SV_Target0; // RGB: Albedo, A: Metallic
    float4 normal_roughness : SV_Target1; // RGB: World Normal (encoded), A: Roughness
    uint4  material_data    : SV_Target2; // R: ShadingModel, G: MaterialID, BA: Custom params
};

[shader("fragment")]
GBufferOutput fsmain(VertexOutput input)
{
    GBufferOutput output;

    float3 N = normalize(input.normal);

    // TODO: sample textures from bindless heap if bound
    output.albedo_metallic  = float4(instance_data.base_color.rgb, instance_data.metallic);
    output.normal_roughness = float4(N * 0.5 + 0.5, instance_data.roughness);
    output.material_data    = uint4(instance_data.shading_model, instance_data.material_id, 0, 0);

    return output;
}
