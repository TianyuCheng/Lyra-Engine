module RayTracing;

import Common.Sampling;

static const float PI         = 3.14159265358979323846;
static const float TWO_PI     = 6.28318530717958647692;
static const float INV_PI     = 0.31830988618379067154;
static const float INV_4PI    = 0.07957747154594766788;
static const float EPSILON    = 1e-6;

// =============================================================================
// 1. Core Ray & Ray Cone Data Structures
// =============================================================================

/**
 * @brief Fundamental 3D parametric ray representation.
 */
public struct Ray
{
    public float3 origin;
    public float  t_min;
    public float3 direction;
    public float  t_max;

    public float3 at(float t)
    {
        return origin + direction * t;
    }
};

/**
 * @brief Ray Cone for texture LOD selection without screen-space derivatives (Akenine-Möller et al., RTG II).
 */
public struct RayCone
{
    public float width;        // cross-sectional cone radius at ray origin
    public float spread_angle; // half-angle divergence of the cone in radians

    public static RayCone from_camera(float fov_y_radians, float screen_height)
    {
        RayCone cone;
        cone.width = 0.0;
        cone.spread_angle = (2.0 * tan(0.5 * fov_y_radians)) / max(screen_height, 1.0);
        return cone;
    }

    public RayCone propagate(float distance)
    {
        RayCone next;
        next.width = width + spread_angle * distance;
        next.spread_angle = spread_angle;
        return next;
    }

    public RayCone reflect(float roughness, float surface_curvature = 0.0)
    {
        RayCone next;
        next.width = width;
        // roughness broadens the cone spread; curvature further expands divergence
        next.spread_angle = spread_angle + (2.0 * roughness * roughness) + (2.0 * surface_curvature * width);
        return next;
    }

    public float compute_lod(float texture_width, float texture_height, float triangle_area, float uv_area)
    {
        float footprint = width * width;
        float texel_density = (texture_width * texture_height * uv_area) / max(triangle_area, 1e-6);
        return 0.5 * log2(max(footprint * texel_density, 1e-6));
    }
};

// =============================================================================
// 2. Robust Ray Origin Offsetting (Wächter & Binder, Ray Tracing Gems Ch. 6)
// =============================================================================

static const float ORIGIN_OFFSET = 1.0 / 32.0;
static const float FLOAT_SCALE   = 1.0 / 65536.0;
static const float INT_SCALE     = 256.0;

/**
 * @brief Offsets a ray origin along the surface geometric normal to prevent self-intersection.
 * Implements Wächter & Binder bit-manipulation integer scaling.
 */
