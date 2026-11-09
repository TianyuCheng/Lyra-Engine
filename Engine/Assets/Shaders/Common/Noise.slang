module Noise;

static const float PI = 3.14159265358979323846;

// -----------------------------------------------------------------------------
// 1. Integer & Floating-Point Hash Functions (PRNG)
// -----------------------------------------------------------------------------

/**
 * @brief High-quality integer PCG (Permuted Congruential Generator) hash.
 */
public uint pcg_hash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

public uint2 pcg_hash2(uint2 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v = v ^ (v >> 16u);
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v = v ^ (v >> 16u);
    return v;
}

public uint3 pcg_hash3(uint3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

/**
 * @brief Fast stateless float hashes (David Hoskins hash without sine).
 */
public float hash11(float p)
{
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

public float hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

public float hash13(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return frac((p.x + p.y) * p.z);
}

public float2 hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

public float3 hash33(float3 p)
{
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.xxy + p.yxx) * p.zyx);
}

// -----------------------------------------------------------------------------
// 2. Value Noise (1D, 2D, 3D)
// -----------------------------------------------------------------------------

/**
 * @brief 2D Value Noise with quintic Hermite interpolation.
 */
public float value_noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    // quintic Hermite curve: 6t^5 - 15t^4 + 10t^3
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float a = hash12(i + float2(0.0, 0.0));
    float b = hash12(i + float2(1.0, 0.0));
    float c = hash12(i + float2(0.0, 1.0));
    float d = hash12(i + float2(1.0, 1.0));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

/**
 * @brief 3D Value Noise with quintic Hermite interpolation.
 */
public float value_noise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);

    float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float n000 = hash13(i + float3(0.0, 0.0, 0.0));
    float n100 = hash13(i + float3(1.0, 0.0, 0.0));
    float n010 = hash13(i + float3(0.0, 1.0, 0.0));
    float n110 = hash13(i + float3(1.0, 1.0, 0.0));
    float n001 = hash13(i + float3(0.0, 0.0, 1.0));
    float n101 = hash13(i + float3(1.0, 0.0, 1.0));
    float n011 = hash13(i + float3(0.0, 1.0, 1.0));
    float n111 = hash13(i + float3(1.0, 1.0, 1.0));

    float x00 = lerp(n000, n100, u.x);
    float x10 = lerp(n010, n110, u.x);
    float x01 = lerp(n001, n101, u.x);
    float x11 = lerp(n011, n111, u.x);

    float y0 = lerp(x00, x10, u.y);
    float y1 = lerp(x01, x11, u.y);

    return lerp(y0, y1, u.z);
}

// -----------------------------------------------------------------------------
// 3. Perlin Gradient Noise (2D, 3D)
// -----------------------------------------------------------------------------

static float2 perlin_grad(float2 p)
{
    float2 h = hash22(p) * 2.0 - 1.0;
    return normalize(h);
}

static float3 perlin_grad(float3 p)
{
    float3 h = hash33(p) * 2.0 - 1.0;
    return normalize(h);
}

/**
 * @brief 2D Perlin Gradient Noise.
 */
public float perlin_noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float2 g00 = perlin_grad(i + float2(0.0, 0.0));
    float2 g10 = perlin_grad(i + float2(1.0, 0.0));
    float2 g01 = perlin_grad(i + float2(0.0, 1.0));
    float2 g11 = perlin_grad(i + float2(1.0, 1.0));

    float n00 = dot(g00, f - float2(0.0, 0.0));
    float n10 = dot(g10, f - float2(1.0, 0.0));
    float n01 = dot(g01, f - float2(0.0, 1.0));
    float n11 = dot(g11, f - float2(1.0, 1.0));

    return lerp(lerp(n00, n10, u.x), lerp(n01, n11, u.x), u.y) * 0.5 + 0.5;
}

/**
 * @brief 3D Perlin Gradient Noise.
 */
public float perlin_noise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);

    float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float n000 = dot(perlin_grad(i + float3(0.0, 0.0, 0.0)), f - float3(0.0, 0.0, 0.0));
    float n100 = dot(perlin_grad(i + float3(1.0, 0.0, 0.0)), f - float3(1.0, 0.0, 0.0));
    float n010 = dot(perlin_grad(i + float3(0.0, 1.0, 0.0)), f - float3(0.0, 1.0, 0.0));
    float n110 = dot(perlin_grad(i + float3(1.0, 1.0, 0.0)), f - float3(1.0, 1.0, 0.0));
    float n001 = dot(perlin_grad(i + float3(0.0, 0.0, 1.0)), f - float3(0.0, 0.0, 1.0));
    float n101 = dot(perlin_grad(i + float3(1.0, 0.0, 1.0)), f - float3(1.0, 0.0, 1.0));
    float n011 = dot(perlin_grad(i + float3(0.0, 1.0, 1.0)), f - float3(0.0, 1.0, 1.0));
    float n111 = dot(perlin_grad(i + float3(1.0, 1.0, 1.0)), f - float3(1.0, 1.0, 1.0));

    float x00 = lerp(n000, n100, u.x);
    float x10 = lerp(n010, n110, u.x);
    float x01 = lerp(n001, n101, u.x);
    float x11 = lerp(n011, n111, u.x);

    float y0 = lerp(x00, x10, u.y);
    float y1 = lerp(x01, x11, u.y);

    return lerp(y0, y1, u.z) * 0.5 + 0.5;
}

