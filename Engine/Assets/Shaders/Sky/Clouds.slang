module Clouds;

// references: Schneider 2015 (Horizon Zero Dawn volumetric cloudscapes),
//             Hillaire 2016 (Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite),
//             Wrenninge 2013 (Production Volume Rendering),
//             Bouthors et al. 2008 (Multiple scattering in clouds)

import Common.Noise;
import Sky.Atmosphere;

// =============================================================================
// 0. Constants & Configuration Structures
// =============================================================================

static const float CLOUD_PI = 3.14159265358979323846;
static const float INV_4PI  = 0.07957747154594766788; // 1.0 / (4.0 * PI)

/**
 * @brief Volumetric cloud simulation parameters.
 * Units can be configured in kilometers (default, matching Atmosphere.slang)
 * or in world meters.
 */
public struct CloudParameters
{
    // Geometry & Planet Shell
    public float  planet_radius;          // planetary radius (e.g. 6360.0 km or 6360000.0 m)
    public float  cloud_bottom_altitude;  // inner boundary altitude (e.g. 1.5 km or 1500.0 m)
    public float  cloud_top_altitude;     // outer boundary altitude (e.g. 4.0 km or 4000.0 m)
    public float  max_march_distance;     // maximum raymarch distance (e.g. 50.0 km or 50000.0 m)

    // Density & Shape Modeling
    public float  cloud_coverage;         // global coverage threshold in [0, 1] (e.g. 0.45)
    public float  cloud_density_scale;    // overall density multiplier (e.g. 0.02)
    public float  cloud_type;             // 0.0 = Stratus, 0.5 = Stratocumulus, 1.0 = Cumulus
    public float  shape_scale;            // base low-frequency noise frequency (e.g. 0.0004)
    public float  detail_scale;           // high-frequency erosion frequency (e.g. 0.004)
    public float  detail_strength;        // detail erosion influence weight in [0, 1] (e.g. 0.35)
    public float  curl_strength;          // turbulence / curl distortion weight (e.g. 0.15)

    // Dynamics & Wind
    public float3 wind_direction;         // normalized horizontal wind direction
    public float  wind_speed;             // wind propagation speed
    public float  time;                   // elapsed simulation time in seconds
    public float  height_shear;           // altitude-dependent wind shear factor

    // Optical & Scattering Properties
    public float3 scattering_coeff;       // scattering cross section sigma_s (e.g. float3(1.0))
    public float3 extinction_coeff;       // extinction cross section sigma_t = sigma_s + sigma_a
    public float  phase_g_forward;        // forward Mie asymmetry parameter g1 in [0, 1] (e.g. 0.8)
    public float  phase_g_backward;       // backward Mie asymmetry parameter g2 in [-1, 0] (e.g. -0.2)
    public float  phase_blend;            // dual-lobe blend weight alpha in [0, 1] (e.g. 0.7)
    public float  powder_scale;           // Beer-Powder dark border / crevasse factor (e.g. 1.5)
    public float  multi_scatter_factor;   // multi-scattering energy transmission boost (e.g. 0.5)

    // Illumination
    public float3 sun_color;              // direct solar beam irradiance color
    public float  sun_intensity;          // solar radiance intensity multiplier (e.g. 20.0)
    public float3 ambient_sky_color;      // zenith ambient diffuse sky light color
    public float3 ambient_ground_color;   // nadir ground albedo bounce color
    public float  ambient_intensity;      // ambient diffuse scale factor
};

/**
 * @brief Volumetric raymarching accumulated result.
 */
public struct CloudRaymarchResult
{
    public float3 radiance;               // accumulated scattered in-radiance
    public float  transmittance;          // scalar beam transmittance remaining (0 = opaque, 1 = clear)
    public float3 transmittance_rgb;      // spectral beam transmittance vector
    public float  optical_depth;          // accumulated extinction optical depth
    public float  cloud_distance;         // ray distance to first non-negligible cloud density (for depth buffer)
    public float  alpha;                  // opacity scalar (1.0 - transmittance)
};

/**
 * @brief Ray-layer geometric intersection result.
 */
public struct CloudLayerIntersection
{
    public bool  hit;                     // whether the ray intersects the cloud layer
    public float t_enter;                 // entrance distance along ray
    public float t_exit;                  // exit distance along ray
};

