import lyra;
import Common.Camera;
import Common.Material;
import Common.Packing;

// TODO: G-Buffer rasterization shader for deferred hybrid rendering
// Supports standard PBR and user custom material shading IDs

[[lyra::binding(0, 0)]]
ConstantBuffer<CameraUniforms> camera;

struct InstanceData
{
    float4x4 model;
    float4   base_color;
    float4   emissive;
    float    metallic;
    float    roughness;
    float    clear_coat;
    float    occlusion;
    uint     shading_model;
    uint     material_id;
    uint     pad0;
    uint     pad1;
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
    float4 world_pos = mul(float4(input.position, 1.0), instance_data.model);
    output.world_pos = world_pos.xyz;
    output.position  = mul(mul(world_pos, camera.view), camera.proj);
    output.normal    = mul(input.normal, (float3x3)instance_data.model);
    output.uv        = input.uv;
    return output;
}

struct GBufferOutput
{
    float4 albedo_occlusion          : SV_Target0; // RGB: BaseColor, A: Occlusion
    float4 normal_roughness_metallic : SV_Target1; // RG: Oct Normal, B: Roughness, A: Metallic
    float4 emissive                 : SV_Target2; // RGB: Emissive radiance
    uint2  shading_model_clearcoat   : SV_Target3; // R: ShadingModel ID, G: Clearcoat (0..255)
};

[shader("fragment")]
GBufferOutput fsmain(VertexOutput input)
{
    GBufferOutput output;

    float3 N     = normalize(input.normal);
    float2 oct_n = oct_encode(N);

    // TODO: sample textures from bindless heap if bound
    output.albedo_occlusion          = float4(instance_data.base_color.rgb, instance_data.occlusion);
    output.normal_roughness_metallic = float4(oct_n, instance_data.roughness, instance_data.metallic);
    output.emissive                 = instance_data.emissive;
    uint clear_coat_quantized       = uint(clamp(instance_data.clear_coat, 0.0, 1.0) * 255.0 + 0.5);
    output.shading_model_clearcoat  = uint2(instance_data.shading_model, clear_coat_quantized);

    return output;
}
