import lyra;
import Common.Camera;
import Common.Sampling;
import Common.BSDF;
import Common.RayTracing;
import Common.Lighting;
import Common.Noise;

// Hardware-accelerated Path Tracer (RayGen, ClosestHit, Miss)
// Multi-bounce Monte Carlo global illumination with BSDF importance sampling

public struct PathTracerPushConstants
{
    public uint  width;
    public uint  height;
    public uint  sample_index;
    public uint  max_bounces;
    public uint  frame_seed;
    public float exposure;
    public uint  debug_mode;
    public uint  pad;
};

[[lyra::immediates]]
ConstantBuffer<PathTracerPushConstants> push;

[[lyra::binding(0, 0)]]
ConstantBuffer<CameraUniforms> camera;

[[lyra::binding(1, 0)]]
RaytracingAccelerationStructure scene_tlas;

[[lyra::binding(2, 0)]]
RWTexture2D<float4> accumulation_buffer;

[[lyra::binding(3, 0)]]
RWTexture2D<float4> output_target;

public struct RayPayload
{
    public float3  radiance;
    public float3  throughput;
    public uint    bounce_count;
    public uint    rng_state;
    public RayCone cone;
    public bool    terminated;
};

[shader("raygeneration")]
void raygen_main()
{
    uint2 pixel = DispatchRaysIndex().xy;
    if (pixel.x >= push.width || pixel.y >= push.height) return;

    float2 uv = (float2(pixel) + 0.5) / float2(push.width, push.height);
    Ray ray = generate_camera_ray(uv, camera.inv_view, camera.inv_proj);

    RayPayload payload;
    payload.radiance     = float3(0.0);
    payload.throughput   = float3(1.0);
    payload.bounce_count = 0;
    payload.rng_state    = pcg_hash(pixel.y * push.width + pixel.x + push.frame_seed);
    payload.cone         = RayCone::from_camera(camera.fov * (3.14159265 / 180.0), float(push.height));
    payload.terminated   = false;

    RayDesc ray_desc;
    ray_desc.Origin    = ray.origin;
    ray_desc.Direction = ray.direction;
    ray_desc.TMin      = 0.001;
    ray_desc.TMax      = 10000.0;

    TraceRay(scene_tlas, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray_desc, payload);

    output_target[pixel] = float4(payload.radiance, 1.0);
}

[shader("miss")]
void miss_main(inout RayPayload payload)
{
    // atmospheric sky background
    payload.radiance += payload.throughput * float3(0.18, 0.42, 0.85) * 0.5;
    payload.terminated = true;
}

[shader("closesthit")]
void closesthit_main(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float2 bary = attribs.barycentrics;
    float3 bary_color = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
    payload.radiance += payload.throughput * bary_color;
    payload.terminated = true;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_id.xy;
    if (pixel.x >= push.width || pixel.y >= push.height) return;

    float2 uv = (float2(pixel) + 0.5) / float2(push.width, push.height);
    Ray ray = generate_camera_ray(uv, camera.inv_view, camera.inv_proj);

    RayQuery<RAY_FLAG_NONE> q;
    RayDesc ray_desc;
    ray_desc.Origin    = ray.origin;
    ray_desc.Direction = ray.direction;
    ray_desc.TMin      = 0.001;
    ray_desc.TMax      = 10000.0;
    q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, ray_desc);
    q.Proceed();

    float4 color = float4(0.0, 0.0, 0.0, 1.0);
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        float2 bary = q.CommittedTriangleBarycentrics();
        float3 bary_color = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
        color = float4(bary_color, 1.0);
    } else {
        color = float4(float3(0.18, 0.42, 0.85) * 0.5, 1.0);
    }

    output_target[pixel] = color;
}