/**
 * @brief Default Earth-like volumetric cloud configuration in kilometers.
 * Compatible with default_earth_atmosphere() in Atmosphere.slang.
 */
public CloudParameters default_earth_clouds()
{
    CloudParameters p;
    p.planet_radius         = 6360.0;
    p.cloud_bottom_altitude = 1.5;
    p.cloud_top_altitude    = 4.0;
    p.max_march_distance    = 50.0;

    p.cloud_coverage        = 0.48;
    p.cloud_density_scale   = 0.55;
    p.cloud_type            = 0.7; // predominantly cumulus / stratocumulus
    p.shape_scale           = 0.35;
    p.detail_scale          = 2.8;
    p.detail_strength       = 0.35;
    p.curl_strength         = 0.15;

    p.wind_direction        = normalize(float3(1.0, 0.0, 0.4));
    p.wind_speed            = 0.04;
    p.time                  = 0.0;
    p.height_shear          = 0.3;

    p.scattering_coeff      = float3(1.0, 1.0, 1.0);
    p.extinction_coeff      = float3(1.0, 1.0, 1.0);
    p.phase_g_forward       = 0.82;
    p.phase_g_backward      = -0.22;
    p.phase_blend           = 0.72;
    p.powder_scale          = 1.5;
    p.multi_scatter_factor  = 0.5;

    p.sun_color             = float3(1.0, 0.98, 0.92);
    p.sun_intensity         = 20.0;
    p.ambient_sky_color     = float3(0.35, 0.45, 0.65);
    p.ambient_ground_color  = float3(0.12, 0.11, 0.09);
    p.ambient_intensity     = 1.2;
    return p;
}

/**
 * @brief Default Earth-like volumetric cloud configuration in meters.
 * Suitable for world-space camera coordinates measured in meters.
 */
public CloudParameters default_earth_clouds_meters()
{
    CloudParameters p       = default_earth_clouds();
    p.planet_radius        *= 1000.0;
    p.cloud_bottom_altitude*= 1000.0;
    p.cloud_top_altitude   *= 1000.0;
    p.max_march_distance   *= 1000.0;
    p.shape_scale          *= 0.001;
    p.detail_scale         *= 0.001;
    p.wind_speed           *= 1000.0;
    return p;
}

// =============================================================================
// 1. Math, Mapping & Phase Functions
// =============================================================================

/**
 * @brief Linear value remapping between source range [old_min, old_max] and target range [new_min, new_max].
 */
public float cloud_remap(float value, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + (value - old_min) * (new_max - new_min) / max(old_max - old_min, 1e-5);
}

/**
 * @brief Linear value remapping clamped to [new_min, new_max].
 */
public float cloud_remap_saturate(float value, float old_min, float old_max, float new_min, float new_max)
{
    float res = cloud_remap(value, old_min, old_max, new_min, new_max);
    return clamp(res, min(new_min, new_max), max(new_min, new_max));
}

/**
 * @brief Standard Henyey-Greenstein scattering phase function.
 *
 * @param cos_theta Cosine of angle between ray direction and light direction
 * @param g Asymmetry parameter in [-1, 1] (positive = forward scattering)
 * @return Normalized phase probability density
 */
public float cloud_phase_hg(float cos_theta, float g)
{
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1e-4), 1.5);
    return INV_4PI * (1.0 - g2) / denom;
}

/**
 * @brief Dual-lobe Henyey-Greenstein phase function (Schneider 2015, Hillaire 2016).
 * Combines forward Mie scattering (silver lining around solar disc) with backward glory backscattering.
 *
 * @param cos_theta Cosine of scattering angle (dot(ray_dir, sun_dir))
 * @param g_forward Forward asymmetry parameter in [0, 1] (e.g. 0.8)
 * @param g_backward Backward asymmetry parameter in [-1, 0] (e.g. -0.2)
 * @param blend Relative weight of forward lobe in [0, 1] (e.g. 0.7)
 */
public float cloud_phase_dual_hg(float cos_theta, float g_forward, float g_backward, float blend)
{
    float forward_lobe  = cloud_phase_hg(cos_theta, g_forward);
    float backward_lobe = cloud_phase_hg(cos_theta, g_backward);
    return lerp(backward_lobe, forward_lobe, blend);
}

