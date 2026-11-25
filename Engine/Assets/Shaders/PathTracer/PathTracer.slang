import lyra;
import Common.Camera;
import Common.Sampling;
import Common.BSDF;
import Common.RayTracing;
import Common.Lighting;
import Common.Noise;

// Hardware-accelerated Path Tracer (RayGen, ClosestHit, Miss, Compute)
// Multi-bounce Monte Carlo global illumination with BSDF importance sampling and Next-Event Estimation

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

static const float3 SUN_DIRECTION      = normalize(float3(0.4, 0.8, 0.3));
static const float3 SUN_EMISSION       = float3(12.0, 11.5, 10.0);
static const float  SUN_ANGULAR_RADIUS = 0.00925; // ~0.53 degrees

// Random number generator helpers
uint step_rng(inout uint state)
{
    state = pcg_hash(state);
    return state;
}

float rand_f(inout uint state)
{
    return float(step_rng(state)) * 2.3283064365386963e-10;
}

float2 rand_f2(inout uint state)
{
    return float2(rand_f(state), rand_f(state));
}

float3 evaluate_sky(float3 direction)
{
    float t = 0.5 * (direction.y + 1.0);
    float3 horizon = float3(0.7, 0.8, 0.95);
    float3 zenith  = float3(0.18, 0.42, 0.85);
    return lerp(horizon, zenith, clamp(t, 0.0, 1.0)) * 0.8;
}

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
    payload.radiance += payload.throughput * evaluate_sky(WorldRayDirection());
    payload.terminated = true;
}

[shader("closesthit")]
void closesthit_main(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 hit_pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 V       = -WorldRayDirection();
    float3 N       = float3(0.0, 1.0, 0.0);

    if (abs(hit_pos.y) > abs(hit_pos.x) && abs(hit_pos.y) > abs(hit_pos.z)) {
        N = float3(0.0, sign(hit_pos.y), 0.0);
    } else if (abs(hit_pos.x) > abs(hit_pos.z)) {
        N = float3(sign(hit_pos.x), 0.0, 0.0);
    } else {
        N = float3(0.0, 0.0, sign(hit_pos.z));
    }
    if (dot(N, V) < 0.0) N = -N;

    PBRMaterial mat;
    mat.base_color           = float3(0.85, 0.85, 0.85);
    mat.metallic             = 0.0;
    mat.roughness            = 0.4;
    mat.reflectance          = 0.5;
    mat.clear_coat.factor    = 0.0;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint      = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    // Next-Event Estimation
    float2 u_light = rand_f2(payload.rng_state);
    LightSample ls = sample_directional_sun(SUN_DIRECTION, SUN_ANGULAR_RADIUS, SUN_EMISSION, u_light);
    if (dot(N, ls.direction) > 0.0 && ls.pdf > 0.0) {
        float3 f = mat.evaluate(V, ls.direction, N);
        payload.radiance += payload.throughput * f * (ls.radiance / ls.pdf);
    }

    payload.terminated = true;
}

