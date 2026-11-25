import lyra;
import Common.Lighting;

struct LightCullingParams
{
    float4x4 view_matrix;
    float4x4 proj_matrix;
    float4x4 inv_proj_matrix;
    float2   screen_resolution;
    float    near_plane;
    float    far_plane;
    uint     cluster_grid_x;
    uint     cluster_grid_y;
    uint     cluster_grid_z;
    uint     num_lights;

    property uint3 cluster_grid_dim { get { return uint3(cluster_grid_x, cluster_grid_y, cluster_grid_z); } }
};

[[lyra::immediates]]
ConstantBuffer<LightCullingParams> params;

[[lyra::binding(0, 0)]]
StructuredBuffer<LightData> lights;

[[lyra::binding(1, 0)]]
RWStructuredBuffer<ClusterRecord> cluster_grid;

[[lyra::binding(2, 0)]]
RWStructuredBuffer<uint> light_index_list;

static const uint MAX_LIGHTS_PER_CLUSTER = 64;

[shader("compute")]
[numthreads(32, 32, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint3 cluster_coord = dispatch_thread_id;
    if (cluster_coord.x >= params.cluster_grid_dim.x ||
        cluster_coord.y >= params.cluster_grid_dim.y ||
        cluster_coord.z >= params.cluster_grid_dim.z) {
        return;
    }

    uint cluster_index = cluster_coord.x +
                         cluster_coord.y * params.cluster_grid_dim.x +
                         cluster_coord.z * params.cluster_grid_dim.x * params.cluster_grid_dim.y;

    uint base_offset = cluster_index * MAX_LIGHTS_PER_CLUSTER;
    uint visible_count = 0;

    // 1. Exponential logarithmic depth slice boundaries
    float log_ratio = log(max(params.far_plane, params.near_plane + 0.1) / max(params.near_plane, 0.001));
    float z_near = params.near_plane * exp(log_ratio * (float(cluster_coord.z) / float(params.cluster_grid_dim.z)));
    float z_far  = params.near_plane * exp(log_ratio * (float(cluster_coord.z + 1) / float(params.cluster_grid_dim.z)));

    // 2. Tile boundaries in screen and NDC space (16x16 tile grid)
    float2 tile_size = float2(16.0, 16.0);
    float2 min_pixel = float2(cluster_coord.xy) * tile_size;
    float2 max_pixel = min(min_pixel + tile_size, params.screen_resolution);

    float2 ndc_min = float2(
        (min_pixel.x / params.screen_resolution.x) * 2.0 - 1.0,
        1.0 - (max_pixel.y / params.screen_resolution.y) * 2.0
    );
    float2 ndc_max = float2(
        (max_pixel.x / params.screen_resolution.x) * 2.0 - 1.0,
        1.0 - (min_pixel.y / params.screen_resolution.y) * 2.0
    );

    // 3. Unproject the 4 corner rays into view space
    float4 c0 = mul(float4(ndc_min.x, ndc_min.y, 0.5, 1.0), params.inv_proj_matrix);
    float4 c1 = mul(float4(ndc_max.x, ndc_min.y, 0.5, 1.0), params.inv_proj_matrix);
    float4 c2 = mul(float4(ndc_min.x, ndc_max.y, 0.5, 1.0), params.inv_proj_matrix);
    float4 c3 = mul(float4(ndc_max.x, ndc_max.y, 0.5, 1.0), params.inv_proj_matrix);

    float3 ray0 = c0.xyz / c0.w;
    float3 ray1 = c1.xyz / c1.w;
    float3 ray2 = c2.xyz / c2.w;
    float3 ray3 = c3.xyz / c3.w;

    // Scale rays so that |z| == 1.0
    ray0 *= 1.0 / max(abs(ray0.z), 1e-6);
    ray1 *= 1.0 / max(abs(ray1.z), 1e-6);
    ray2 *= 1.0 / max(abs(ray2.z), 1e-6);
    ray3 *= 1.0 / max(abs(ray3.z), 1e-6);

    // 8 points defining the cluster frustum in view space
    float3 p_near0 = ray0 * z_near;
    float3 p_near1 = ray1 * z_near;
    float3 p_near2 = ray2 * z_near;
    float3 p_near3 = ray3 * z_near;

    float3 p_far0 = ray0 * z_far;
    float3 p_far1 = ray1 * z_far;
    float3 p_far2 = ray2 * z_far;
    float3 p_far3 = ray3 * z_far;

    float3 aabb_min = min(min(min(p_near0, p_near1), min(p_near2, p_near3)),
                          min(min(p_far0, p_far1), min(p_far2, p_far3)));
    float3 aabb_max = max(max(max(p_near0, p_near1), max(p_near2, p_near3)),
                          max(max(p_far0, p_far1), max(p_far2, p_far3)));

    // 4. Test lights against the cluster bounding AABB
    for (uint i = 0; i < params.num_lights && visible_count < MAX_LIGHTS_PER_CLUSTER; ++i) {
        LightData light = lights[i];
        if (light.type == 0) {
            // Directional light affects all clusters
            light_index_list[base_offset + visible_count] = i;
            visible_count++;
            continue;
        }

        // Transform light position into view space
        float4 view_pos = mul(float4(light.position, 1.0), params.view_matrix);
        float3 light_center = view_pos.xyz;

        // Closest point on AABB to light sphere center
        float3 closest = clamp(light_center, aabb_min, aabb_max);
        float3 delta = light_center - closest;
        float dist_sq = dot(delta, delta);

        if (dist_sq <= (light.range * light.range)) {
            light_index_list[base_offset + visible_count] = i;
            visible_count++;
        }
    }

    ClusterRecord record;
    record.offset = base_offset;
    record.count  = visible_count;
    cluster_grid[cluster_index] = record;
}
