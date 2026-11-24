module Forward;

import lyra;
import Common.BSDF;
import Common.Lighting;
import Common.Tonemapping;
import Common.ShadingModel;
import Common.SphericalHarmonics;
import Shading.BuiltinShading;

public struct Camera
{
    public float4x4 proj;
    public float4x4 view;
    public float4x4 inv_proj;
    public float4x4 inv_view;
    public float4   pos_and_intensity;
    public float4   sun_dir_and_pad;
    public float4   sun_color_and_pad;
    public float4x4 light_view_proj;
    public float4   cluster_params;
    public float4   cluster_dims;
    public float4   env_sh[9];

    public property float3 pos           { get { return pos_and_intensity.xyz; } }
    public property float  sun_intensity { get { return pos_and_intensity.w; } }
    public property float3 sun_dir       { get { return sun_dir_and_pad.xyz; } }
    public property float3 sun_color     { get { return sun_color_and_pad.xyz; } }
    public property float2 screen_size   { get { return cluster_params.xy; } }
    public property float  near_plane    { get { return cluster_params.z; } }
    public property float  far_plane     { get { return cluster_params.w; } }
    public property uint3  grid_dim      { get { return uint3(cluster_dims.xyz); } }
    public property uint   num_lights    { get { return uint(cluster_dims.w); } }
};

[[lyra::binding(0, 0)]]
public ConstantBuffer<Camera> camera;

[[lyra::binding(1, 0)]]
public Texture2D<float> shadow_map;

[[lyra::binding(2, 0)]]
public SamplerState shadow_sampler;

public struct ClusterRecord
{
    public uint offset;
    public uint count;
};

[[lyra::binding(3, 0)]]
public StructuredBuffer<ClusterRecord> cluster_grid;

[[lyra::binding(4, 0)]]
public StructuredBuffer<uint> light_index_list;

[[lyra::binding(5, 0)]]
public StructuredBuffer<LightData> lights;

[[lyra::binding(6, 0)]]
public TextureCube<float4> environment_map;

[[lyra::binding(7, 0)]]
public Texture2D<float4> brdf_lut;

[[lyra::binding(8, 0)]]
public SamplerState environment_sampler;

[[lyra::binding(0, 1)]]
public ByteAddressBuffer vertex_buffer;

public struct InstanceData
{
    public float4x4 model;
    public float4   base_color;
    public float    roughness;
    public float    metallic;
    public float    clear_coat;
    public uint     material_id;
    public uint     position_offset;
    public uint     normal_offset;
    public uint     uv_offset;
    public uint     padding1;
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
    float3 position = vertex_buffer.Load<float3>(instance_data.position_offset + vertex_id * 12);
    float3 normal   = vertex_buffer.Load<float3>(instance_data.normal_offset + vertex_id * 12);
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
    surf.base_color = instance_data.base_color;
    surf.roughness  = instance_data.roughness;
    surf.metallic   = instance_data.metallic;
    surf.clear_coat = instance_data.clear_coat;
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

public float evaluate_shadow(float3 world_pos)
{
    float4 light_clip = mul(float4(world_pos, 1.0), camera.light_view_proj);
    if (light_clip.w <= 0.0) return 1.0;
    float3 shadow_coords = light_clip.xyz / light_clip.w;

    #if defined(__spirv__) || defined(LYRA_VULKAN)
    float2 shadow_uv = float2(shadow_coords.x * 0.5 + 0.5, shadow_coords.y * 0.5 + 0.5);
    #else
    float2 shadow_uv = float2(shadow_coords.x * 0.5 + 0.5, 0.5 - shadow_coords.y * 0.5);
    #endif

    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0) {
        return 1.0;
    }

    float current_depth = shadow_coords.z;
    if (current_depth > 1.0) return 1.0;