/**
 * @brief Schneider 2015 forward-peak Henyey-Greenstein phase function.
 * Adds an intense narrow silver-lining forward scattering boost when looking directly towards the sun.
 */
public float cloud_phase_schneider(float cos_theta, float g_forward, float g_backward, float blend)
{
    float dual_hg      = cloud_phase_dual_hg(cos_theta, g_forward, g_backward, blend);
    float silver_lining = pow(max(0.0, cos_theta), 16.0) * 0.5;
    return max(dual_hg, silver_lining);
}

// =============================================================================
// 2. Extinction, Powder & Multi-Scattering Laws
// =============================================================================

/**
 * @brief Beer-Lambert transmission law.
 * Computes direct monochromatic transmittance through a participating medium.
 *
 * @param optical_depth Integrated extinction optical depth tau
 */
public float cloud_beer_lambert(float optical_depth)
{
    return exp(-max(optical_depth, 0.0));
}

/**
 * @brief Sugar / Beer-Powder effect (Schneider 2015).
 * Simulates multiple scattering in cloud crevasses and edges.
 * Darkens interior cloud valleys while boosting bright illuminated edges when looking towards the sun.
 *
 * @param optical_depth Extinction optical depth
 * @param powder_scale Darkness exaggeration factor (typically 1.0 .. 2.0)
 */
public float cloud_beer_powder(float optical_depth, float powder_scale = 1.5)
{
    float tau = max(optical_depth, 0.0);
    return exp(-tau) * (1.0 - exp(-2.0 * tau * powder_scale));
}

/**
 * @brief Wrenninge 2013 / Bouthors multiple scattering approximation.
 * Evaluates an octave series where each consecutive multiple-scattering bounce
 * halves extinction optical depth and relaxes directional eccentricity, accumulating diffuse energy.
 *
 * @param optical_depth Primary optical depth to light source
 * @param cos_theta Scattering angle cosine
 * @param g Base phase asymmetry parameter
 * @param octaves Number of multiple-scattering octaves (typically 3 .. 4)
 * @param multi_scatter_factor Energy transmission scale per octave (e.g. 0.5)
 */
public float cloud_multiple_scattering_attenuation(
    float optical_depth,
    float cos_theta,
    float g,
    int   octaves              = 3,
    float multi_scatter_factor = 0.5)
{
    float acc_transmission = 0.0;
    float current_energy   = 1.0;
    float current_depth    = max(optical_depth, 0.0);
    float current_g        = g;

    for (int i = 0; i < octaves; ++i) {
        float phase = cloud_phase_hg(cos_theta, current_g);
        float trans = cloud_beer_lambert(current_depth);

        acc_transmission += current_energy * trans * phase;

        current_energy *= multi_scatter_factor;
        current_depth  *= 0.5;
        current_g      *= 0.5;
    }

    return acc_transmission;
}

// =============================================================================
// 3. Spherical Planetary Shell & Planar Geometry Intersections
// =============================================================================

/**
 * @brief Computes ray intersection interval [t_enter, t_exit] with concentric spherical cloud shell.
 * Correctly accounts for planetary curvature, observer inside/outside cloud shell, and ground occlusion.
 *
 * @param ro Ray origin in planetary coordinates (center of planet is at (0, 0, 0))
 * @param rd Unit ray direction
 * @param planet_radius Radius of planetary body
 * @param cloud_bottom_altitude Inner cloud shell boundary altitude above surface
 * @param cloud_top_altitude Outer cloud shell boundary altitude above surface
 * @return Intersection structure with hit flag and [t_enter, t_exit] distances
 */
