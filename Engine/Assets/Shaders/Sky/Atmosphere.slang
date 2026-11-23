module Atmosphere;

// references: Bruneton & Neyret 2008, Nishita et al. 1993, Hillaire 2020
import Common.Colorspace;
import Common.Sampling;

public static const float PI = 3.14159265358979323846;

public struct AtmosphereParameters
{
    public float  planet_radius;          // e.g. 6360.0 km
    public float  atmosphere_radius;      // e.g. 6420.0 km
    public float  rayleigh_scale_height;  // density scale height hr, e.g. 8.0 km
    public float  mie_scale_height;       // density scale height hm, e.g. 1.2 km

    public float3 rayleigh_scattering;    // beta_R, e.g. float3(5.802, 13.558, 33.100) * 1e-3 (1/km)
    public float  mie_scattering;         // beta_M scattering, e.g. 3.996e-3 (1/km)

    public float  mie_extinction;         // beta_M extinction, e.g. 4.440e-3 (1/km)
    public float  mie_asymmetry;          // g parameter in [-1, 1], e.g. 0.8
    public float  sun_intensity;          // solar irradiance scaling factor, e.g. 20.0
    public float  sun_angular_radius;     // angular radius in radians (approx 0.00467 rad = 0.27 deg)
    public float3 ozone_absorption;       // beta_ozone, e.g. float3(0.650, 1.881, 0.085) * 1e-3 (1/km)
    public float  ground_albedo;          // surface diffuse albedo, e.g. 0.15
};

/**
 * @brief Default Earth-like atmosphere configuration.
 */
public AtmosphereParameters default_earth_atmosphere()
{
    AtmosphereParameters p;
    p.planet_radius         = 6360.0;
    p.atmosphere_radius     = 6420.0;
    p.rayleigh_scale_height = 8.0;
    p.mie_scale_height      = 1.2;
    p.rayleigh_scattering   = float3(5.802, 13.558, 33.100) * 1e-3;
    p.mie_scattering        = 3.996e-3;
    p.mie_extinction        = 4.440e-3;
    p.mie_asymmetry         = 0.8;
    p.sun_intensity         = 20.0;
    p.sun_angular_radius    = 0.004675; // ~0.268 degrees
    p.ozone_absorption      = float3(0.650, 1.881, 0.085) * 1e-3;
    p.ground_albedo         = 0.15;
    return p;
}

// =============================================================================
// 1. Phase Functions (Rayleigh & Cornette-Shanks Mie)
// =============================================================================

/**
 * @brief Rayleigh phase function for isotropic molecular scattering.
 */
public float rayleigh_phase(float cos_theta)
{
    return (3.0 / (16.0 * PI)) * (1.0 + cos_theta * cos_theta);
}

/**
 * @brief Cornette-Shanks analytic phase function for aerosol Mie scattering.
 */
public float mie_phase(float cos_theta, float g)
{
    float g2 = g * g;
    float num = 3.0 * (1.0 - g2) * (1.0 + cos_theta * cos_theta);
    float denom = (8.0 * PI) * (2.0 + g2) * pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1e-4), 1.5);
    return num / denom;
}

// =============================================================================
// 2. Ray-Sphere Planetary Boundary Intersection
// =============================================================================

/**
 * @brief Computes ray intersection interval [t_min, t_max] with concentric spherical atmosphere shell.
 */
public bool intersect_atmosphere(
    float3  ro,
    float3  rd,
    float   planet_radius,
    float   atmosphere_radius,
    out float out_t_min,
    out float out_t_max)
{
    out_t_min = 0.0;
    out_t_max = 0.0;

    // atmosphere sphere intersection
    float b = dot(ro, rd);
    float c = dot(ro, ro) - atmosphere_radius * atmosphere_radius;
    float d = b * b - c;
    if (d < 0.0) {
        return false;
    }
    float sqrt_d = sqrt(d);
    float t0 = -b - sqrt_d;
    float t1 = -b + sqrt_d;

    if (t1 < 0.0) {
        return false;
    }
    out_t_min = max(t0, 0.0);
    out_t_max = t1;

    // ground sphere occlusion
    float c_ground = dot(ro, ro) - planet_radius * planet_radius;
    float d_ground = b * b - c_ground;
    if (d_ground >= 0.0) {
        float t_ground = -b - sqrt(d_ground);
        if (t_ground > 0.0) {
            out_t_max = min(out_t_max, t_ground);
        }
    }

    return out_t_max > out_t_min;
}

// =============================================================================
// 3. Optical Depth & In-Scattering Numerical Integration
// =============================================================================

/**
 * @brief Integrates optical depth along a ray from position p towards the sun.
 * Returns optical depth for Rayleigh (x), Mie (y), and Ozone (z).
 */
