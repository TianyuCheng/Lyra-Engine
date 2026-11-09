module Sampling;

static const float PI        = 3.14159265358979323846;
static const float TWO_PI    = 6.28318530717958647692;
static const float INV_PI    = 0.31830988618379067154;
static const float INV_TWO_PI= 0.15915494309189533576;
static const float EPSILON   = 1e-6;

// -----------------------------------------------------------------------------
// Orthonormal Basis Construction
// -----------------------------------------------------------------------------

/**
 * @brief Constructs an orthonormal basis from a unit normal using Duff et al. 2017 branchless formulation.
 */
public void build_orthonormal_basis(float3 n, out float3 b1, out float3 b2)
{
    float sign = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (sign + n.z);
    float b = n.x * n.y * a;
    b1 = float3(1.0 + sign * n.x * n.x * a, sign * b, -sign * n.x);
    b2 = float3(b, sign + n.y * n.y * a, -n.y);
}

/**
 * @brief Transforms a direction from tangent space (where Z is up) to world space.
 */
public float3 tangent_to_world(float3 v, float3 N)
{
    float3 T, B;
    build_orthonormal_basis(N, T, B);
    return T * v.x + B * v.y + N * v.z;
}

// -----------------------------------------------------------------------------
// Low-Discrepancy Quasi-Monte Carlo Sequences
// -----------------------------------------------------------------------------

/**
 * @brief Radical inverse (Van der Corput) using 32-bit hardware bit-reversal.
 */