public CloudLayerIntersection intersect_spherical_cloud_layer(
    float3 ro,
    float3 rd,
    float  planet_radius,
    float  cloud_bottom_altitude,
    float  cloud_top_altitude)
{
    CloudLayerIntersection result;
    result.hit     = false;
    result.t_enter = 0.0;
    result.t_exit  = 0.0;

    float r_inner = planet_radius + cloud_bottom_altitude;
    float r_outer = planet_radius + cloud_top_altitude;

    float r_cam   = length(ro);
    float b       = dot(ro, rd);
    float ro2     = dot(ro, ro);

    // 1. Intersect outer sphere (r_outer)
    float c_outer = ro2 - r_outer * r_outer;
    float d_outer = b * b - c_outer;
    if (d_outer < 0.0) {
        return result; // misses outer shell completely
    }
    float sqrt_d_outer = sqrt(d_outer);
    float t_outer_near = -b - sqrt_d_outer;
    float t_outer_far  = -b + sqrt_d_outer;

    if (t_outer_far < 0.0) {
        return result; // outer shell is entirely behind camera
    }

    // 2. Intersect inner sphere (r_inner)
    float c_inner = ro2 - r_inner * r_inner;
    float d_inner = b * b - c_inner;
    float sqrt_d_inner = d_inner >= 0.0 ? sqrt(d_inner) : -1.0;
    float t_inner_near = d_inner >= 0.0 ? -b - sqrt_d_inner : -1.0;
    float t_inner_far  = d_inner >= 0.0 ? -b + sqrt_d_inner : -1.0;

    // 3. Classify observer position relative to cloud shell
    if (r_cam < r_inner) {
        // observer is below cloud layer (on or near ground looking up)
        if (d_inner < 0.0 || t_inner_far < 0.0) {
            return result; // pointing downwards or away
        }
        result.t_enter = max(t_inner_far, 0.0);
        result.t_exit  = t_outer_far;
    } else if (r_cam > r_outer) {
        // observer is above cloud layer (space or orbital altitude looking down)
        if (t_outer_near < 0.0) {
            return result;
        }
        result.t_enter = max(t_outer_near, 0.0);
        if (d_inner >= 0.0 && t_inner_near > 0.0) {
            result.t_exit = t_inner_near; // clipped by top of inner sphere
        } else {
            result.t_exit = t_outer_far;
        }
    } else {
        // observer is inside cloud layer
        result.t_enter = 0.0;
        if (d_inner >= 0.0 && t_inner_near > 0.0) {
            result.t_exit = t_inner_near; // hits inner bottom boundary
        } else {
            result.t_exit = t_outer_far;  // hits outer top boundary
        }
    }

    // 4. Occlude ray if blocked by ground planet surface
    float c_ground = ro2 - planet_radius * planet_radius;
    float d_ground = b * b - c_ground;
    if (d_ground >= 0.0) {
        float t_ground = -b - sqrt(d_ground);
        if (t_ground > 0.0) {
            result.t_exit = min(result.t_exit, t_ground);
        }
    }

    if (result.t_exit > result.t_enter) {
        result.hit = true;
    }

    return result;
}

/**
 * @brief Computes ray intersection interval [t_enter, t_exit] with flat horizontal planar cloud slab.
 *
 * @param ro Ray origin with Y being vertical altitude
 * @param rd Unit ray direction
 * @param cloud_bottom_altitude Lower slab boundary altitude
 * @param cloud_top_altitude Upper slab boundary altitude
 */
public CloudLayerIntersection intersect_planar_cloud_layer(
    float3 ro,
    float3 rd,
    float  cloud_bottom_altitude,
    float  cloud_top_altitude)
{
    CloudLayerIntersection result;
    result.hit     = false;
    result.t_enter = 0.0;
    result.t_exit  = 0.0;

    if (abs(rd.y) < 1e-5) {
        // ray is horizontal: intersects only if camera is already between planes
        if (ro.y >= cloud_bottom_altitude && ro.y <= cloud_top_altitude) {
            result.hit     = true;
            result.t_enter = 0.0;
            result.t_exit  = 1e6;
        }
        return result;
    }

    float t0 = (cloud_bottom_altitude - ro.y) / rd.y;
    float t1 = (cloud_top_altitude - ro.y) / rd.y;

    float t_near = min(t0, t1);
    float t_far  = max(t0, t1);

    if (t_far < 0.0) {
        return result;
    }

    result.t_enter = max(t_near, 0.0);
    result.t_exit  = t_far;
    result.hit     = result.t_exit > result.t_enter;
    return result;
}

/**
 * @brief Calculates normalized relative altitude h in [0, 1] within spherical cloud shell.
 *
 * @param pos Current sample position
 * @param planet_radius Planetary radius
 * @param cloud_bottom_altitude Lower boundary altitude
 * @param cloud_top_altitude Upper boundary altitude
 */
