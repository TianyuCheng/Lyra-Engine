module SphericalHarmonics;

static const float PI = 3.14159265358979323846;

// =============================================================================
// 1. Spherical Harmonics Data Structures (Order-3 / Band 0, 1, 2 = 9 Coefficients)
// =============================================================================

/**
 * @brief 9-coefficient scalar Spherical Harmonics structure.
 */
public struct SH9
{
    public float c[9];

    public static SH9 zero()
    {
        SH9 res;
        [unroll]
        for (int i = 0; i < 9; ++i) res.c[i] = 0.0;
        return res;
    }

    public SH9 add(SH9 other)
    {
        SH9 res;
        [unroll]
        for (int i = 0; i < 9; ++i) res.c[i] = c[i] + other.c[i];
        return res;
    }

    public SH9 multiply(float scalar)
    {
        SH9 res;
        [unroll]
        for (int i = 0; i < 9; ++i) res.c[i] = c[i] * scalar;
        return res;
    }
};

/**
 * @brief 9-coefficient RGB Spherical Harmonics structure (L2 SH, 27 floats).
 * Industry standard for diffuse irradiance environment mapping and light probes.
 */
public struct SH9Color
{
    public float3 c[9];

    public static SH9Color zero()
    {
        SH9Color res;
        [unroll]
        for (int i = 0; i < 9; ++i) res.c[i] = float3(0.0);
        return res;
    }

    public SH9Color add(SH9Color other)
    {
        SH9Color res;
        [unroll]
        for (int i = 0; i < 9; ++i) res.c[i] = c[i] + other.c[i];
        return res;
    }

    public SH9Color multiply(float scalar)
    {
        SH9Color res;
        [unroll]
        for (int i = 0; i < 9; ++i) res.c[i] = c[i] * scalar;
        return res;
    }

    public static SH9Color lerp_sh(SH9Color a, SH9Color b, float t)
    {
        SH9Color res;
        [unroll]
        for (int i = 0; i < 9; ++i) res.c[i] = lerp(a.c[i], b.c[i], t);
        return res;
    }
};

// =============================================================================
// 2. Real Spherical Harmonics Basis Functions Evaluation (Y_lm)
// =============================================================================

/**
 * @brief Evaluates all 9 real Spherical Harmonics basis functions Y_lm for a unit direction vector.
 * @param d Normalized direction vector (x, y, z)
 * @param sh Output array of 9 basis evaluations
 */
public void evaluate_sh_basis_9(float3 d, out float sh[9])
{
    // Band 0: l = 0, m = 0
    sh[0] = 0.28209479; // 0.5 * sqrt(1.0 / PI)

    // Band 1: l = 1, m = -1, 0, 1
    const float C1 = 0.48860251; // 0.5 * sqrt(3.0 / PI)
    sh[1] = C1 * d.y;
    sh[2] = C1 * d.z;
    sh[3] = C1 * d.x;

    // Band 2: l = 2, m = -2, -1, 0, 1, 2
    const float C2_xy = 1.09254843; // 0.5 * sqrt(15.0 / PI)
    const float C2_z  = 0.31539156; // 0.25 * sqrt(5.0 / PI)
    const float C2_sq = 0.54627421; // 0.25 * sqrt(15.0 / PI)

    sh[4] = C2_xy * (d.x * d.y);
    sh[5] = C2_xy * (d.y * d.z);
    sh[6] = C2_z  * (3.0 * d.z * d.z - 1.0);
    sh[7] = C2_xy * (d.x * d.z);
    sh[8] = C2_sq * (d.x * d.x - d.y * d.y);
}

// =============================================================================
// 3. Diffuse Irradiance Evaluation (Ramamoorthi & Hanrahan / Peter-Pike Sloan)
// =============================================================================

/**
 * @brief Evaluates diffuse irradiance E(N) from an L2 SH representation in ~10 arithmetic operations.
 * Uses the Peter-Pike Sloan / Ramamoorthi & Hanrahan quadratic polynomial representation.
 * @param sh L2 RGB Spherical Harmonics coefficients (already convolved with cosine lobe, or convolved internally)
 * @param N Macro surface normal vector in world space
 * @return Diffuse irradiance in linear RGB
 */
public float3 evaluate_sh_irradiance(SH9Color sh, float3 N)
{
    // Quadratic polynomial constants derived by Ramamoorthi & Hanrahan
    const float c1 = 0.429043;
    const float c2 = 0.511664;
    const float c3 = 0.743125;
    const float c4 = 0.886227;
    const float c5 = 0.247708;

    float x = N.x;
    float y = N.y;
    float z = N.z;

    float3 irradiance =
        c4 * sh.c[0] +
        2.0 * c2 * (sh.c[3] * x + sh.c[1] * y + sh.c[2] * z) +
        c1 * sh.c[8] * (x * x - y * y) +
        c3 * sh.c[6] * (z * z) - c5 * sh.c[6] +
        2.0 * c1 * (sh.c[4] * (x * y) + sh.c[7] * (x * z) + sh.c[5] * (y * z));

    return max(irradiance, float3(0.0));
}