    float shadow = 0.0;
    float bias = 0.002;
    float2 texel_size = float2(1.0 / 2048.0, 1.0 / 2048.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float depth_sample = shadow_map.SampleLevel(shadow_sampler, shadow_uv + float2(x, y) * texel_size, 0).r;
            shadow += (current_depth - bias <= depth_sample) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

public float4 evaluate_forward_shading<TModel : IShadingModel>(VertexOutput input)
{
    SurfaceData     surf  = extract_surface_data(input);
    LightingContext light = extract_lighting_context();
    float shadow = evaluate_shadow(surf.world_pos);

    PBRMaterial mat;
    mat.base_color           = surf.base_color.rgb;
    mat.metallic             = surf.metallic;
    mat.roughness            = max(surf.roughness, 0.04);
    mat.reflectance          = 0.5;
    mat.clear_coat.factor    = 0.0;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint      = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    float3 light_radiance = light.sun_color * light.sun_intensity;
    float3 direct = mat.evaluate(surf.view_dir, light.sun_dir, surf.normal) * light_radiance * shadow;

    // Evaluate clustered point and spot lights (Forward+)
    if (camera.grid_dim.x > 0 && camera.grid_dim.y > 0 && camera.grid_dim.z > 0) {
        uint tile_x = uint(input.position.x / 16.0);
        uint tile_y = uint(input.position.y / 16.0);

        float4 view_pos = mul(float4(surf.world_pos, 1.0), camera.view);
        float linear_z = max(abs(view_pos.z), camera.near_plane);
        float log_ratio = log(max(camera.far_plane, camera.near_plane + 0.1) / max(camera.near_plane, 0.001));
        float slice_f = (log(linear_z / max(camera.near_plane, 0.001)) / log_ratio) * float(camera.grid_dim.z);
        uint tile_z = uint(clamp(slice_f, 0.0, float(camera.grid_dim.z - 1)));

        uint3 cluster_coord = uint3(
            clamp(tile_x, 0u, camera.grid_dim.x - 1u),
            clamp(tile_y, 0u, camera.grid_dim.y - 1u),
            tile_z
        );

        uint cluster_index = cluster_coord.x +
                             cluster_coord.y * camera.grid_dim.x +
                             cluster_coord.z * camera.grid_dim.x * camera.grid_dim.y;

        ClusterRecord record = cluster_grid[cluster_index];
        for (uint li = 0; li < record.count; ++li) {
            uint light_idx = light_index_list[record.offset + li];
            LightData pt_light = lights[light_idx];
            if (pt_light.type == 0) {
                continue; // Directional sun is evaluated explicitly
            }

            float3 to_light = pt_light.position - surf.world_pos;
            float dist = length(to_light);
            if (dist >= pt_light.range || dist < 1e-4) {
                continue;
            }

            float3 L_local = to_light / dist;
            float norm_dist = dist / pt_light.range;
            float window = saturate(1.0 - norm_dist * norm_dist * norm_dist * norm_dist);
            float atten = (window * window) / (dist * dist + 1.0);

            // Spot light cone attenuation
            if (pt_light.type == 2) {
                float cos_theta = dot(-L_local, normalize(pt_light.direction));
                float cos_outer = cos(pt_light.spot_outer_angle);
                float cos_inner = cos(pt_light.spot_inner_angle);
                float spot_factor = saturate((cos_theta - cos_outer) / max(cos_inner - cos_outer, 1e-4));
                atten *= spot_factor * spot_factor;
            }

            float3 pt_radiance = pt_light.color * pt_light.intensity * atten;
            direct += mat.evaluate(surf.view_dir, L_local, surf.normal) * pt_radiance;
        }
    }

    // -------------------------------------------------------------------------
    // Image-Based Lighting (IBL): Split-Sum Specular + SH9 Diffuse Irradiance
    // -------------------------------------------------------------------------
    float3 V = surf.view_dir;
    float3 N = surf.normal;
    float3 R = reflect(-V, N);
    float NoV = clamp(dot(N, V), 1e-4, 1.0);

    float lod = surf.roughness * 5.0;
    float3 prefiltered_color = environment_map.SampleLevel(environment_sampler, R, lod).rgb;
    float2 env_brdf = brdf_lut.SampleLevel(environment_sampler, float2(NoV, surf.roughness), 0.0).rg;

    float3 F0 = lerp(float3(0.04), surf.base_color.rgb, surf.metallic);
    float3 F_env = F0 + (max(float3(1.0 - surf.roughness), F0) - F0) * pow(1.0 - NoV, 5.0);
    float3 specular_ibl = prefiltered_color * (F_env * env_brdf.x + env_brdf.y);

    SH9Color sh;
    [unroll]
    for (int i = 0; i < 9; ++i) {
        sh.c[i] = camera.env_sh[i].rgb;
    }
    float3 diffuse_irradiance = evaluate_sh_irradiance(sh, N);

    float3 ambient;
    if (dot(diffuse_irradiance, diffuse_irradiance) < 1e-6) {
        ambient = surf.base_color.rgb * max(light.sun_intensity * 0.15, 0.04);
    } else {
        float3 kD = (1.0 - F_env) * (1.0 - surf.metallic);
        ambient = kD * surf.base_color.rgb * diffuse_irradiance + specular_ibl;
    }
    float3 final_color = tonemap_aces_film(direct + ambient);

    return float4(final_color, surf.base_color.a);
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
public float4 fs_standard(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<StandardShadingModel>(input);
}

[shader("fragment")]
public float4 fs_clearcoat(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<ClearcoatShadingModel>(input);
}

[shader("fragment")]
public float4 fs_subsurface(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<SubsurfaceShadingModel>(input);
}

[shader("fragment")]
public float4 fs_unlit(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<UnlitShadingModel>(input);
}

// Default forward fragment entry point (evaluates Standard)
[shader("fragment")]
public float4 fsmain(VertexOutput input) : SV_Target
{
    return evaluate_forward_shading<StandardShadingModel>(input);
}