float4 trace_path(uint2 pixel, inout uint rng_state)
{
    float2 jitter      = rand_f2(rng_state) - 0.5;
    float2 pixel_coord = float2(pixel) + 0.5 + jitter;
    float2 uv          = pixel_coord / float2(push.width, push.height);

    Ray ray   = generate_camera_ray(uv, camera.inv_view, camera.inv_proj);
    RayCone cone = RayCone::from_camera(camera.fov * (3.14159265 / 180.0), float(push.height));

    float3 accumulated_radiance = float3(0.0);
    float3 throughput           = float3(1.0);

    for (uint bounce = 0; bounce < push.max_bounces; ++bounce) {
        RayQuery<RAY_FLAG_NONE> q;
        RayDesc ray_desc;
        ray_desc.Origin    = ray.origin;
        ray_desc.Direction = ray.direction;
        ray_desc.TMin      = 0.001;
        ray_desc.TMax      = 10000.0;
        q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, ray_desc);
        q.Proceed();

        if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
            float3 sky = evaluate_sky(ray.direction);
            accumulated_radiance += throughput * sky;
            break;
        }

        float hit_t    = q.CommittedRayT();
        float3 hit_pos = ray.origin + ray.direction * hit_t;
        float3 V       = -ray.direction;

        cone = cone.propagate(hit_t);

        // Estimate geometric normal
        float3 N = float3(0.0, 1.0, 0.0);
        if (abs(hit_pos.y) > abs(hit_pos.x) && abs(hit_pos.y) > abs(hit_pos.z)) {
            N = float3(0.0, sign(hit_pos.y), 0.0);
        } else if (abs(hit_pos.x) > abs(hit_pos.z)) {
            N = float3(sign(hit_pos.x), 0.0, 0.0);
        } else {
            N = float3(0.0, 0.0, sign(hit_pos.z));
        }
        if (dot(N, V) < 0.0) N = -N;

        // PBR material properties based on normal orientation
        PBRMaterial mat;
        mat.clear_coat.factor    = 0.0;
        mat.clear_coat.roughness = 0.04;
        mat.clear_coat.tint      = float3(0.0);
        mat.clear_coat.thickness = 0.0;
        mat.reflectance          = 0.5;

        if (N.x > 0.6) {
            // Left wall: red
            mat.base_color = float3(0.85, 0.12, 0.12);
            mat.metallic   = 0.0;
            mat.roughness  = 0.5;
        } else if (N.x < -0.6) {
            // Right wall: green
            mat.base_color = float3(0.12, 0.85, 0.12);
            mat.metallic   = 0.0;
            mat.roughness  = 0.5;
        } else if (N.y > 0.6) {
            // Floor: neutral gray
            mat.base_color = float3(0.75, 0.75, 0.75);
            mat.metallic   = 0.0;
            mat.roughness  = 0.4;
        } else if (N.y < -0.6) {
            // Ceiling
            mat.base_color = float3(0.85, 0.85, 0.85);
            mat.metallic   = 0.0;
            mat.roughness  = 0.6;
        } else {
            // Back wall / object
            mat.base_color = float3(0.9, 0.9, 0.9);
            mat.metallic   = 0.1;
            mat.roughness  = 0.2;
        }

        // =====================================================================
        // Next-Event Estimation (NEE)
        // =====================================================================
        float2 u_light = rand_f2(rng_state);
        LightSample ls = sample_directional_sun(SUN_DIRECTION, SUN_ANGULAR_RADIUS, SUN_EMISSION, u_light);

        float NoL = dot(N, ls.direction);
        if (NoL > 0.0 && ls.pdf > 0.0) {
            RayQuery<RAY_FLAG_NONE> shadow_q;
            RayDesc shadow_ray;
            shadow_ray.Origin    = offset_ray_origin(hit_pos, N, ls.direction);
            shadow_ray.Direction = ls.direction;
            shadow_ray.TMin      = 0.001;
            shadow_ray.TMax      = 10000.0;
            shadow_q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, shadow_ray);
            shadow_q.Proceed();

            if (shadow_q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
                float3 f         = mat.evaluate(V, ls.direction, N);
                float  bsdf_pdf  = mat.pdf(V, ls.direction, N);
                float  mis_weight = power_heuristic(ls.pdf, bsdf_pdf);
                accumulated_radiance += throughput * f * (ls.radiance / ls.pdf) * mis_weight;
            }
        }

        // =====================================================================
        // BSDF Importance Sampling
        // =====================================================================
        float2 u_bsdf = rand_f2(rng_state);
        float3 next_L;
        float  bsdf_pdf;
        float3 bsdf_val = mat.sample(V, N, u_bsdf, next_L, bsdf_pdf);

        if (bsdf_pdf <= 0.0 || dot(bsdf_val, bsdf_val) <= 0.0) {
            break;
        }

        throughput *= bsdf_val / bsdf_pdf;
        cone = cone.reflect(mat.roughness);

        // =====================================================================
        // Russian Roulette
        // =====================================================================
        if (bounce >= 2) {
            float u_rr = rand_f(rng_state);
            if (!evaluate_russian_roulette(throughput, u_rr, bounce, 3)) {
                break;
            }
        }

        ray.origin    = offset_ray_origin(hit_pos, N, next_L);
        ray.direction = next_L;
    }

    return float4(accumulated_radiance, 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_id.xy;
    if (pixel.x >= push.width || pixel.y >= push.height) return;

    uint rng_state = pcg_hash(pixel.y * push.width + pixel.x + push.frame_seed * 1999u + push.sample_index);

    float4 color = trace_path(pixel, rng_state);

    if (push.debug_mode == 1) {
        Ray ray = generate_camera_ray((float2(pixel) + 0.5) / float2(push.width, push.height), camera.inv_view, camera.inv_proj);
        RayQuery<RAY_FLAG_NONE> q;
        RayDesc ray_desc;
        ray_desc.Origin    = ray.origin;
        ray_desc.Direction = ray.direction;
        ray_desc.TMin      = 0.001;
        ray_desc.TMax      = 10000.0;
        q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, ray_desc);
        q.Proceed();
        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            float2 bary = q.CommittedTriangleBarycentrics();
            color       = float4(1.0 - bary.x - bary.y, bary.x, bary.y, 1.0);
        }
    }

    output_target[pixel] = color;
}