/**
 * @brief Evaluates raw (unconvolved) radiance in direction `dir`.
 */
public float3 evaluate_sh_radiance(SH9Color sh, float3 dir)
{
    float b[9];
    evaluate_sh_basis_9(dir, b);

    float3 radiance = float3(0.0);
    [unroll]
    for (int i = 0; i < 9; ++i) {
        radiance += sh.c[i] * b[i];
    }
    return max(radiance, float3(0.0));
}

// =============================================================================
// 4. Directional Light Projection & Cosine Lobe Convolution
// =============================================================================

/**
 * @brief Projects a directional light (or sun disk) into L2 Spherical Harmonics coefficients.
 * @param dir Normalized pointing direction towards the light source
 * @param radiance Incident light radiance (color * intensity)
 */
public SH9Color project_directional_light(float3 dir, float3 radiance)
{
    float b[9];
    evaluate_sh_basis_9(dir, b);

    SH9Color res;
    [unroll]
    for (int i = 0; i < 9; ++i) {
        res.c[i] = radiance * b[i];
    }
    return res;
}

/**
 * @brief Convolves raw environment radiance SH coefficients with the clamped cosine lobe.
 * Converts environment radiance L_lm into diffuse irradiance E_lm coefficients:
 *   Band 0: A_0 = PI
 *   Band 1: A_1 = 2*PI / 3
 *   Band 2: A_2 = PI / 4
 */
public SH9Color convolve_cosine_lobe(SH9Color sh_radiance)
{
    const float A0 = PI;
    const float A1 = (2.0 * PI) / 3.0;
    const float A2 = PI / 4.0;

    SH9Color res;
    // Band 0
    res.c[0] = sh_radiance.c[0] * A0;

    // Band 1
    res.c[1] = sh_radiance.c[1] * A1;
    res.c[2] = sh_radiance.c[2] * A1;
    res.c[3] = sh_radiance.c[3] * A1;

    // Band 2
    res.c[4] = sh_radiance.c[4] * A2;
    res.c[5] = sh_radiance.c[5] * A2;
    res.c[6] = sh_radiance.c[6] * A2;
    res.c[7] = sh_radiance.c[7] * A2;
    res.c[8] = sh_radiance.c[8] * A2;

    return res;
}

// =============================================================================
// 5. Sloan Windowing (Ringing & Negative Irradiance Suppression)
// =============================================================================

/**
 * @brief Smoothly dampens higher frequency SH bands using Peter-Pike Sloan windowing.
 * Prevents ringing artifacts and negative irradiance lobes caused by high-contrast lights.
 * Formula: w_l = 1.0 / (1.0 + lambda * l^2 * (l + 1)^2)
 * @param sh Input SH9Color
 * @param lambda Smoothing factor (default: 0.05, increase for aggressive ringing suppression)
 */
public SH9Color apply_sloan_windowing(SH9Color sh, float lambda = 0.05)
{
    // l = 0: w0 = 1.0
    // l = 1: w1 = 1.0 / (1.0 + lambda * 4.0)
    // l = 2: w2 = 1.0 / (1.0 + lambda * 36.0)
    float w0 = 1.0;
    float w1 = 1.0 / (1.0 + lambda * 4.0);
    float w2 = 1.0 / (1.0 + lambda * 36.0);

    SH9Color res;
    res.c[0] = sh.c[0] * w0;

    res.c[1] = sh.c[1] * w1;
    res.c[2] = sh.c[2] * w1;
    res.c[3] = sh.c[3] * w1;

    res.c[4] = sh.c[4] * w2;
    res.c[5] = sh.c[5] * w2;
    res.c[6] = sh.c[6] * w2;
    res.c[7] = sh.c[7] * w2;
    res.c[8] = sh.c[8] * w2;

    return res;
}

// =============================================================================
// 6. Bent Normal & Ambient Occlusion (Directional Occlusion)
// =============================================================================

/**
 * @brief Evaluates diffuse irradiance along the bent normal, weighted by scalar ambient occlusion.
 */
public float3 evaluate_sh_bent_normal(SH9Color sh, float3 bent_normal, float ao)
{
    float3 irradiance = evaluate_sh_irradiance(sh, normalize(bent_normal));
    return irradiance * ao;
}