public float3 integrate_optical_depth(
    AtmosphereParameters params,
    float3 p,
    float3 sun_dir,
    int    step_count = 8)
{
    // occluded by planet body
    float b_ground = dot(p, sun_dir);
    float c_ground = dot(p, p) - params.planet_radius * params.planet_radius;
    float d_ground = b_ground * b_ground - c_ground;
    if (d_ground >= 0.0) {
        float t_ground = -b_ground - sqrt(d_ground);
        if (t_ground > 0.0) {
            return float3(1e9, 1e9, 1e9);
        }
    }

    float t_min, t_max;
    if (!intersect_atmosphere(p, sun_dir, params.planet_radius, params.atmosphere_radius, t_min, t_max)) {
        return float3(0.0);
    }

    float step_size = t_max / float(step_count);
    float3 optical_depth = float3(0.0);

    for (int i = 0; i < step_count; ++i) {
        float3 sample_p = p + sun_dir * ((float(i) + 0.5) * step_size);
        float h = max(length(sample_p) - params.planet_radius, 0.0);
        optical_depth.x += exp(-h / params.rayleigh_scale_height) * step_size;
        optical_depth.y += exp(-h / params.mie_scale_height) * step_size;
        // Chappuis ozone layer tent profile centered at 25km with 15km half-width
        optical_depth.z += max(0.0, 1.0 - abs(h - 25.0) / 15.0) * step_size;
    }

    return optical_depth;
}

/**
 * @brief Evaluates physically based sky radiance along a viewing ray,
 * including single scattering, multi-scattering ambient diffuse illumination,
 * and Chappuis ozone absorption.
 *
 * @param params Atmosphere geometry and scattering coefficients
 * @param ro Camera world position (origin at center of planet)
 * @param rd Normalized camera viewing direction
 * @param sun_dir Normalized vector pointing towards the sun
 * @param num_steps Primary ray numerical march steps (e.g. 16 .. 32)
 * @return In-scattered linear HDR sky radiance
 */
public float3 evaluate_sky_radiance(
    AtmosphereParameters params,
    float3 ro,
    float3 rd,
    float3 sun_dir,
    int    num_steps = 16)
{
    float t_min, t_max;
    if (!intersect_atmosphere(ro, rd, params.planet_radius, params.atmosphere_radius, t_min, t_max)) {
        return float3(0.0);
    }

    float cos_theta = dot(rd, sun_dir);
    float p_rayleigh = rayleigh_phase(cos_theta);
    float p_mie      = mie_phase(cos_theta, params.mie_asymmetry);

    // compute ambient diffuse skylight and ground reflection field for multiple scattering.
    // In Earth atmosphere, diffuse multiple scattering prevents daytime horizon black/red-out
    // by providing an isotropic diffuse illumination bath.
    float sun_elev = max(sun_dir.y, 0.0);
    float od_zenith_r = params.rayleigh_scale_height;
    float od_zenith_m = params.mie_scale_height;
    float od_zenith_o = 15.0; // integrated thickness of ozone tent layer
    float3 tau_zenith = params.rayleigh_scattering * od_zenith_r +
                        params.mie_extinction * od_zenith_m +
                        params.ozone_absorption * od_zenith_o;
    float3 trans_zenith = exp(-tau_zenith);

    // downward diffuse flux from molecular scattering
    float3 e_sky = params.sun_intensity * sun_elev * (float3(1.0) - trans_zenith) * 0.45;
    // upward ground reflection
    float3 e_ground = (params.sun_intensity * sun_elev * trans_zenith + e_sky) * params.ground_albedo;
    // isotropic multi-scattering ambient flux density
    float3 psi_multi = (e_sky + e_ground) / (4.0 * PI);

    float3 acc_in_scatter = float3(0.0);
    float3 optical_depth_view = float3(0.0);

    // quadratic non-linear stepping distributes more sample points near the observer
    // where atmospheric density is highest, preventing horizon discretization artifacts.
    for (int i = 0; i < num_steps; ++i) {
        float u0 = float(i) / float(num_steps);
        float u1 = float(i + 1) / float(num_steps);
        float t0 = t_min + (t_max - t_min) * (u0 * u0);
        float t1 = t_min + (t_max - t_min) * (u1 * u1);
        float step_size = t1 - t0;
        float t = 0.5 * (t0 + t1);

        float3 sample_p = ro + rd * t;
        float h = max(length(sample_p) - params.planet_radius, 0.0);

        float d_r = exp(-h / params.rayleigh_scale_height) * step_size;
        float d_m = exp(-h / params.mie_scale_height) * step_size;
        float d_o = max(0.0, 1.0 - abs(h - 25.0) / 15.0) * step_size;

        optical_depth_view += float3(d_r, d_m, d_o);

        float3 optical_depth_sun = integrate_optical_depth(params, sample_p, sun_dir, 6);

        // solar beam transmittance from space to sample point
        float3 tau_sun = params.rayleigh_scattering * optical_depth_sun.x +
                         params.mie_extinction * optical_depth_sun.y +
                         params.ozone_absorption * optical_depth_sun.z;
        float3 trans_sun = exp(-tau_sun);

        // view path transmittance from sample point to camera
        float3 tau_view = params.rayleigh_scattering * optical_depth_view.x +
                          params.mie_extinction * optical_depth_view.y +
                          params.ozone_absorption * optical_depth_view.z;
        float3 trans_view = exp(-tau_view);

        // single scattering from direct solar beam
        float3 s_single = (d_r * params.rayleigh_scattering * p_rayleigh +
                           d_m * params.mie_scattering * p_mie) * (params.sun_intensity * trans_sun);

        // multiple scattering from ambient diffuse illumination field
        float3 s_multi = (d_r * params.rayleigh_scattering + d_m * params.mie_scattering) * psi_multi;

        acc_in_scatter += (s_single + s_multi) * trans_view;
    }

    return acc_in_scatter;
}