public float3 offset_ray(float3 p, float3 n)
{
    int3 of_i = int3(int(n.x * INT_SCALE), int(n.y * INT_SCALE), int(n.z * INT_SCALE));

    float3 p_i = float3(
        asfloat(asint(p.x) + ((p.x < 0.0) ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + ((p.y < 0.0) ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + ((p.z < 0.0) ? -of_i.z : of_i.z))
    );

    return float3(
        abs(p.x) < ORIGIN_OFFSET ? p.x + FLOAT_SCALE * n.x : p_i.x,
        abs(p.y) < ORIGIN_OFFSET ? p.y + FLOAT_SCALE * n.y : p_i.y,
        abs(p.z) < ORIGIN_OFFSET ? p.z + FLOAT_SCALE * n.z : p_i.z
    );
}

/**
 * @brief Offsets ray origin according to ray direction (forward for reflection, backward for transmission).
 */
public float3 offset_ray_origin(float3 p, float3 n, float3 dir)
{
    return dot(n, dir) >= 0.0 ? offset_ray(p, n) : offset_ray(p, -n);
}

// =============================================================================
// 3. Multiple Importance Sampling (MIS) & Russian Roulette
// =============================================================================

/**
 * @brief Balance heuristic for combining estimators (Veach 1997).
 */
public float balance_heuristic(float pdf_f, float pdf_g)
{
    return pdf_f / max(pdf_f + pdf_g, EPSILON);
}

/**
 * @brief Power heuristic (beta = 2.0) with optimal variance reduction.
 */
public float power_heuristic(float pdf_f, float pdf_g, float beta = 2.0)
{
    float f2 = pow(pdf_f, beta);
    float g2 = pow(pdf_g, beta);
    return f2 / max(f2 + g2, EPSILON);
}

/**
 * @brief Adaptive Russian Roulette path termination based on perceptual luminance.
 * @param throughput Cumulative path throughput (modified in-place if path continues)
 * @param u Random sample [0, 1)
 * @param bounce Current bounce index
 * @param min_bounces Minimum bounces to trace before termination checks
 * @param max_continuation_prob Maximum survival probability clamp (default: 0.95)
 * @return True if the path survives, false if terminated
 */
public bool evaluate_russian_roulette(
    inout float3 throughput,
    float        u,
    uint         bounce,
    uint         min_bounces = 3,
    float        max_continuation_prob = 0.95)
{
    if (bounce < min_bounces) {
        return true;
    }

    // ITU-R BT.709 perceptual luminance
    float lum = dot(throughput, float3(0.2126, 0.7152, 0.0722));
    float survival_prob = clamp(lum, 0.05, max_continuation_prob);

    if (u > survival_prob) {
        throughput = float3(0.0);
        return false;
    }

    // unbias estimator
    throughput /= survival_prob;
    return true;
}

// =============================================================================
// 4. Camera & Lens Ray Utilities
// =============================================================================

/**
 * @brief Generates a standard pinhole camera ray from normalized screen UV coordinates.
 */
public Ray generate_camera_ray(
    float2   screen_uv,
    float4x4 inv_view,
    float4x4 inv_proj)
{
    float2 ndc = screen_uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 target = mul(inv_proj, float4(ndc, 1.0, 1.0));
    float3 ray_dir_view = normalize(target.xyz / target.w);
    float3 ray_dir_world = normalize(mul((float3x3)inv_view, ray_dir_view));
    float3 ray_origin_world = inv_view[3].xyz;

    Ray ray;
    ray.origin    = ray_origin_world;
    ray.t_min     = 1e-4;
    ray.direction = ray_dir_world;
    ray.t_max     = 1e7;
    return ray;
}

/**
 * @brief Generates a thin-lens camera ray supporting depth of field with circular aperture bokeh.
 */
public Ray generate_thin_lens_ray(
    float2   screen_uv,
    float2   lens_sample,
    float    focal_distance,
    float    lens_radius,
    float4x4 inv_view,
    float4x4 inv_proj)
{
    Ray pinhole = generate_camera_ray(screen_uv, inv_view, inv_proj);
    if (lens_radius <= 0.0) {
        return pinhole;
    }

    float2 lens_offset = sample_disk_concentric(lens_sample) * lens_radius;
    float3 lens_point_world = pinhole.origin + mul((float3x3)inv_view, float3(lens_offset, 0.0));

    float ft = focal_distance / max(abs(dot(pinhole.direction, mul((float3x3)inv_view, float3(0.0, 0.0, -1.0)))), 1e-5);
    float3 focal_point = pinhole.origin + pinhole.direction * ft;

    Ray ray;
    ray.origin    = lens_point_world;
    ray.t_min     = 1e-4;
    ray.direction = normalize(focal_point - lens_point_world);
    ray.t_max     = 1e7;
    return ray;
}

// =============================================================================
// 5. Next Event Estimation (NEE) Light Samplers
// =============================================================================

public struct LightSample
{
    public float3 direction; // Normalized direction from shading point to light
    public float  distance;  // Distance from shading point to light emitter
    public float3 radiance;  // Emitted radiance reaching the surface point
    public float  pdf;       // Probability density with respect to solid angle
};

/**
 * @brief Exact solid-angle subtended cone sampling of a spherical light emitter.
 */
public LightSample sample_sphere_light(
    float3 p,
    float3 center,
    float  radius,
    float3 emission,
    float2 u)
{
    LightSample ls;
    float3 to_center = center - p;
    float dist_sq = dot(to_center, to_center);
    float dist = sqrt(dist_sq);

    if (dist <= radius) {
        // Shading point is inside the sphere
        ls.direction = float3(0.0, 1.0, 0.0);
        ls.distance  = 0.0;
        ls.radiance  = emission;
        ls.pdf       = 1.0;
        return ls;
    }

    // Cone half-angle subtended by sphere from shading point
    float sin_theta_max_sq = (radius * radius) / dist_sq;
    float cos_theta_max = sqrt(max(0.0, 1.0 - sin_theta_max_sq));

    float3 local_dir = sample_cone_uniform(u, cos_theta_max);
    ls.direction = tangent_to_world(local_dir, to_center / dist);
    ls.pdf = pdf_cone_uniform(cos_theta_max);

    // Compute ray-sphere intersection distance
    float b = dot(to_center, ls.direction);
    float c = dist_sq - radius * radius;
    float d = max(0.0, b * b - c);
    ls.distance = b - sqrt(d);
    ls.radiance = emission;
    return ls;
}

/**
 * @brief Uniform area sampling of an oriented disk area light.
 */
public LightSample sample_disk_light(
    float3 p,
    float3 disk_center,
    float3 disk_normal,
    float  radius,
    float3 emission,
    float2 u)
{
    LightSample ls;
    float2 disk_uv = sample_disk_concentric(u) * radius;
    float3 t, b;
    build_orthonormal_basis(disk_normal, t, b);
    float3 light_pt = disk_center + t * disk_uv.x + b * disk_uv.y;

    float3 to_light = light_pt - p;
    float dist_sq = dot(to_light, to_light);
    ls.distance = sqrt(dist_sq);
    ls.direction = to_light / max(ls.distance, EPSILON);

    float cos_light = max(dot(-ls.direction, disk_normal), 0.0);
    float area = PI * radius * radius;
    float area_pdf = 1.0 / max(area, EPSILON);

    // Convert area PDF to solid angle PDF: pdf_omega = pdf_area * dist^2 / cos_theta
    ls.pdf = (cos_light > 0.0) ? (area_pdf * dist_sq / cos_light) : 0.0;
    ls.radiance = cos_light > 0.0 ? emission : float3(0.0);
    return ls;
}

/**
 * @brief Uniform area sampling of a rectangular quad area light.
 */
public LightSample sample_quad_light(
    float3 p,
    float3 corner,
    float3 edge_x,
    float3 edge_y,
    float3 emission,
    float2 u)
{
    LightSample ls;
    float3 light_pt = corner + edge_x * u.x + edge_y * u.y;
    float3 quad_normal = normalize(cross(edge_x, edge_y));
    float area = length(cross(edge_x, edge_y));

    float3 to_light = light_pt - p;
    float dist_sq = dot(to_light, to_light);
    ls.distance = sqrt(dist_sq);
    ls.direction = to_light / max(ls.distance, EPSILON);

    float cos_light = max(dot(-ls.direction, quad_normal), 0.0);
    float area_pdf = 1.0 / max(area, EPSILON);

    ls.pdf = (cos_light > 0.0) ? (area_pdf * dist_sq / cos_light) : 0.0;
    ls.radiance = cos_light > 0.0 ? emission : float3(0.0);
    return ls;
}

/**
 * @brief Directional sun sampling with realistic angular radius cone (~0.53 degrees).
 */
public LightSample sample_directional_sun(
    float3 sun_dir,
    float  sun_angular_radius,
    float3 sun_emission,
    float2 u)
{
    LightSample ls;
    float cos_theta_max = cos(sun_angular_radius);
    float3 local_dir = sample_cone_uniform(u, cos_theta_max);

    ls.direction = tangent_to_world(local_dir, sun_dir);
    ls.distance  = 1e7;
    ls.pdf       = pdf_cone_uniform(cos_theta_max);
    ls.radiance  = sun_emission;
    return ls;
}

// =============================================================================
// 6. Analytical Geometric Ray Intersectors
// =============================================================================

/**
 * @brief Branchless Kay-Kajiya slab test for Ray-AABB intersection (IEEE-754 robust).
 */
public bool intersect_ray_aabb(Ray ray, float3 box_min, float3 box_max, out float t_near, out float t_far)
{
    float3 inv_d = 1.0 / ray.direction;
    float3 t0 = (box_min - ray.origin) * inv_d;
    float3 t1 = (box_max - ray.origin) * inv_d;

    float3 t_min_v = min(t0, t1);
    float3 t_max_v = max(t0, t1);

    t_near = max(max(t_min_v.x, t_min_v.y), max(t_min_v.z, ray.t_min));
    t_far  = min(min(t_max_v.x, t_max_v.y), min(t_max_v.z, ray.t_max));

    return t_near <= t_far;
}

/**
 * @brief Analytical Ray-Sphere intersection with closest hit and outward surface normal.
 */
public bool intersect_ray_sphere(Ray ray, float3 center, float radius, out float t_hit, out float3 hit_normal)
{
    float3 oc = ray.origin - center;
    float b = dot(oc, ray.direction);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;

    if (disc < 0.0) {
        t_hit = ray.t_max;
        hit_normal = float3(0.0);
        return false;
    }

    float sqrt_disc = sqrt(disc);
    float t0 = -b - sqrt_disc;
    float t1 = -b + sqrt_disc;

    t_hit = (t0 >= ray.t_min) ? t0 : t1;
    if (t_hit < ray.t_min || t_hit > ray.t_max) {
        hit_normal = float3(0.0);
        return false;
    }

    hit_normal = normalize((ray.origin + ray.direction * t_hit) - center);
    return true;
}

/**
 * @brief Ray-Plane intersection.
 */
public bool intersect_ray_plane(Ray ray, float3 plane_point, float3 plane_normal, out float t_hit)
{
    float denom = dot(ray.direction, plane_normal);
    if (abs(denom) < EPSILON) {
        t_hit = ray.t_max;
        return false;
    }

    t_hit = dot(plane_point - ray.origin, plane_normal) / denom;
    return t_hit >= ray.t_min && t_hit <= ray.t_max;
}

/**
 * @brief Ray-Disk intersection.
 */
public bool intersect_ray_disk(Ray ray, float3 disk_center, float3 disk_normal, float radius, out float t_hit)
{
    if (!intersect_ray_plane(ray, disk_center, disk_normal, t_hit)) {
        return false;
    }

    float3 hit_point = ray.origin + ray.direction * t_hit;
    return dot(hit_point - disk_center, hit_point - disk_center) <= radius * radius;
}

/**
 * @brief Analytical Ray-Capsule intersection for hair, curve, and cable geometry.
 */
public bool intersect_ray_capsule(Ray ray, float3 pa, float3 pb, float radius, out float t_hit, out float3 hit_normal)
{
    float3 ba = pb - pa;
    float3 oa = ray.origin - pa;
    float baba = dot(ba, ba);
    float bard = dot(ba, ray.direction);
    float baoa = dot(ba, oa);
    float rdoa = dot(ray.direction, oa);
    float oaoa = dot(oa, oa);

    float a = baba - bard * bard;
    float b = baba * rdoa - baoa * bard;
    float c = baba * oaoa - baoa * baoa - radius * radius * baba;
    float h = b * b - a * c;

    if (h >= 0.0) {
        float t = (-b - sqrt(h)) / a;
        float y = baoa + t * bard;
        if (y > 0.0 && y < baba && t >= ray.t_min && t <= ray.t_max) {
            t_hit = t;
            float3 p = ray.origin + ray.direction * t;
            hit_normal = normalize(p - (pa + ba * (y / baba)));
            return true;
        }

        // Cap spheres
        float3 oc = (y <= 0.0) ? oa : ray.origin - pb;
        b = dot(ray.direction, oc);
        c = dot(oc, oc) - radius * radius;
        h = b * b - c;
        if (h >= 0.0) {
            t = -b - sqrt(h);
            if (t >= ray.t_min && t <= ray.t_max) {
                t_hit = t;
                float3 p = ray.origin + ray.direction * t;
                hit_normal = normalize(p - ((y <= 0.0) ? pa : pb));
                return true;
            }
        }
    }

    t_hit = ray.t_max;
    hit_normal = float3(0.0);
    return false;
}

// =============================================================================
// 7. Volumetric & Participating Media Utilities
// =============================================================================

/**
 * @brief Beer-Lambert law exponential attenuation through homogeneous absorbing/scattering medium.
 */
public float3 beer_lambert(float3 extinction_coefficient, float distance)
{
    return exp(-extinction_coefficient * distance);
}

/**
 * @brief Henyey-Greenstein phase function for anisotropic volumetric scattering.
 * @param cos_theta Cosine of angle between incident and scattered ray
 * @param g Asymmetry parameter in (-1, 1). g > 0 forward, g < 0 backward, g = 0 isotropic.
 */
public float henyey_greenstein_phase(float cos_theta, float g)
{
    float denom = 1.0 + g * g - 2.0 * g * cos_theta;
    return INV_4PI * (1.0 - g * g) / max(denom * sqrt(max(denom, 0.0)), EPSILON);
}

/**
 * @brief Importance samples a scattered direction according to the Henyey-Greenstein phase function.
 */
public float3 sample_henyey_greenstein(float3 incident_dir, float g, float2 u)
{
    float cos_theta;
    if (abs(g) < 1e-3) {
        cos_theta = 1.0 - 2.0 * u.x;
    } else {
        float sqr = (1.0 - g * g) / (1.0 - g + 2.0 * g * u.x);
        cos_theta = (1.0 + g * g - sqr * sqr) / (2.0 * g);
    }

    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    float phi = TWO_PI * u.y;
    float3 local_dir = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    return tangent_to_world(local_dir, incident_dir);
}

/**
 * @brief Schlick's fast rational approximation to Henyey-Greenstein.
 */
public float schlick_phase(float cos_theta, float k)
{
    float d = 1.0 + k * cos_theta;
    return INV_4PI * (1.0 - k * k) / max(d * d, EPSILON);
}

// =============================================================================
// 8. Compact Ray Payload Encodings (Octahedral & RGBE)
// =============================================================================

/**
 * @brief Octahedral normal encoding to float2 in [-1, 1]^2 (Meyer et al. 2010).
 */
public float2 encode_octahedral(float3 n)
{
    float l1 = abs(n.x) + abs(n.y) + abs(n.z);
    float2 p = n.xy * (1.0 / max(l1, EPSILON));
    if (n.z < 0.0) {
        p = (1.0 - abs(p.yx)) * float2(p.x >= 0.0 ? 1.0 : -1.0, p.y >= 0.0 ? 1.0 : -1.0);
    }
    return p;
}

/**
 * @brief Decodes an octahedral encoded normal vector back to a unit float3.
 */
public float3 decode_octahedral(float2 e)
{
    float3 v = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
    }
    return normalize(v);
}

/**
 * @brief Packs unit normal into 32-bit integer (2x16 unorm snorm).
 */
public uint pack_normal_2x16(float3 n)
{
    float2 oct = encode_octahedral(n) * 0.5 + 0.5;
    uint2 quant = uint2(clamp(oct * 65535.0 + 0.5, 0.0, 65535.0));
    return (quant.x & 0xFFFF) | ((quant.y & 0xFFFF) << 16);
}

/**
 * @brief Unpacks 32-bit packed normal back into unit float3.
 */
public float3 unpack_normal_2x16(uint packed)
{
    float2 oct = float2(packed & 0xFFFF, (packed >> 16) & 0xFFFF) / 65535.0;
    return decode_octahedral(oct * 2.0 - 1.0);
}

/**
 * @brief Packs HDR RGB radiance into 32-bit Greg Ward RGBE shared-exponent format.
 */
public uint pack_radiance_rgbe(float3 rgb)
{
    float max_val = max(max(rgb.r, rgb.g), rgb.b);
    if (max_val < 1e-32) {
        return 0;
    }

    int exp_val;
    float mantissa = frexp(max_val, exp_val);
    float scale = (mantissa * 256.0 / max_val);

    uint r = uint(clamp(rgb.r * scale, 0.0, 255.0));
    uint g = uint(clamp(rgb.g * scale, 0.0, 255.0));
    uint b = uint(clamp(rgb.b * scale, 0.0, 255.0));
    uint e = uint(clamp(exp_val + 128, 0, 255));

    return r | (g << 8) | (b << 16) | (e << 24);
}

/**
 * @brief Unpacks 32-bit RGBE shared-exponent format back into high dynamic range float3.
 */
public float3 unpack_radiance_rgbe(uint rgbe)
{
    if (rgbe == 0) {
        return float3(0.0);
    }

    int exponent = int((rgbe >> 24) & 0xFF) - 128 - 8;
    float scale = ldexp(1.0, exponent);

    return float3(
        float(rgbe & 0xFF),
        float((rgbe >> 8) & 0xFF),
        float((rgbe >> 16) & 0xFF)
    ) * scale;
}
