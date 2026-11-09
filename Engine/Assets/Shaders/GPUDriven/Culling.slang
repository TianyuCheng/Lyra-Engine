import lyra;
import Common.Culling;

struct CameraData
{
    float4x4 view_proj;
    float4   frustum_planes[6];
    float3   camera_pos;
    float    fov_y;
    float2   screen_size;
    float    min_pixel_size;
    uint     enable_occlusion_culling;
};

struct InstanceData
{
    float4x4 model;
    float3   aabb_min;
    float    padding0;
    float3   aabb_max;
    float    bounding_radius;
    uint     draw_command_index;
    uint     instance_index;
    uint2    padding1;
};

struct DrawIndexedIndirectCommand
{
    uint index_count;
    uint instance_count;
    uint first_index;
    int  base_vertex;
    uint first_instance;
};

[[lyra::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[[lyra::binding(1, 0)]]
StructuredBuffer<InstanceData> instances;

[[lyra::binding(2, 0)]]
RWStructuredBuffer<DrawIndexedIndirectCommand> draw_commands;

[[lyra::binding(3, 0)]]
Texture2D<float> hiz_pyramid;

[[lyra::binding(4, 0)]]
SamplerState point_sampler;

[shader("compute")]
[numthreads(64, 1, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint instance_idx = dispatch_thread_id.x;
    uint total_instances, stride;
    instances.GetDimensions(total_instances, stride);

    if (instance_idx >= total_instances) {
        return;
    }

    InstanceData instance = instances[instance_idx];

    // 1. Transform bounding sphere to world space
    float3 center = (instance.aabb_min + instance.aabb_max) * 0.5;
    float3 world_center = mul(instance.model, float4(center, 1.0)).xyz;
    float  world_radius = instance.bounding_radius * max(max(length(instance.model[0].xyz), length(instance.model[1].xyz)), length(instance.model[2].xyz));

    // 2. Frustum Culling
    if (cull_sphere_frustum(camera.frustum_planes, world_center, world_radius)) {
        return;
    }

    // 3. Subpixel Culling
    if (cull_subpixel_size(world_center, world_radius, camera.camera_pos, camera.fov_y, camera.screen_size.y, camera.min_pixel_size)) {
        return;
    }

    // 4. Hi-Z Occlusion Culling
    if (camera.enable_occlusion_culling != 0) {
        float3 world_min = world_center - float3(world_radius);
        float3 world_max = world_center + float3(world_radius);
        float4 screen_rect;
        float  min_depth, max_depth;

        if (project_aabb_to_screen_rect(world_min, world_max, camera.view_proj, screen_rect, min_depth, max_depth)) {
            if (cull_hiz_occlusion(hiz_pyramid, point_sampler, screen_rect, max_depth, camera.screen_size, true)) {
                return; // occluded
            }
        }
    }

    // 5. Increment indirect draw command instance count
    InterlockedAdd(draw_commands[instance.draw_command_index].instance_count, 1);
}