// -----------------------------------------------------------------------------
// 4. Simplex Noise (2D, 3D)
// -----------------------------------------------------------------------------

/**
 * @brief 2D Simplex Noise (Ken Perlin formulation).
 */
public float simplex_noise(float2 p)
{
    const float C_X = 0.211324865405187; // (3.0 - sqrt(3.0)) / 6.0
    const float C_Y = 0.366025403784439; // 0.5 * (sqrt(3.0) - 1.0)
    const float C_Z = -0.577350269189626; // -1.0 + 2.0 * C.x
    const float C_W = 0.024390243902439; // 1.0 / 41.0

    // first corner
    float2 i  = floor(p + dot(p, float2(C_Y, C_Y)));
    float2 x0 = p -   i + dot(i, float2(C_X, C_X));

    // other corners
    float2 i1  = (x0.x > x0.y) ? float2(1.0, 0.0) : float2(0.0, 1.0);
    float4 x12 = x0.xyxy + float4(C_X, C_X, C_Z, C_Z);
    x12.xy -= i1;

    // permutations
    i = frac(i * C_W);
    float3 p_idx = float3(dot(i, float2(1.0, 57.0)),
                          dot(i + i1, float2(1.0, 57.0)),
                          dot(i + 1.0, float2(1.0, 57.0)));

    float3 m = max(0.5 - float3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
    m = m * m;
    m = m * m;

    // gradients
    float3 x = 2.0 * frac(p_idx * 0.02439024) - 1.0;
    float3 h = abs(x) - 0.5;
    float3 ox = floor(x + 0.5);
    float3 a0 = x - ox;

    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);

    // compute final noise value
    float3 g;
    g.x  = a0.x  * x0.x   + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

/**
 * @brief 3D Simplex Noise.
 */
public float simplex_noise(float3 p)
{
    const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);
    const float4 D = float4(0.0, 0.5, 1.0, 2.0);

    // first corner
    float3 i  = floor(p + dot(p, float3(C.y, C.y, C.y)));
    float3 x0 = p - i + dot(i, float3(C.x, C.x, C.x));

    // other corners
    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    float3 x1 = x0 - i1 + float3(C.x, C.x, C.x);
    float3 x2 = x0 - i2 + float3(C.y, C.y, C.y);
    float3 x3 = x0 - 0.5;

    // contributions from the four corners
    float4 n = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    n = n * n;
    n = n * n;

    float4 p_hash = float4(hash13(i), hash13(i + i1), hash13(i + i2), hash13(i + 1.0));
    float4 p_grad = p_hash * 2.0 - 1.0;

    return 42.0 * dot(n, p_grad);
}

// -----------------------------------------------------------------------------
// 5. Worley / Cellular / Voronoi Noise (2D, 3D)
// -----------------------------------------------------------------------------

public struct CellularResult2D
{
    public float  f1;           // distance to closest point
    public float  f2;           // distance to second closest point
    public float2 cell_id;      // grid coordinate of closest cell
    public float2 cell_point;   // world space point of closest seed
};

/**
 * @brief 2D Cellular / Worley Noise.
 */
public CellularResult2D cellular_noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    CellularResult2D res;
    res.f1 = 8.0;
    res.f2 = 8.0;
    res.cell_id = float2(0.0);
    res.cell_point = float2(0.0);

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float2 neighbor = float2(float(x), float(y));
            float2 point = hash22(i + neighbor);
            float2 diff = neighbor + point - f;
            float dist = dot(diff, diff);

            if (dist < res.f1) {
                res.f2 = res.f1;
                res.f1 = dist;
                res.cell_id = i + neighbor;
                res.cell_point = i + neighbor + point;
            } else if (dist < res.f2) {
                res.f2 = dist;
            }
        }
    }

    res.f1 = sqrt(res.f1);
    res.f2 = sqrt(res.f2);
    return res;
}

public struct CellularResult3D
{
    public float  f1;
    public float  f2;
    public float3 cell_id;
};

/**
 * @brief 3D Cellular / Worley Noise.
 */
public CellularResult3D cellular_noise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);

    CellularResult3D res;
    res.f1 = 8.0;
    res.f2 = 8.0;
    res.cell_id = float3(0.0);

    for (int z = -1; z <= 1; ++z) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                float3 neighbor = float3(float(x), float(y), float(z));
                float3 point = hash33(i + neighbor);
                float3 diff = neighbor + point - f;
                float dist = dot(diff, diff);

                if (dist < res.f1) {
                    res.f2 = res.f1;
                    res.f1 = dist;
                    res.cell_id = i + neighbor;
                } else if (dist < res.f2) {
                    res.f2 = dist;
                }
            }
        }
    }

    res.f1 = sqrt(res.f1);
    res.f2 = sqrt(res.f2);
    return res;
}