public float cloud_relative_height(
    float3 pos,
    float  planet_radius,
    float  cloud_bottom_altitude,
    float  cloud_top_altitude)
{
    float altitude = length(pos) - planet_radius;
    float thickness = max(cloud_top_altitude - cloud_bottom_altitude, 1e-4);
    return saturate((altitude - cloud_bottom_altitude) / thickness);
}

// =============================================================================
// 4. Cloud Type Height Gradient Profiles
// =============================================================================

/**
 * @brief Height-density profile for Cumulus clouds.
 * Characteristics: sharp flat base, round fluffy convective core peaking at 30-40% height,
 * gentle rounded anvil top.
 */
public float cloud_cumulus_gradient(float h)
{
    float base = smoothstep(0.0, 0.15, h);
    float top  = smoothstep(1.0, 0.70, h);
    return base * top;
}

/**
 * @brief Height-density profile for Stratocumulus clouds.
 * Characteristics: broader flatter layer, gentle base, steady mid-deck density, sharp top falloff.
 */
public float cloud_stratocumulus_gradient(float h)
{
    float base = smoothstep(0.0, 0.25, h);
    float top  = smoothstep(1.0, 0.85, h);
    return base * top;
}

/**
 * @brief Height-density profile for Stratus clouds.
 * Characteristics: low-lying thin layer, sharp base, peaks near 15%, dissipates above 40%.
 */
public float cloud_stratus_gradient(float h)
{
    float base = smoothstep(0.0, 0.08, h);
    float top  = smoothstep(0.4, 0.15, h);
    return base * top;
}

/**
 * @brief Smoothly blends cloud height density gradient across cloud types:
 * 0.0 = Stratus, 0.5 = Stratocumulus, 1.0 = Cumulus.
 */
public float cloud_height_gradient(float h, float cloud_type)
{
    float stratus       = cloud_stratus_gradient(h);
    float stratocumulus = cloud_stratocumulus_gradient(h);
    float cumulus       = cloud_cumulus_gradient(h);

    float t = clamp(cloud_type, 0.0, 1.0);
    if (t < 0.5) {
        return lerp(stratus, stratocumulus, t * 2.0);
    } else {
        return lerp(stratocumulus, cumulus, (t - 0.5) * 2.0);
    }
}

// =============================================================================
// 5. Procedural Noise & Density Evaluation
// =============================================================================

/**
 * @brief Multi-octave inverted Worley noise using 3D cellular noise from Common.Noise.
 * Used for eroding cloud edges and producing wispy cauliflower billows.
 */
public float cloud_worley_fbm(float3 p, float freq_mult = 2.0)
{
    float w1 = 1.0 - saturate(cellular_noise(p).f1);
    float w2 = 1.0 - saturate(cellular_noise(p * freq_mult).f1);
    return w1 * 0.7 + w2 * 0.3;
}

/**
 * @brief Evaluates volumetric cloud density at a given sample position.
 * Applies wind animation, altitude wind shear, curl noise perturbation,
 * cloud height profile, low-frequency base shaping, and high-frequency Worley detail erosion.
 *
 * @param params Simulation parameters
 * @param world_pos Sample position in world / planetary coordinates
 * @param lod Level-of-detail flag (if > 0, skips high-frequency detail erosion for speed)
 * @return Local volumetric extinction density scalar
 */