/**
 * @brief Renders physically based high-intensity solar disc with Eddington limb darkening,
 * atmospheric extinction attenuation, circumsolar Mie forward-scattering halo (corona),
 * and anti-aliased edge.
 *
 * @param params Atmosphere physical parameters
 * @param view_dir Normalized viewing direction
 * @param sun_dir Normalized vector pointing towards the sun
 * @param sun_transmittance Direct solar beam transmittance along sun path through atmosphere
 * @param halo_size Halo radius multiplier relative to sun disc
 * @param halo_falloff Halo falloff exponent
 * @param limb_darkening Eddington limb darkening coefficient [0, 1]
 * @return Direct solar physical radiance in cd/m^2 (nits)
 */
public float3 render_physical_sun(
    AtmosphereParameters params,
    float3 view_dir,
    float3 sun_dir,
    float3 sun_transmittance,
    float  halo_size = 10.0,
    float  halo_falloff = 80.0,
    float  limb_darkening = 0.6)
{
    float cos_theta = dot(view_dir, sun_dir);
    float theta     = acos(clamp(cos_theta, -1.0, 1.0));
    float r_sun     = max(params.sun_angular_radius, 1e-4);

    // 1. Mean top-of-atmosphere solar disc physical luminance (cd/m^2):
    // Solid angle of the sun disc: Omega = 2*PI*(1 - cos(r_sun)) approx PI * r_sun^2
    float solid_angle = max(2.0 * PI * (1.0 - cos(r_sun)), 1e-8);
    float mean_luminance = params.sun_intensity / solid_angle;

    // 2. Solar disc with Eddington limb darkening:
    // Center luminance I0 = L_mean / (1 - u / 3)
    float I0 = mean_luminance / max(1.0 - limb_darkening / 3.0, 0.01);

    float3 result = float3(0.0);

    if (theta <= r_sun) {
        float r = theta / r_sun;
        float mu = sqrt(max(0.0, 1.0 - r * r));
        float limb = 1.0 - limb_darkening * (1.0 - mu);

        // Anti-aliased disc boundary
        float edge_aa = smoothstep(r_sun, r_sun * 0.96, theta);
        result += float3(I0 * limb * edge_aa);
    }

    // 3. Circumsolar forward-scattering halo (corona)
    float r_halo = r_sun * max(halo_size, 1.0);
    if (theta < r_halo) {
        float halo_norm = (theta - r_sun * 0.5) / (r_halo - r_sun * 0.5);
        halo_norm = clamp(halo_norm, 0.0, 1.0);
        float halo_weight = pow(1.0 - halo_norm, max(halo_falloff * 0.1, 1.0));
        float halo_radiance = (mean_luminance * 0.002) * halo_weight;
        result += float3(halo_radiance);
    }

    // 4. Attenuate by atmospheric transmittance (sunsets turn golden/red)
    return result * sun_transmittance;
}

/**
 * @brief Renders analytical high-intensity solar disc with limb darkening.
 */
public float3 render_sun_disc(
    AtmosphereParameters params,
    float3 view_dir,
    float3 sun_dir,
    float  limb_darkening_factor = 0.6)
{
    float3 unit_transmittance = float3(1.0);
    return render_physical_sun(params, view_dir, sun_dir, unit_transmittance, 10.0, 80.0, limb_darkening_factor);
}
