module Tonemapping;

// -----------------------------------------------------------------------------
// Color Space Conversions
// -----------------------------------------------------------------------------

public float3 linear_to_srgb(float3 color)
{
    float3 lo = color * 12.92;
    float3 hi = (pow(max(abs(color), float3(1e-5)), float3(1.0 / 2.4)) * 1.055) - 0.055;
    return lerp(lo, hi, float3(color > 0.0031308));
}

public float3 srgb_to_linear(float3 color)
{
    float3 lo = color / 12.92;
    float3 hi = pow((color + 0.055) / 1.055, float3(2.4));
    return lerp(lo, hi, float3(color > 0.04045));
}

public float4 linear_to_srgb(float4 color)
{
    return float4(linear_to_srgb(color.rgb), color.a);
}

// -----------------------------------------------------------------------------
// Tone Mapping Operators
// -----------------------------------------------------------------------------

/**
 * @brief Simple Reinhard tone mapping operator.
 */
public float3 tonemap_reinhard(float3 color)
{
    return color / (color + float3(1.0));
}

/**
 * @brief Extended Reinhard with luminance white point preserve.
 */
public float3 tonemap_reinhard_extended(float3 color, float max_white)
{
    float3 numerator = color * (1.0 + (color / (max_white * max_white)));
    return numerator / (1.0 + color);
}

/**
 * @brief ACES Film tone mapping curve by Krzysztof Narkowicz.
 * High performance, widely used in game engines.
 */
public float3 tonemap_aces_film(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

/**
 * @brief John Hable / Uncharted 2 filmic tone curve helper.
 */
float3 hable_tonemap_partial(float3 x)
{
    float A = 0.15; // Shoulder Strength
    float B = 0.50; // Linear Strength
    float C = 0.10; // Linear Angle
    float D = 0.20; // Toe Strength
    float E = 0.02; // Toe Numerator
    float F = 0.30; // Toe Denominator
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

/**
 * @brief Uncharted 2 filmic tone mapping operator.
 */
public float3 tonemap_uncharted2(float3 color, float exposure = 2.0)
{
    float W = 11.2; // Linear White Point Value
    float3 curr = hable_tonemap_partial(color * exposure);
    float3 white_scale = float3(1.0) / hable_tonemap_partial(float3(W));
    return curr * white_scale;
}

/**
 * @brief Modern AgX tonemapping approximation (Troy Sobotka).
 * Provides cinematic highlight roll-off and preserves saturation across bright highlights.
 */
public float3 tonemap_agx(float3 color)
{
    // input matrix (sRGB to AgX working space)
    static const float3x3 agx_mat = float3x3(
        0.8424790619653353,  0.0784335999999992,  0.0792237458265074,
        0.0423282422610123,  0.878468636469772,   0.0791661279446337,
        0.0423756549057051,  0.0784336,           0.8791429734169527
    );

    // output matrix
    static const float3x3 agx_out_mat = float3x3(
        1.19687900512017,   -0.052896851700548, -0.052971635508472,
       -0.0980208811401368,  1.15190312990417,  -0.0980434501171241,
       -0.0990297440797205, -0.0989611768448433, 1.20101404602514
    );

    float3 val = mul(agx_mat, color);

    // log2 encoding [-10, +6.5] EV
    float min_ev = -10.0;
    float max_ev = 6.5;
    val = clamp(log2(max(val, float3(1e-10))), min_ev, max_ev);
    val = (val - min_ev) / (max_ev - min_ev);

    // AgX sigmoid curve approximation
    float3 x = val;
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    float3 sigmoid = 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;

    float3 linear_agx = mul(agx_out_mat, sigmoid);
    return clamp(linear_agx, 0.0, 1.0);
}