public float sample_cloud_density(
    CloudParameters params,
    float3          world_pos,
    int             lod = 0)
{
    // 1. Compute normalized height fraction within cloud shell
    float h = cloud_relative_height(world_pos, params.planet_radius, params.cloud_bottom_altitude, params.cloud_top_altitude);
    if (h <= 0.0 || h >= 1.0) {
        return 0.0;
    }

    // 2. Evaluate cloud height density gradient
    float height_grad = cloud_height_gradient(h, params.cloud_type);
    if (height_grad <= 0.001) {
        return 0.0;
    }

    // 3. Apply wind drift and vertical wind shear
    float3 wind_offset = params.wind_direction * (params.time * params.wind_speed);
    float3 sample_p    = world_pos - wind_offset;
    sample_p.y        -= params.planet_radius;
    sample_p.xz       -= params.wind_direction.xz * (h * params.height_shear);

    // 4. Low-frequency shape noise with curl distortion
    float3 base_uvw = sample_p * params.shape_scale;
    if (params.curl_strength > 0.0) {
        float3 curl = curl_noise(base_uvw * 0.5, 0.05);
        base_uvw += curl * (params.curl_strength * (1.0 - h * 0.5));
    }

    float base_fbm    = fbm(base_uvw, 4, 2.0, 0.5);
    float base_worley = 1.0 - saturate(cellular_noise(base_uvw * 1.5).f1);
    // Perlin-Worley hybrid base noise (Schneider 2015: erode Perlin with Worley noise)
    float base_noise  = cloud_remap_saturate(base_fbm, base_worley - 1.0, 1.0, 0.0, 1.0);

    // 5. Modulate by coverage threshold and height profile
    float threshold    = 1.0 - params.cloud_coverage;
    float base_density = cloud_remap_saturate(base_noise * height_grad, threshold, 1.0, 0.0, 1.0);

    if (base_density <= 0.001) {
        return 0.0;
    }

    // 6. High-frequency detail erosion at cloud boundaries
    if (lod == 0 && params.detail_strength > 0.0) {
        float3 detail_uvw   = sample_p * params.detail_scale;
        float  detail_noise = cloud_worley_fbm(detail_uvw, 2.0);

        // erode more at edges where base density is low, preserve solid dense cloud core
        float edge_erosion = 1.0 - base_density;
        float erode_amount = detail_noise * params.detail_strength * edge_erosion;
        base_density       = cloud_remap_saturate(base_density, erode_amount, 1.0, 0.0, 1.0);
    }

    return base_density * params.cloud_density_scale;
}

// =============================================================================
// 6. Cloud Illumination & Light Marching
// =============================================================================

/**
 * @brief Computes ambient diffuse lighting received at a cloud sample point.
 * Simulates upward ground reflection and downward zenith skylight with self-shadowing ambient occlusion.
 *
 * @param params Simulation parameters
 * @param h Normalized altitude in [0, 1]
 * @param density Local sample density
 */
public float3 evaluate_cloud_ambient_light(
    CloudParameters params,
    float           h,
    float           density)
{
    // Ambient Occlusion: lower altitude deep inside clouds is occluded by cloud body
    float sky_ao        = smoothstep(0.0, 0.65, h) * saturate(1.0 - density * 0.6);
    float ground_bounce = smoothstep(0.35, 0.0, h) * 0.35;

    float3 sky_radiance    = params.ambient_sky_color * (sky_ao * params.ambient_intensity);
    float3 ground_radiance = params.ambient_ground_color * (ground_bounce * params.ambient_intensity);

    return sky_radiance + ground_radiance;
}

/**
 * @brief Light raymarching towards directional light source (Sun/Moon).
 * Computes self-shadowing optical depth with cone stepping and multi-scattering boost.
 *
 * @param params Cloud configuration
 * @param sample_pos Position inside cloud
 * @param sun_dir Unit direction towards sun
 * @param light_steps Number of sample steps towards light (e.g. 4 .. 6)
 * @param step_length Base step size along light ray
 * @return Beam transmittance scalar in [0, 1]
 */
public float march_cloud_light_transmittance(
    CloudParameters params,
    float3          sample_pos,
    float3          sun_dir,
    int             light_steps = 6,
    float           step_length = 0.08)
{
    float acc_optical_depth = 0.0;
    float current_step      = step_length;

    for (int i = 0; i < light_steps; ++i) {
        float3 light_p = sample_pos + sun_dir * (current_step * (float(i) + 0.5));
        float  d       = sample_cloud_density(params, light_p, 1); // lod 1: fast sampling without detail erosion
        acc_optical_depth += d * current_step;

        // cone expansion: larger steps further away from point
        current_step *= 1.35;
    }

    // Combine Beer-Lambert with Beer-Powder dark border effect (Schneider 2015)
    float trans_beer   = cloud_beer_lambert(acc_optical_depth);
    float powder_term  = 1.0 - exp(-2.0 * acc_optical_depth * params.powder_scale);
    float trans_powder = lerp(1.0, powder_term * 2.0, saturate(acc_optical_depth * 0.5));

    return trans_beer * trans_powder;
}

// =============================================================================
// 7. Full Volumetric Cloud Raymarching Pipeline
// =============================================================================

