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
    uint3    cluster_grid_dim;
    uint     num_lights;
};

struct ClusterRecord
{
    uint offset;
    uint count;
};

[[lyra::immediates]]
ConstantBuffer<LightCullingParams> params;

[[lyra::binding(0, 0)]]
StructuredBuffer<LightData> lights;

[[lyra::binding(1, 0)]]
RWStructuredBuffer<ClusterRecord> cluster_grid;

[[lyra::binding(2, 0)]]
RWStructuredBuffer<uint> light_index_list;

[shader("compute")]
[numthreads(8, 8, 1)]
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

    static const uint MAX_LIGHTS_PER_CLUSTER = 64;
    uint base_offset = cluster_index * MAX_LIGHTS_PER_CLUSTER;
    uint visible_count = 0;

    float z_near = params.near_plane * pow(abs(params.far_plane / max(params.near_plane, 0.001)), float(cluster_coord.z) / float(params.cluster_grid_dim.z));
    float z_far  = params.near_plane * pow(abs(params.far_plane / max(params.near_plane, 0.001)), float(cluster_coord.z + 1) / float(params.cluster_grid_dim.z));

    for (uint i = 0; i < params.num_lights && visible_count < MAX_LIGHTS_PER_CLUSTER; ++i) {
        LightData light = lights[i];
        if (light.type == 0) {
            light_index_list[base_offset + visible_count] = i;
            visible_count++;
            continue;
        }

        float4 view_pos = mul(params.view_matrix, float4(light.position, 1.0));
        float light_z   = view_pos.z;

        if ((light_z + light.range) >= z_near && (light_z - light.range) <= z_far) {
            light_index_list[base_offset + visible_count] = i;
            visible_count++;
        }
    }

    ClusterRecord record;
    record.offset = base_offset;
    record.count  = visible_count;
    cluster_grid[cluster_index] = record;
}
