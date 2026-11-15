module Ocean;

// references: Tessendorf 2001 (ocean simulation), Finch 2004 (Gerstner waves)
import Common.RayTracing;
import Common.BSDF;
import Common.Noise;

static const float PI = 3.14159265358979323846;
static const float GRAVITY = 9.81;
static const float WATER_IOR = 1.333; // refractive index of pure water

public struct GerstnerWave
{
    public float2 direction; // unit propagation direction
    public float  amplitude; // wave height A
    public float  wavelength; // peak-to-peak distance lambda
    public float  steepness; // crest sharpness Q in [0, 1]
    public float  phase;     // initial phase offset
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
 * @brief Evaluates an ensemble of Gerstner waves at world coordinates (x, z) at time t.
 * Computes exact vertex displacement and analytical surface differential geometry (normal, tangent, bitangent).
 *
 * @param xz Horizontal world coordinates
 * @param time Elapsed time in seconds
 * @param waves Array of Gerstner wave descriptors
 * @param num_waves Number of wave components (e.g. 4 .. 8)
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
    pt.foam_factor = clamp((0.4 - jacobian) * 2.5, 0.0, 1.0);

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
