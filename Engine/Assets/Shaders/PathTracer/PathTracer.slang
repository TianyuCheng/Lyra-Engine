import lyra;
import Common.BSDF;
import Common.Noise;
import Common.Camera;
import Common.Lighting;
import Common.Sampling;
import Common.RayTracing;
import Common.Tonemapping;

// Hardware-accelerated Path Tracer (RayGen, ClosestHit, Miss, Compute)
// Multi-bounce Monte Carlo global illumination with BSDF importance sampling and Next-Event Estimation

public struct PathTracerVertex
{
    public float3 position;
    public float3 normal;
    public float3 color;
    public float3 emission;
};

public struct EmissiveTriangle
{
    public float3 p0;
    public float3 p1;
    public float3 p2;
    public float3 normal;
    public float3 emission;
    public float  area;
};

public struct PathTracerPushConstants
{
    public uint  width;
    public uint  height;
    public uint  sample_index;
    public uint  max_bounces;
    public uint  frame_seed;
    public float exposure;
    public uint  debug_mode;
    public uint  num_emissive_triangles;
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

[[lyra::binding(4, 0)]]
StructuredBuffer<PathTracerVertex> scene_vertices;

[[lyra::binding(5, 0)]]
StructuredBuffer<uint> scene_indices;

[[lyra::binding(6, 0)]]
StructuredBuffer<EmissiveTriangle> emissive_triangles;

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

#if defined(__spirv__) || defined(__dxil__)

[shader("raygeneration")]
void raygen_main()
{
    uint2 pixel = DispatchRaysIndex().xy;
    if (pixel.x >= push.width || pixel.y >= push.height) return;

    float2 uv  = (float2(pixel) + 0.5) / float2(push.width, push.height);
    Ray    ray = generate_camera_ray(uv, camera.inv_view, camera.inv_proj);

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

    float4 color = float4(payload.radiance, 1.0);
    if (push.sample_index == 0) {
        accumulation_buffer[pixel] = color;
        float3 mapped              = tonemap_aces_film(color.rgb * push.exposure);
        output_target[pixel]       = float4(mapped, 1.0);
    } else {
        float4 prev_sum            = accumulation_buffer[pixel];
        float4 new_sum             = prev_sum + color;
        accumulation_buffer[pixel] = new_sum;
        float3 avg_color           = (new_sum.rgb / float(push.sample_index + 1)) * push.exposure;
        float3 mapped              = tonemap_aces_film(avg_color);
        output_target[pixel]       = float4(mapped, 1.0);
    }
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
    uint   prim_id = PrimitiveIndex();

    uint i0 = scene_indices[prim_id * 3 + 0];
    uint i1 = scene_indices[prim_id * 3 + 1];
    uint i2 = scene_indices[prim_id * 3 + 2];
    PathTracerVertex v0 = scene_vertices[i0];
    PathTracerVertex v1 = scene_vertices[i1];
    PathTracerVertex v2 = scene_vertices[i2];

    float2 bary = attribs.barycentrics;
    float3 w    = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
    float3 N    = normalize(v0.normal * w.x + v1.normal * w.y + v2.normal * w.z);
    if (dot(N, V) < 0.0) N = -N;

    float3 albedo   = v0.color * w.x + v1.color * w.y + v2.color * w.z;
    float3 emission = v0.emission * w.x + v1.emission * w.y + v2.emission * w.z;

    if (any(emission > 0.0)) {
        if (payload.bounce_count == 0) {
            payload.radiance += payload.throughput * emission;
        }
        payload.terminated = true;
        return;
    }

    PBRMaterial mat;
    mat.base_color           = albedo;
    mat.metallic             = 0.0;
    mat.roughness            = 0.5;
    mat.reflectance          = 0.5;
    mat.clear_coat.factor    = 0.0;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint      = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    // Next-Event Estimation
    if (push.num_emissive_triangles > 0) {
        float u_pick = rand_f(payload.rng_state);
        uint tri_idx = min(uint(u_pick * float(push.num_emissive_triangles)), push.num_emissive_triangles - 1);
        EmissiveTriangle tri = emissive_triangles[tri_idx];

        float2 u_tri = rand_f2(payload.rng_state);
        float3 bary_light = sample_triangle_uniform(u_tri);
        float3 light_pos = tri.p0 * bary_light.x + tri.p1 * bary_light.y + tri.p2 * bary_light.z;

        float3 light_dir = light_pos - hit_pos;
        float  dist_sq   = dot(light_dir, light_dir);
        float  dist      = sqrt(dist_sq);
        float3 L         = light_dir / dist;

        float NoL       = dot(N, L);
        float cos_light = max(dot(tri.normal, -L), 0.0);

        if (NoL > 0.0 && cos_light > 0.0) {
            float pdf = (dist_sq / (tri.area * cos_light)) * (1.0 / float(push.num_emissive_triangles));
            float3 f = mat.evaluate(V, L, N);
            payload.radiance += payload.throughput * f * (tri.emission / pdf);
        }
    } else {
        float2 u_light = rand_f2(payload.rng_state);
        LightSample ls = sample_directional_sun(SUN_DIRECTION, SUN_ANGULAR_RADIUS, SUN_EMISSION, u_light);
        if (dot(N, ls.direction) > 0.0 && ls.pdf > 0.0) {
            float3 f = mat.evaluate(V, ls.direction, N);
            payload.radiance += payload.throughput * f * (ls.radiance / ls.pdf);
        }
    }

    payload.terminated = true;
}

#endif

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
        ray_desc.TMin      = 0.01;
        ray_desc.TMax      = 10000.0;
        q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, ray_desc);
        q.Proceed();

        if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
            if (bounce == 0 && push.num_emissive_triangles == 0) {
                accumulated_radiance += throughput * evaluate_sky(ray.direction);
            }
            break;
        }

        float hit_t    = q.CommittedRayT();
        uint  prim_id  = q.CommittedPrimitiveIndex();
        float3 hit_pos = ray.origin + ray.direction * hit_t;
        float3 V       = -ray.direction;

        cone = cone.propagate(hit_t);

        // Fetch hit triangle vertices from scene buffers
        uint i0 = scene_indices[prim_id * 3 + 0];
        uint i1 = scene_indices[prim_id * 3 + 1];
        uint i2 = scene_indices[prim_id * 3 + 2];
        PathTracerVertex v0 = scene_vertices[i0];
        PathTracerVertex v1 = scene_vertices[i1];
        PathTracerVertex v2 = scene_vertices[i2];

        float2 bary = q.CommittedTriangleBarycentrics();
        float3 w    = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
        float3 N    = normalize(v0.normal * w.x + v1.normal * w.y + v2.normal * w.z);
        if (dot(N, V) < 0.0) N = -N;

        float3 albedo   = v0.color * w.x + v1.color * w.y + v2.color * w.z;
        float3 emission = v0.emission * w.x + v1.emission * w.y + v2.emission * w.z;

        // Direct hit on an emissive surface
        if (any(emission > 0.0)) {
            if (bounce == 0) {
                accumulated_radiance += throughput * emission;
            }
            break;
        }

        PBRMaterial mat;
        mat.base_color           = albedo;
        mat.metallic             = 0.0;
        mat.roughness            = 0.5;
        mat.reflectance          = 0.5;
        mat.clear_coat.factor    = 0.0;
        mat.clear_coat.roughness = 0.04;
        mat.clear_coat.tint      = float3(0.0);
        mat.clear_coat.thickness = 0.0;

        // =====================================================================
        // Next-Event Estimation (NEE)
        // =====================================================================
        if (push.num_emissive_triangles > 0) {
            float u_pick = rand_f(rng_state);
            uint tri_idx = min(uint(u_pick * float(push.num_emissive_triangles)), push.num_emissive_triangles - 1);
            EmissiveTriangle tri = emissive_triangles[tri_idx];

            float2 u_tri = rand_f2(rng_state);
            float3 bary_light = sample_triangle_uniform(u_tri);
            float3 light_pos = tri.p0 * bary_light.x + tri.p1 * bary_light.y + tri.p2 * bary_light.z;

            float3 light_dir = light_pos - hit_pos;
            float  dist_sq   = dot(light_dir, light_dir);
            float  dist      = sqrt(dist_sq);
            float3 L         = light_dir / dist;

            float NoL       = dot(N, L);
            float cos_light = max(dot(tri.normal, -L), 0.0);

            if (NoL > 0.0 && cos_light > 0.0) {
                RayQuery<RAY_FLAG_NONE> shadow_q;
                RayDesc                 shadow_ray;
                shadow_ray.Origin    = offset_ray_origin(hit_pos, N, L);
                shadow_ray.Direction = L;
                shadow_ray.TMin      = 0.01;
                shadow_ray.TMax      = max(dist - 0.05, 0.01);
                shadow_q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, shadow_ray);
                shadow_q.Proceed();

                if (shadow_q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
                    float  pdf = (dist_sq / (tri.area * cos_light)) * (1.0 / float(push.num_emissive_triangles));
                    float3 f   = mat.evaluate(V, L, N);
                    accumulated_radiance += throughput * f * (tri.emission / pdf);
                }
            }
        } else {
            float2 u_light = rand_f2(rng_state);
            LightSample ls = sample_directional_sun(SUN_DIRECTION, SUN_ANGULAR_RADIUS, SUN_EMISSION, u_light);
            float NoL = dot(N, ls.direction);
            if (NoL > 0.0 && ls.pdf > 0.0) {
                RayQuery<RAY_FLAG_NONE> shadow_q;
                RayDesc                 shadow_ray;
                shadow_ray.Origin    = offset_ray_origin(hit_pos, N, ls.direction);
                shadow_ray.Direction = ls.direction;
                shadow_ray.TMin      = 0.01;
                shadow_ray.TMax      = 10000.0;
                shadow_q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, shadow_ray);
                shadow_q.Proceed();

                if (shadow_q.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
                    float3 f = mat.evaluate(V, ls.direction, N);
                    accumulated_radiance += throughput * f * (ls.radiance / ls.pdf);
                }
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

    if (push.sample_index == 0) {
        accumulation_buffer[pixel] = color;
        float3 mapped              = tonemap_aces_film(color.rgb * push.exposure);
        output_target[pixel]       = float4(mapped, 1.0);
    } else {
        float4 prev_sum            = accumulation_buffer[pixel];
        float4 new_sum             = prev_sum + color;
        accumulation_buffer[pixel] = new_sum;
        float3 avg_color           = (new_sum.rgb / float(push.sample_index + 1)) * push.exposure;
        float3 mapped              = tonemap_aces_film(avg_color);
        output_target[pixel]       = float4(mapped, 1.0);
    }
}