public float radical_inverse_vdc(uint bits)
{
    bits = reversebits(bits);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

/**
 * @brief 2D Hammersley low-discrepancy sequence.
 * @param i Index of sample in sequence [0 .. N - 1]
 * @param N Total number of samples
 * @return 2D sample in [0, 1)^2
 */
public float2 hammersley_2d(uint i, uint N)
{
    return float2(float(i) / float(N), radical_inverse_vdc(i));
}

// -----------------------------------------------------------------------------
// Sampling Zoo: Domain & Directional Samplers
// -----------------------------------------------------------------------------

/**
 * @brief Uniformly samples the unit sphere.
 * PDF = 1 / (4 * PI)
 */
public float3 sample_sphere_uniform(float2 u)
{
    float z = 1.0 - 2.0 * u.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = TWO_PI * u.y;
    return float3(r * cos(phi), r * sin(phi), z);
}

public float pdf_sphere_uniform()
{
    return 1.0 / (4.0 * PI);
}

/**
 * @brief Uniformly samples the upper hemisphere (Z >= 0 in tangent space).
 * PDF = 1 / (2 * PI)
 */
public float3 sample_hemisphere_uniform(float2 u)
{
    float z = u.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = TWO_PI * u.y;
    return float3(r * cos(phi), r * sin(phi), z);
}

public float pdf_hemisphere_uniform()
{
    return INV_TWO_PI;
}

/**
 * @brief Cosine-weighted hemisphere sampling (Malley's method).
 * PDF = cos_theta / PI
 */
public float3 sample_hemisphere_cosine(float2 u)
{
    float phi = TWO_PI * u.x;
    float sin_theta = sqrt(u.y);
    float cos_theta = sqrt(max(0.0, 1.0 - u.y));
    return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

public float pdf_hemisphere_cosine(float cos_theta)
{
    return max(0.0, cos_theta) * INV_PI;
}

/**
 * @brief Uniform spherical cone sampling (e.g. for analytical disk or spherical light sources).
 * @param u 2D random sample [0, 1)^2
 * @param cos_theta_max Cosine of cone half-angle
 * PDF = 1 / (2 * PI * (1 - cos_theta_max))
 */
public float3 sample_cone_uniform(float2 u, float cos_theta_max)
{
    float cos_theta = (1.0 - u.x) + u.x * cos_theta_max;
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    float phi = TWO_PI * u.y;
    return float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

public float pdf_cone_uniform(float cos_theta_max)
{
    return 1.0 / (TWO_PI * max(1.0 - cos_theta_max, EPSILON));
}

/**
 * @brief Shirley's concentric mapping from [0, 1)^2 to unit disk (preserves fractional area and boundaries).
 */
public float2 sample_disk_concentric(float2 u)
{
    float2 u_offset = 2.0 * u - 1.0;
    if (u_offset.x == 0.0 && u_offset.y == 0.0) {
        return float2(0.0, 0.0);
    }

    float r, phi;
    if (abs(u_offset.x) > abs(u_offset.y)) {
        r = u_offset.x;
        phi = (PI * 0.25) * (u_offset.y / u_offset.x);
    } else {
        r = u_offset.y;
        phi = (PI * 0.5) - (PI * 0.25) * (u_offset.x / u_offset.y);
    }
    return float2(r * cos(phi), r * sin(phi));
}

/**
 * @brief Uniform barycentric coordinate sampling on a triangle.
 */
public float3 sample_triangle_uniform(float2 u)
{
    float su1 = sqrt(u.x);
    float u_coord = 1.0 - su1;
    float v_coord = u.y * su1;
    return float3(u_coord, v_coord, 1.0 - u_coord - v_coord);
}

// -----------------------------------------------------------------------------
// Microfacet / Specular BRDF Samplers
// -----------------------------------------------------------------------------

/**
 * @brief Importance samples the GGX / Trowbridge-Reitz normal distribution (half-vector H).
 * @param u 2D random sample [0, 1)^2
 * @param roughness Linear perceptual roughness [0, 1]
 * @param N Macro surface normal in world space
 * @return Sampled microfacet normal H in world space
 */
public float3 sample_ggx(float2 u, float roughness, float3 N)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float phi = TWO_PI * u.x;
    float cos_theta = sqrt(max(0.0, (1.0 - u.y) / (1.0 + (a2 - 1.0) * u.y)));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

    // tangent-space H
    float3 H_tangent = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    return tangent_to_world(H_tangent, N);
}

/**
 * @brief PDF for sampling the reflected direction L from GGX half-vector H.
 */
public float pdf_ggx(float D, float NoH, float VoH)
{
    return (D * NoH) / (4.0 * max(VoH, EPSILON));
}

/**
 * @brief Eric Heitz 2018: Sampling the Visible Normal Distribution Function (VNDF) of GGX.
 * Strictly samples only visible microfacets from viewpoint V, drastically reducing sample variance.
 * @param u 2D random sample [0, 1)^2
 * @param roughness Linear perceptual roughness
 * @param V View vector in world space
 * @param N Macro surface normal in world space
 * @return Sampled microfacet normal H in world space
 */
public float3 sample_ggx_vndf(float2 u, float roughness, float3 V, float3 N)
{
    float a = max(roughness * roughness, 0.001);

    // construct orthonormal basis with N as +Z
    float3 T, B;
    build_orthonormal_basis(N, T, B);

    // transform view vector to local hemisphere frame
    float3 Vh = normalize(float3(dot(V, T) * a, dot(V, B) * a, dot(V, N)));

    // orthonormal basis around Vh
    float3 T1 = (Vh.z < 0.9999) ? normalize(cross(Vh, float3(0.0, 0.0, 1.0))) : float3(1.0, 0.0, 0.0);
    float3 T2 = cross(T1, Vh);

    // parameterize projected area
    float r = sqrt(u.x);
    float phi = TWO_PI * u.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(max(0.0, 1.0 - t1 * t1)) + s * t2;

    // unproject to local hemisphere
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    // unscale roughness and normalize
    float3 Ne = normalize(float3(a * Nh.x, a * Nh.y, max(0.0, Nh.z)));

    // transform back to world space
    return normalize(T * Ne.x + B * Ne.y + N * Ne.z);
}

/**
 * @brief Importance samples the Charlie sheen/cloth distribution.
 */
public float3 sample_charlie(float2 u, float roughness, float3 N)
{
    float a = max(roughness * roughness, 0.001);
    float inv_a = 1.0 / a;

    float phi = TWO_PI * u.x;
    float sin_theta = pow(u.y, a / (2.0 * a + 1.0));
    float cos_theta = sqrt(max(0.0, 1.0 - sin_theta * sin_theta));

    float3 H_tangent = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    return tangent_to_world(H_tangent, N);
}