/**
 * @brief Primary volumetric raymarching function for planetary spherical cloud shell.
 * Integrates emission, scattering, extinction, dual-lobe Mie phase function,
 * light self-shadowing, ambient bounce, and early ray termination.
 *
 * @param params Cloud parameters
 * @param ro Ray origin
 * @param rd Unit ray direction
 * @param sun_dir Unit vector towards sun
 * @param max_distance Maximum scene distance (e.g. from depth buffer) to prevent marching through solid geometry
 * @param max_steps Maximum raymarching step count (e.g. 48 .. 96)
 * @param dither_offset Ray start jitter offset in [0, 1] to break up wood-grain stepping banding
 * @return Complete raymarch result with accumulated radiance, transmittance, optical depth, and hit distance
 */
public CloudRaymarchResult raymarch_volumetric_clouds(
    CloudParameters params,
    float3          ro,
    float3          rd,
    float3          sun_dir,
    float           max_distance  = 1e6,
    int             max_steps     = 64,
    float           dither_offset = 0.0)
{
    CloudRaymarchResult result;
    result.radiance          = float3(0.0);
    result.transmittance     = 1.0;
    result.transmittance_rgb = float3(1.0);
    result.optical_depth     = 0.0;
    result.cloud_distance    = -1.0;
    result.alpha             = 0.0;

    // 1. Intersect ray with spherical cloud layer boundaries
    CloudLayerIntersection isect = intersect_spherical_cloud_layer(
        ro,
        rd,
        params.planet_radius,
        params.cloud_bottom_altitude,
        params.cloud_top_altitude
    );

    if (!isect.hit) {
        return result;
    }

    // Clamp to maximum scene geometry depth
    float t_start = isect.t_enter;
    float t_end   = min(isect.t_exit, max_distance);
    if (t_end <= t_start) {
        return result;
    }

    float march_distance = min(t_end - t_start, params.max_march_distance);
    if (march_distance <= 0.0) {
        return result;
    }

    // 2. Precompute phase function and step configuration
    float cos_theta  = dot(rd, sun_dir);
    float phase_val  = cloud_phase_schneider(cos_theta, params.phase_g_forward, params.phase_g_backward, params.phase_blend);
    float step_size  = march_distance / float(max_steps);
    float light_step = step_size * 0.75;

    // Jitter initial step to suppress slicing / banding artifacts
    float current_t = t_start + step_size * clamp(dither_offset, 0.0, 1.0);

    // 3. March through volume
    int consecutive_zero_samples = 0;

    for (int i = 0; i < max_steps; ++i) {
        if (current_t >= t_end || result.transmittance < 0.02) {
            break; // early ray termination
        }

        float3 sample_p = ro + rd * current_t;
        float  density  = sample_cloud_density(params, sample_p, 0);

        if (density > 1e-4) {
            // inside cloud volume
            if (result.cloud_distance < 0.0) {
                result.cloud_distance = current_t; // record first surface contact
            }

            consecutive_zero_samples = 0;

            // Evaluate direct sun transmission with 3-step light marching
            float sun_trans = march_cloud_light_transmittance(params, sample_p, sun_dir, 3, light_step);

            // Multiple scattering approximation boost
            float ms_boost = cloud_multiple_scattering_attenuation(
                density * step_size * 5.0,
                cos_theta,
                params.phase_g_forward,
                3,
                params.multi_scatter_factor
            );

            float3 direct_light = params.sun_color * (params.sun_intensity * (sun_trans * phase_val + ms_boost * 0.1));

            // Ambient diffuse skylight and ground bounce
            float h = cloud_relative_height(sample_p, params.planet_radius, params.cloud_bottom_altitude, params.cloud_top_altitude);
            float3 ambient_light = evaluate_cloud_ambient_light(params, h, density);

            // Scattering and extinction integration
            float3 total_inscatter = direct_light * params.scattering_coeff + ambient_light;
            float  step_extinction = density * step_size;
            float  step_trans      = cloud_beer_lambert(step_extinction);

            // Analytic in-scatter integration along step
            float3 step_radiance = total_inscatter * ((1.0 - step_trans) * result.transmittance);
            result.radiance += step_radiance;

            result.transmittance *= step_trans;
            result.optical_depth += step_extinction;

            current_t += step_size;
        } else {
            // Empty space skipping: accelerate ray when density is zero
            consecutive_zero_samples++;
            float skip_multiplier = consecutive_zero_samples > 3 ? 2.5 : (consecutive_zero_samples > 1 ? 1.75 : 1.0);
            current_t += step_size * skip_multiplier;
        }
    }

    result.alpha             = saturate(1.0 - result.transmittance);
    result.transmittance_rgb = float3(result.transmittance);
    return result;
}

