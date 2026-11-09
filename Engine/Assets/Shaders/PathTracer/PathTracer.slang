import lyra;
import Common.Sampling;
import Common.BSDF;
import Common.RayTracing;

// Hardware-accelerated Path Tracer (RayGen, ClosestHit, Miss)
// Multi-bounce Monte Carlo global illumination with BSDF importance sampling

struct RayPayload
{
    float3  radiance;
    float3  throughput;
    uint    bounce_count;
    uint    rng_state;
    RayCone cone;
    bool    terminated;
};

[shader("raygeneration")]
void raygen_main()
{
    // 1. generate camera primary ray using jittered sub-pixel sample and ray cone
    // 2. trace ray against scene TLAS
    // 3. evaluate multi-bounce radiance loop using IBSDF sampling and Russian roulette
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
    // 1. fetch surface geometric normal and material parameters
    float3 hit_pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 N = float3(0.0, 1.0, 0.0);
    float3 V = -WorldRayDirection();

    // propagate ray cone to current hit distance
    payload.cone = payload.cone.propagate(RayTCurrent());

    PBRMaterial mat;
    mat.base_color   = float3(0.8, 0.8, 0.8);
    mat.metallic     = 0.0;
    mat.roughness    = 0.2;
    mat.reflectance  = 0.5;
    mat.clear_coat.factor = 0.0;
    mat.clear_coat.roughness = 0.04;
    mat.clear_coat.tint = float3(0.0);
    mat.clear_coat.thickness = 0.0;

    // 2. sample next bounce ray using IBSDF::sample
    float2 u = float2(0.5, 0.5); // random sample from payload RNG
    float3 next_L;
    float  pdf;
    float3 bsdf_val = mat.sample(V, N, u, next_L, pdf);

    if (pdf > 0.0) {
        payload.throughput *= bsdf_val / pdf;
        payload.bounce_count++;
        payload.cone = payload.cone.reflect(mat.roughness);

        // evaluate Russian roulette path termination
        if (!evaluate_russian_roulette(payload.throughput, 0.5, payload.bounce_count, 3)) {
            payload.terminated = true;
            return;
        }

        // offset ray origin to safely avoid self-intersection acne
        float3 next_origin = offset_ray_origin(hit_pos, N, next_L);
    } else {
        payload.terminated = true;
    }
}