// -----------------------------------------------------------------------------
// 6. Fractal Brownian Motion (fBm), Turbulence, & Ridged Multi-Fractal
// -----------------------------------------------------------------------------

/**
 * @brief 2D Fractal Brownian Motion based on Perlin noise.
 */
public float fbm(float2 p, int octaves = 5, float lacunarity = 2.0, float gain = 0.5)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * perlin_noise(p * frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }

    return value;
}

/**
 * @brief 3D Fractal Brownian Motion based on Perlin noise.
 */
public float fbm(float3 p, int octaves = 5, float lacunarity = 2.0, float gain = 0.5)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * perlin_noise(p * frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }

    return value;
}

/**
 * @brief Turbulence: absolute value fBm for fire, clouds, marble.
 */
public float turbulence(float3 p, int octaves = 5, float lacunarity = 2.0, float gain = 0.5)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * abs(perlin_noise(p * frequency) * 2.0 - 1.0);
        frequency *= lacunarity;
        amplitude *= gain;
    }

    return value;
}

/**
 * @brief Ridged Multi-Fractal: inverted sharp ridge fBm for mountains, terrain erosion, lightning.
 */
public float ridged_multifractal(float3 p, int octaves = 5, float lacunarity = 2.0, float gain = 0.5, float offset = 1.0)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    float weight = 1.0;

    for (int i = 0; i < octaves; ++i) {
        float signal = perlin_noise(p * frequency);
        signal = abs(signal * 2.0 - 1.0);
        signal = offset - signal;
        signal *= signal;
        signal *= weight;
        weight = clamp(signal * 2.0, 0.0, 1.0);

        value += signal * amplitude;
        frequency *= lacunarity;
        amplitude *= gain;
    }

    return value;
}

// -----------------------------------------------------------------------------
// 7. Curl Noise (3D Divergence-Free Vector Field)
// -----------------------------------------------------------------------------

/**
 * @brief 3D Curl Noise for fluid simulation, smoke, and advecting particles.
 * Computes curl(potential_field) = (dFz/dy - dFy/dz, dFx/dz - dFz/dx, dFy/dx - dFx/dy).
 */
public float3 curl_noise(float3 p, float eps = 1e-3)
{
    float2 h = float2(eps, 0.0);

    // potential field values from 3 offset simplex evaluations
    float p_x0 = simplex_noise(p + float3(12.3, 45.6, 78.9) + h.yxy) - simplex_noise(p + float3(12.3, 45.6, 78.9) - h.yxy);
    float p_x1 = simplex_noise(p + float3(12.3, 45.6, 78.9) + h.yyx) - simplex_noise(p + float3(12.3, 45.6, 78.9) - h.yyx);

    float p_y0 = simplex_noise(p + float3(55.1, 88.2, 22.3) + h.yyx) - simplex_noise(p + float3(55.1, 88.2, 22.3) - h.yyx);
    float p_y1 = simplex_noise(p + float3(55.1, 88.2, 22.3) + h.xyy) - simplex_noise(p + float3(55.1, 88.2, 22.3) - h.xyy);

    float p_z0 = simplex_noise(p + float3(91.7, 33.4, 66.5) + h.xyy) - simplex_noise(p + float3(91.7, 33.4, 66.5) - h.xyy);
    float p_z1 = simplex_noise(p + float3(91.7, 33.4, 66.5) + h.yxy) - simplex_noise(p + float3(91.7, 33.4, 66.5) - h.yxy);

    float inv_2h = 0.5 / eps;
    return float3(
        (p_z1 - p_y0) * inv_2h,
        (p_x1 - p_z0) * inv_2h,
        (p_y1 - p_x0) * inv_2h
    );
}

// -----------------------------------------------------------------------------
// 8. Screen-Space Interleaved Gradient Noise (IGN - Jorge Jimenez 2014)
// -----------------------------------------------------------------------------

/**
 * @brief Jorge Jimenez (Call of Duty: Advanced Warfare) Interleaved Gradient Noise.
 * Used for screen-space dithering, temporal supersampling, SSAO, SSR ray jittering.
 * @param pixel_coord Integer screen coordinate (e.g. SV_Position.xy)
 * @param frame_index Optional temporal frame counter for TAA accumulation
 * @return Dither value in [0, 1)
 */
public float interleaved_gradient_noise(float2 pixel_coord, uint frame_index = 0)
{
    // temporal animated offset
    pixel_coord += float(frame_index % 64u) * 5.588238;
    return frac(52.9829189 * frac(dot(pixel_coord, float2(0.06711056, 0.00583715))));
}
