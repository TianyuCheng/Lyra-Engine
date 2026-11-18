module Forward;

import lyra;
import Common.ShadingModel;
import Shading.BuiltinShading;

public struct Camera
{
    public float4x4 proj;
    public float4x4 view;
    public float4x4 inv_proj;
    public float4x4 inv_view;
    public float3   pos;
    public float    sun_intensity;
    public float3   sun_dir;
    public float    padding;
    public float3   sun_color;
    public float    padding2;
};

[[lyra::binding(0, 0)]]
public ConstantBuffer<Camera> camera;

[[lyra::binding(0, 1)]]
public ByteAddressBuffer vertex_buffer;

public struct InstanceData
{
    public float4x4 model;
    public uint     pos_offset;
    public uint     norm_offset;
    public uint     uv_offset;
    public uint     material_id;
};

[[lyra::immediates]]
public ConstantBuffer<InstanceData> instance_data;

public struct VertexOutput
{
    public float4 position  : SV_Position;
    public float3 normal    : NORMAL;
    public float2 uv        : TEXCOORD0;
    public float3 world_pos : TEXCOORD1;
};

public VertexOutput forward_vsmain(uint vertex_id : SV_VertexID)
{
    VertexOutput output;

    // pull vertex attributes directly from byte-address storage buffer
    float3 position = vertex_buffer.Load<float3>(instance_data.pos_offset + vertex_id * 12);
    float3 normal   = vertex_buffer.Load<float3>(instance_data.norm_offset + vertex_id * 12);
    float2 uv       = vertex_buffer.Load<float2>(instance_data.uv_offset + vertex_id * 8);

    float4 world_pos = mul(float4(position, 1.0), instance_data.model);
    output.world_pos = world_pos.xyz;
    output.position  = mul(mul(world_pos, camera.view), camera.proj);
    output.normal    = mul(normal, (float3x3)instance_data.model);
    output.uv        = uv;
    return output;
}

public SurfaceData extract_surface_data(VertexOutput input)
{
    SurfaceData surf;
    surf.world_pos  = input.world_pos;
    surf.normal     = normalize(input.normal);
    surf.uv         = input.uv;
    surf.base_color = float4(1.0, 1.0, 1.0, 1.0);
    surf.roughness  = 0.5;
    surf.metallic   = 0.0;
    surf.clear_coat = 0.0;
    surf.view_dir   = normalize(camera.pos - input.world_pos);
    return surf;
}

public LightingContext extract_lighting_context()
{
    LightingContext light;
    light.sun_dir       = normalize(camera.sun_dir);
    light.sun_color     = camera.sun_color;
    light.sun_intensity = camera.sun_intensity;
    return light;
}

public float4 evaluate_forward_shading<TModel : IShadingModel>(VertexOutput input)
{
    SurfaceData     surf  = extract_surface_data(input);
    LightingContext light = extract_lighting_context();
    TModel          model;
    return model.evaluate(surf, light);
}

// ---------------------------------------------------------------------------
// Specialized entry points
// ---------------------------------------------------------------------------

[shader("vertex")]
public VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    return forward_vsmain(vertex_id);
}

[shader("fragment")]
public float4 fs_pbr(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<PBRShadingModel>(input);
}

[shader("fragment")]
public float4 fs_npr(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<NPRShadingModel>(input);
}

[shader("fragment")]
public float4 fs_unlit(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<UnlitShadingModel>(input);
}

// Default forward fragment entry point (evaluates PBR)
[shader("fragment")]
public float4 fsmain(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<PBRShadingModel>(input);
}
