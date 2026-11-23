import lyra;
import Common.BSDF;
import Common.Packing;
import Common.Tonemapping;
import Common.Lighting;

// Deferred lighting & hybrid ray tracing composition pass
[[lyra::binding(0, 0)]] Texture2D<float4> gbuffer_albedo;
[[lyra::binding(1, 0)]] Texture2D<float4> gbuffer_normal;
[[lyra::binding(2, 0)]] Texture2D<float4> gbuffer_emissive;
[[lyra::binding(3, 0)]] Texture2D<uint2>  gbuffer_shading_model;
[[lyra::binding(4, 0)]] Texture2D<float>  gbuffer_depth;

struct CameraUniforms
{
    float4x4 proj;
    float4x4 view;
    float4x4 inv_proj;
    float4x4 inv_view;
    float4   pos_and_intensity;
    float4   sun_dir_and_pad;
    float4   sun_color_and_pad;
    float4x4 light_view_proj;
    float4   cluster_params;
    float4   cluster_dims;

    property float3 pos           { get { return pos_and_intensity.xyz; } }
    property float  sun_intensity { get { return pos_and_intensity.w; } }
    property float3 sun_dir       { get { return sun_dir_and_pad.xyz; } }
    property float3 sun_color     { get { return sun_color_and_pad.xyz; } }
    property float2 screen_size   { get { return cluster_params.xy; } }
    property float  near_plane    { get { return cluster_params.z; } }
    property float  far_plane     { get { return cluster_params.w; } }
    property uint3  grid_dim      { get { return uint3(cluster_dims.xyz); } }
    property uint   num_lights    { get { return uint(cluster_dims.w); } }
};

[[lyra::binding(5, 0)]]
ConstantBuffer<CameraUniforms> camera;

[[lyra::binding(6, 0)]] Texture2D<float> shadow_map;
[[lyra::binding(7, 0)]] SamplerState    shadow_sampler;

struct ClusterRecord
{
    uint offset;
    uint count;
};

[[lyra::binding(8, 0)]]
StructuredBuffer<ClusterRecord> cluster_grid;

[[lyra::binding(9, 0)]]
StructuredBuffer<uint> light_index_list;

[[lyra::binding(10, 0)]]
StructuredBuffer<LightData> lights;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct FragmentOutput
{
    float4 lit_color : SV_Target;
};

[shader("vertex")]
VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    static const float2 quad_vertices[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0,  1.0)
    };

    float2 p = quad_vertices[vertex_id];

    VertexOutput output;
    output.position = float4(p, 0.0, 1.0);
    output.uv       = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    return output;
}

// extensible hook for user custom material evaluator
float3 evaluate_custom_material(uint shading_model, uint material_id, float3 albedo, float3 N, float3 V)
{
    return albedo;
}

float evaluate_shadow(float3 world_pos)
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

[shader("fragment")]
FragmentOutput fsmain(VertexOutput input)
{
    int2 pixel = int2(input.position.xy);

    float4 albedo_occlusion      = gbuffer_albedo.Load(int3(pixel, 0));
    float4 normal_rough_metallic = gbuffer_normal.Load(int3(pixel, 0));
    float3 emissive             = gbuffer_emissive.Load(int3(pixel, 0)).rgb;
    uint2  shading_model_cc      = gbuffer_shading_model.Load(int3(pixel, 0));
    float  depth                = gbuffer_depth.Load(int3(pixel, 0));

    if (depth >= 1.0) {
        discard;
    }

    uint  shading_model = shading_model_cc.x;
    float clear_coat    = float(shading_model_cc.y) / 255.0;
    float occlusion     = albedo_occlusion.a;
    float roughness     = max(normal_rough_metallic.z, 0.04);
    float metallic      = normal_rough_metallic.w;

    // 0. Unlit shading model
    if (shading_model == 0) {
        FragmentOutput out_unlit;
        out_unlit.lit_color = float4(albedo_occlusion.rgb + emissive, 1.0);
        return out_unlit;
    }

    // Reconstruct world position from depth and screen UV
    float2 ndc_xy = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
    float4 clip_pos = float4(ndc_xy, depth, 1.0);

    float4 view_pos = mul(clip_pos, camera.inv_proj);
    view_pos /= max(view_pos.w, 1e-6);
    float4 world_pos = mul(view_pos, camera.inv_view);

    float3 N = oct_decode(normal_rough_metallic.xy);
    float3 V = normalize(camera.pos - world_pos.xyz);
    float3 L = normalize(camera.sun_dir);

    // 2. Custom shading model
    if (shading_model >= 4) {
        FragmentOutput out_custom;
        out_custom.lit_color = float4(evaluate_custom_material(shading_model, 0, albedo_occlusion.rgb, N, V) + emissive, 1.0);
        return out_custom;
    }

    // 3. PBR evaluation matching Forward PBRShadingModel
    PBRMaterial mat;
    mat.base_color   = albedo_occlusion.rgb;
    mat.metallic     = metallic;
    mat.roughness    = roughness;
    mat.reflectance  = 0.5;
    mat.clear_coat.factor    = clear_coat;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint      = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    float shadow = evaluate_shadow(world_pos.xyz);
    float3 light_radiance = camera.sun_color * camera.sun_intensity;
    float3 direct = mat.evaluate(V, L, N) * light_radiance * shadow;

    // Evaluate clustered point and spot lights
    if (camera.grid_dim.x > 0 && camera.grid_dim.y > 0 && camera.grid_dim.z > 0) {
        uint tile_x = uint(pixel.x / 16);
        uint tile_y = uint(pixel.y / 16);

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
            LightData light = lights[light_idx];
            if (light.type == 0) {
                continue; // Directional sun is evaluated explicitly
            }

            float3 to_light = light.position - world_pos.xyz;
            float dist = length(to_light);
            if (dist >= light.range || dist < 1e-4) {
                continue;
            }

            float3 L_local = to_light / dist;
            float norm_dist = dist / light.range;
            float window = saturate(1.0 - norm_dist * norm_dist * norm_dist * norm_dist);
            float atten = (window * window) / (dist * dist + 1.0);

            // Spot light cone attenuation
            if (light.type == 2) {
                float cos_theta = dot(-L_local, normalize(light.direction));
                float cos_outer = cos(light.spot_outer_angle);
                float cos_inner = cos(light.spot_inner_angle);
                float spot_factor = saturate((cos_theta - cos_outer) / max(cos_inner - cos_outer, 1e-4));
                atten *= spot_factor * spot_factor;
            }

            float3 pt_radiance = light.color * light.intensity * atten;
            direct += mat.evaluate(V, L_local, N) * pt_radiance;
        }
    }

    float3 ambient = albedo_occlusion.rgb * max(camera.sun_intensity * 0.15, 0.04) * occlusion;

    float3 final_color = tonemap_aces_film(direct + ambient + emissive);

    FragmentOutput output;
    output.lit_color = float4(final_color, 1.0);
    return output;
}
