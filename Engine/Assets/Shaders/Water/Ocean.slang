module Ocean;

// references: Tessendorf 2001 (ocean simulation), Finch 2004 (Gerstner waves)
import Common.RayTracing;
import Common.BSDF;
import Common.Noise;
import Common.Tonemapping;

static const float PI = 3.14159265358979323846;
static const float GRAVITY = 9.81;
static const float WATER_IOR = 1.333; // refractive index of pure water

public struct GerstnerWave
{
    public float2 direction;  // unit propagation direction
    public float  amplitude;  // wave height A
    public float  wavelength; // peak-to-peak distance lambda
    public float  steepness;  // crest sharpness Q in [0, 1]
    public float  phase;      // initial phase offset
    public float2 padding;    // 16-byte align each wave descriptor across targets
};

public struct WaterSurfacePoint
{
    public float3 position;      // displaced world position
    public float3 normal;        // analytical surface normal unit vector
    public float3 tangent;       // analytical surface tangent unit vector
    public float3 bitangent;     // analytical surface bitangent unit vector
    public float  foam_factor;   // wave crest jacobian foam mask [0, 1]
};

/**
 * @brief Default realistic ocean wave spectrum (swell + chop + capillary waves).
 */
public void default_ocean_waves(out GerstnerWave waves[4])
{
    // Wave 0: Primary ocean swell
    waves[0].direction  = normalize(float2(0.85, 0.52));
    waves[0].amplitude  = 0.40;
    waves[0].wavelength = 24.0;
    waves[0].steepness  = 0.35;
    waves[0].phase      = 0.0;
    waves[0].padding    = float2(0.0);

    // Wave 1: Secondary crossing swell
    waves[1].direction  = normalize(float2(0.55, -0.83));
    waves[1].amplitude  = 0.25;
    waves[1].wavelength = 14.0;
    waves[1].steepness  = 0.30;
    waves[1].phase      = 1.4;
    waves[1].padding    = float2(0.0);

    // Wave 2: Agitated wind chop
    waves[2].direction  = normalize(float2(-0.42, 0.90));
    waves[2].amplitude  = 0.14;
    waves[2].wavelength = 6.5;
    waves[2].steepness  = 0.42;
    waves[2].phase      = 2.8;
    waves[2].padding    = float2(0.0);

    // Wave 3: High-frequency capillary ripples
    waves[3].direction  = normalize(float2(0.92, -0.38));
    waves[3].amplitude  = 0.07;
    waves[3].wavelength = 2.8;
    waves[3].steepness  = 0.48;
    waves[3].phase      = 0.9;
    waves[3].padding    = float2(0.0);
}

/**
 * @brief Evaluates an ensemble of Gerstner waves at world coordinates (x, z) at time t.
 * Computes exact vertex displacement and analytical surface differential geometry (normal, tangent, bitangent).
 *
 * @param xz Horizontal world coordinates
 * @param time Elapsed time in seconds
 * @param waves Array of Gerstner wave descriptors
 * @return Surface point with displacement, orthonormal frame, and foam factor
 */
public WaterSurfacePoint evaluate_gerstner_waves(
    float2       xz,
    float        time,
    GerstnerWave waves[4])
{
    WaterSurfacePoint pt;
    pt.position = float3(xz.x, 0.0, xz.y);

    float3 tangent   = float3(1.0, 0.0, 0.0);
    float3 bitangent = float3(0.0, 0.0, 1.0);
    float3 normal    = float3(0.0, 1.0, 0.0);
    float  jacobian  = 1.0;

    for (int i = 0; i < 4; ++i) {
        float2 d = normalize(waves[i].direction);
        float a  = waves[i].amplitude;
        float w  = 2.0 * PI / max(waves[i].wavelength, 0.1); // wavenumber k
        float q  = clamp(waves[i].steepness / max(w * a * 4.0, 0.001), 0.0, 1.0); // normalized Q
        float speed = sqrt(GRAVITY / w); // dispersion relation in deep water
        float theta = w * dot(d, xz) - speed * w * time + waves[i].phase;

        float c = cos(theta);
        float s = sin(theta);

        // Gerstner vertex displacement
        pt.position.x += q * a * d.x * c;
        pt.position.z += q * a * d.y * c;
        pt.position.y += a * s;

        // Analytical partial derivatives
        float wa = w * a;
        tangent.x   -= q * d.x * d.x * wa * s;
        tangent.z   -= q * d.x * d.y * wa * s;
        tangent.y   += d.x * wa * c;

        bitangent.x -= q * d.x * d.y * wa * s;
        bitangent.z -= q * d.y * d.y * wa * s;
        bitangent.y += d.y * wa * c;

        normal.x    -= d.x * wa * c;
        normal.z    -= d.y * wa * c;
        normal.y    -= q * wa * s;

        jacobian    -= q * wa * s;
    }

    pt.normal     = normalize(normal);
    pt.tangent    = normalize(tangent);
    pt.bitangent  = normalize(bitangent);
    pt.foam_factor = clamp((0.8 - jacobian) * 4.0, 0.0, 1.0);

    return pt;
}

/**
 * @brief Computes physical underwater spectral light absorption using the Beer-Lambert law.
 * Reuses beer_lambert() from Common.RayTracing.
 *
 * @param water_depth Distance traveled by light through the water column in meters
 * @param water_clarity Pure ocean (1.0), coastal water (0.5), or muddy lake (0.1)
 * @return Transmittance color attenuation RGB
 */