/**
 * @brief Volumetric raymarching for planar cloud slab (flat-world or localized rendering).
 */
public CloudRaymarchResult raymarch_planar_clouds(
    CloudParameters params,
    float3          ro,
    float3          rd,
    float3          sun_dir,
    float           max_distance  = 1e6,
    int             max_steps     = 64,
    float           dither_offset = 0.0)
{
    CloudRaymarchResult result;
    result.radiance          = float3(0.0);
    result.transmittance     = 1.0;
    result.transmittance_rgb = float3(1.0);
    result.optical_depth     = 0.0;
    result.cloud_distance    = -1.0;
    result.alpha             = 0.0;

    CloudLayerIntersection isect = intersect_planar_cloud_layer(
        ro,
        rd,
        params.cloud_bottom_altitude,
        params.cloud_top_altitude
    );

    if (!isect.hit) {
        return result;
    }

    float t_start = isect.t_enter;
    float t_end   = min(isect.t_exit, max_distance);
    if (t_end <= t_start) {
        return result;
    }

    float march_distance = min(t_end - t_start, params.max_march_distance);
    if (march_distance <= 0.0) {
        return result;
    }

    float cos_theta  = dot(rd, sun_dir);
    float phase_val  = cloud_phase_schneider(cos_theta, params.phase_g_forward, params.phase_g_backward, params.phase_blend);
    float step_size  = march_distance / float(max_steps);
    float light_step = step_size * 0.75;

    float current_t = t_start + step_size * clamp(dither_offset, 0.0, 1.0);

    for (int i = 0; i < max_steps; ++i) {
        if (current_t >= t_end || result.transmittance < 0.002) {
            break;
        }

        float3 sample_p = ro + rd * current_t;
        float  density  = sample_cloud_density(params, sample_p, 0);

        if (density > 1e-4) {
            if (result.cloud_distance < 0.0) {
                result.cloud_distance = current_t;
            }

            float  sun_trans    = march_cloud_light_transmittance(params, sample_p, sun_dir, 5, light_step);
            float3 direct_light = params.sun_color * (params.sun_intensity * (sun_trans * phase_val));

            float  h             = saturate((sample_p.y - params.cloud_bottom_altitude) / max(params.cloud_top_altitude - params.cloud_bottom_altitude, 1e-4));
            float3 ambient_light = evaluate_cloud_ambient_light(params, h, density);

            float3 total_inscatter = direct_light * params.scattering_coeff + ambient_light;
            float  step_extinction = density * step_size;
            float  step_trans      = cloud_beer_lambert(step_extinction);

            result.radiance     += total_inscatter * ((1.0 - step_trans) * result.transmittance);
            result.transmittance*= step_trans;
            result.optical_depth+= step_extinction;

            current_t += step_size;
        } else {
            current_t += step_size;
        }
    }

    result.alpha             = saturate(1.0 - result.transmittance);
    result.transmittance_rgb = float3(result.transmittance);
    return result;
}

// =============================================================================
// 8. Atmospheric Aerial Perspective & Compositing
// =============================================================================

/**
 * @brief Composites raymarched cloud layer over background scene with atmospheric aerial perspective.
 *
 * @param background_color Color of distant scene / clear sky behind clouds
 * @param cloud Accumulated cloud raymarching result
 * @param atmospheric_transmittance Extinction transmittance of atmosphere between observer and cloud
 * @param atmospheric_inscatter Scattered skylight radiance between observer and cloud
 */
public float3 composite_clouds(
    float3              background_color,
    CloudRaymarchResult cloud,
    float3              atmospheric_transmittance = float3(1.0),
    float3              atmospheric_inscatter     = float3(0.0))
{
    // Clouds occlude background by their alpha / transmittance
    float3 cloud_color = cloud.radiance;

    // Aerial perspective over distance to cloud
    cloud_color = cloud_color * atmospheric_transmittance + atmospheric_inscatter;

    // Blend with background
    return background_color * cloud.transmittance + cloud_color;
}