public float3 compute_water_transmittance(float water_depth, float water_clarity = 1.0)
{
    // Pure ocean water absorption coefficients (red absorbs much faster than blue)
    const float3 BASE_ABSORPTION = float3(0.35, 0.06, 0.02); // 1/m
    float3 absorption = BASE_ABSORPTION / max(water_clarity, 0.05);
    return beer_lambert(absorption, water_depth);
}

/**
 * @brief Computes exact water surface reflection and transmission factors using Snell-Fresnel dielectric equations.
 * Reuses fresnel_dielectric() from Common.BSDF.
 *
 * @param NoV Cosine of view angle with water normal
 * @return Reflection fraction [0, 1] (transmission = 1 - reflection)
 */
public float compute_water_fresnel(float NoV)
{
    return fresnel_dielectric(NoV, 1.0, WATER_IOR);
}

/**
 * @brief Context parameters for lighting an ocean surface point.
 */
public struct OceanShadingContext
{
    public float3 view_dir;        // unit vector from surface toward camera
    public float3 sun_dir;         // unit vector pointing toward the sun
    public float3 sun_color;       // directional sun emission color
    public float  sun_intensity;   // sun radiance scale
    public float  water_depth;     // optical column depth in meters
    public float  water_clarity;   // 1.0 = clear ocean, 0.5 = coastal, 0.1 = turbid
    public float  roughness;       // microfacet roughness for glint evaluation
    public float4 deep_color;      // base deep ocean color
    public float4 shallow_color;   // wave crest / shallow water color
};

/**
 * @brief Full ocean surface lighting evaluation combining:
 * 1. Snell-Fresnel reflection and transmission splitting
 * 2. Cook-Torrance GGX sun specular glint reflection
 * 3. Atmospheric sky reflection gradient with horizon haze
 * 4. Beer-Lambert underwater spectral absorption and shallow SSS
 * 5. Dynamic foam accumulation on sharp wave crests (Jacobian J < 0)
 */
public static const float3 OCEAN_HORIZON_SKY = float3(0.55, 0.62, 0.66);
public static const float3 OCEAN_ZENITH_SKY  = float3(0.12, 0.28, 0.58);

/**
 * @brief sun_intensity is expected to be an exposed radiance scale (same units as the skybox pass),
 * not raw illuminance in lux.
 */
public float3 evaluate_ocean_surface(
    WaterSurfacePoint   pt,
    OceanShadingContext ctx)
{
    float3 N = normalize(pt.normal);
    float3 V = normalize(ctx.view_dir);
    float3 L = normalize(ctx.sun_dir);

    float NoV = clamp(dot(N, V), 0.001, 1.0);
    float NoL = clamp(dot(N, L), 0.0, 1.0);

    // 1. dielectric fresnel reflection factor
    float F = compute_water_fresnel(NoV);

    // 2. ggx sun glint
    float3 H   = normalize(L + V);
    float  NoH = clamp(dot(N, H), 0.0, 1.0);
    float  VoH = clamp(dot(V, H), 0.0, 1.0);

    float alpha  = max(ctx.roughness, 0.04);
    float alpha2 = alpha * alpha;
    float denom  = (NoH * NoH * (alpha2 - 1.0) + 1.0);
    float D      = alpha2 / max(PI * denom * denom, 1e-5);

    float k    = (alpha + 1.0) * (alpha + 1.0) / 8.0;
    float g1_l = NoL / (NoL * (1.0 - k) + k);
    float g1_v = NoV / (NoV * (1.0 - k) + k);
    float G    = g1_l * g1_v;

    float  sun_fresnel    = compute_water_fresnel(clamp(VoH, 0.001, 1.0));
    float3 specular_glint = (D * G * sun_fresnel / max(4.0 * NoV, 0.001))
                          * ctx.sun_color * (ctx.sun_intensity * 0.05);

    // 3. sky reflection along the reflected ray (below-horizon rays fold to the horizon color)
    float3 R           = reflect(-V, N);
    float  sky_elev    = clamp(R.y, 0.0, 1.0);
    float3 sky_radiance = lerp(OCEAN_HORIZON_SKY, OCEAN_ZENITH_SKY, pow(sky_elev, 0.45));

    float sun_halo = pow(clamp(dot(R, L), 0.0, 1.0), 64.0);
    sky_radiance += ctx.sun_color * (sun_halo * 0.04 * ctx.sun_intensity);

    float3 reflected_light = F * sky_radiance;

    // 4. in-scattered body color, lit by sky ambient and sun
    float3 transmittance = compute_water_transmittance(ctx.water_depth, ctx.water_clarity);
    float  crest_t       = clamp((pt.position.y + 0.3) * 1.2, 0.0, 1.0);
    float3 water_body    = lerp(ctx.deep_color.rgb, ctx.shallow_color.rgb, crest_t);

    float3 incident = float3(0.45, 0.6, 0.8) + ctx.sun_color * (ctx.sun_intensity * 0.03 * NoL);
    float  sss      = pow(clamp(dot(V, -L), 0.0, 1.0), 4.0) * crest_t;
    float3 subsurface = ctx.shallow_color.rgb * ctx.sun_color * (sss * ctx.sun_intensity * 0.05);

    float3 transmitted_light = (1.0 - F) * (water_body * incident * (0.35 + transmittance) + subsurface);

    // 5. foam on compressed crests
    float  foam_noise = perlin_noise(pt.position.xz * 4.0) * 0.5 + 0.5;
    float  foam_mask  = clamp(pt.foam_factor * (0.6 + foam_noise * 0.8), 0.0, 1.0);
    float3 foam_color = float3(0.85, 0.9, 0.95) * (0.35 + NoL * 0.25);

    float3 water_surface = transmitted_light + reflected_light + specular_glint;
    return lerp(water_surface, foam_color, foam_mask * 0.85);
}
