module Colorspace;

static const float EPSILON = 1e-6;

// =============================================================================
// 1. Standard Reference Illuminants & White Points
// =============================================================================

// Standard CIE D65 white point in XYZ space
static const float3 D65_WHITE_XYZ = float3(0.95047, 1.00000, 1.08883);

// Standard CIE D65 white point chromaticity coordinates (x, y)
static const float2 D65_WHITE_XY  = float2(0.3127, 0.3290);

/**
 * @brief Computes CIE Rec.709 / linear sRGB relative luminance (Y in CIE XYZ).
 */
public float linear_srgb_to_luminance(float3 linear_srgb)
{
    return dot(linear_srgb, float3(0.212639, 0.715169, 0.072192));
}

/**
 * @brief Computes ITU-R BT.2020 relative luminance.
 */
public float linear_rec2020_to_luminance(float3 linear_rec2020)
{
    return dot(linear_rec2020, float3(0.262700, 0.677998, 0.059302));
}

/**
 * @brief Computes ACEScg relative luminance (AP1 gamut).
 */
public float linear_acescg_to_luminance(float3 linear_acescg)
{
    return dot(linear_acescg, float3(0.2722287168, 0.6740817658, 0.0536895174));
}

// =============================================================================
// 2. Electro-Optical & Opto-Electrical Transfer Functions (EOTF / OETF)
// =============================================================================

/**
 * @brief Exact IEC 61966-2-1 piecewise sRGB to Linear conversion.
 */
public float3 srgb_to_linear(float3 srgb)
{
    float3 lo = srgb / 12.92;
    float3 hi = pow(max((srgb + 0.055) / 1.055, float3(0.0)), float3(2.4));
    return lerp(lo, hi, float3(srgb > 0.04045));
}

/**
 * @brief Exact IEC 61966-2-1 piecewise Linear to sRGB conversion.
 */
public float3 linear_to_srgb(float3 linear_color)
{
    float3 lo = linear_color * 12.92;
    float3 hi = (pow(max(linear_color, float3(0.0)), float3(1.0 / 2.4)) * 1.055) - 0.055;
    return lerp(lo, hi, float3(linear_color > 0.0031308));
}

/**
 * @brief ITU-R BT.709 transfer function (HDTV).
 */
public float3 rec709_to_linear(float3 rec709)
{
    float3 lo = rec709 / 4.5;
    float3 hi = pow(max((rec709 + 0.099) / 1.099, float3(0.0)), float3(1.0 / 0.45));
    return lerp(lo, hi, float3(rec709 >= 0.081));
}

public float3 linear_to_rec709(float3 linear_color)
{
    float3 lo = linear_color * 4.5;
    float3 hi = 1.099 * pow(max(linear_color, float3(0.0)), float3(0.45)) - 0.099;
    return lerp(lo, hi, float3(linear_color >= 0.018));
}

/**
 * @brief SMPTE ST 2084 Perceptual Quantizer (PQ) EOTF for HDR10 displays (0 - 10,000 nits).
 * @param linear_nits Absolute linear scene radiance in cd/m^2 (nits), typically [0 .. 10000]
 * @return Normalized PQ signal in [0, 1]
 */
public float3 linear_to_pq(float3 linear_nits, float max_nits = 10000.0)
{
    float3 y = clamp(linear_nits / max_nits, float3(0.0), float3(1.0));
    float  m1 = 2610.0 / 16384.0;
    float  m2 = (2523.0 / 4096.0) * 128.0;
    float  c1 = 3424.0 / 4096.0;
    float  c2 = (2413.0 / 4096.0) * 32.0;
    float  c3 = (2392.0 / 4096.0) * 32.0;

    float3 ym1 = pow(y, float3(m1));
    return pow((c1 + c2 * ym1) / (1.0 + c3 * ym1), float3(m2));
}

public float3 pq_to_linear(float3 pq_color, float max_nits = 10000.0)
{
    float m1 = 2610.0 / 16384.0;
    float m2 = (2523.0 / 4096.0) * 128.0;
    float c1 = 3424.0 / 4096.0;
    float c2 = (2413.0 / 4096.0) * 32.0;
    float c3 = (2392.0 / 4096.0) * 32.0;

    float3 p = pow(clamp(pq_color, float3(0.0), float3(1.0)), float3(1.0 / m2));
    float3 num = max(p - c1, float3(0.0));
    float3 den = max(c2 - c3 * p, float3(EPSILON));
    return pow(num / den, float3(1.0 / m1)) * max_nits;
}

/**
 * @brief BT.2100 Hybrid Log-Gamma (HLG) OETF for broadcast HDR.
 */
public float3 linear_to_hlg(float3 linear_color)
{
    const float a = 0.17883277;
    const float b = 0.28466892; // 1 - 4*a
    const float c = 0.55991073; // 0.5 - a*ln(4*a)

    float3 lo = sqrt(3.0 * linear_color) * 0.5;
    float3 hi = a * log(max(12.0 * linear_color - b, float3(EPSILON))) + c;
    return lerp(lo, hi, float3(linear_color > (1.0 / 12.0)));
}

public float3 hlg_to_linear(float3 hlg_color)
{
    const float a = 0.17883277;
    const float b = 0.28466892;
    const float c = 0.55991073;

    float3 lo = (hlg_color * hlg_color) / 3.0;
    float3 hi = (exp((hlg_color - c) / a) + b) / 12.0;
    return lerp(lo, hi, float3(hlg_color > 0.5));
}

// =============================================================================
// 3. CIE Colorimetry Models (XYZ, xyY, Lab)
// =============================================================================

/**
 * @brief Converts Linear sRGB (D65) to CIE 1931 XYZ.
 */
public float3 linear_srgb_to_xyz(float3 rgb)
{
    // sRGB to XYZ matrix (D65)
    return float3(
        dot(rgb, float3(0.4124564, 0.3575761, 0.1804375)),
        dot(rgb, float3(0.2126729, 0.7151522, 0.0721750)),
        dot(rgb, float3(0.0193339, 0.1191920, 0.9503041))
    );
}

/**
 * @brief Converts CIE 1931 XYZ to Linear sRGB (D65).
 */
public float3 xyz_to_linear_srgb(float3 xyz)
{
    // XYZ to sRGB matrix (D65)
    return float3(
        dot(xyz, float3( 3.2404542, -1.5371385, -0.4985314)),
        dot(xyz, float3(-0.9692660,  1.8760108,  0.0415560)),
        dot(xyz, float3( 0.0556434, -0.2040259,  1.0572252))
    );
}

/**
 * @brief Converts CIE XYZ to CIE xyY chromaticity coordinates.
 */
public float3 xyz_to_xyy(float3 xyz)
{
    float sum = xyz.x + xyz.y + xyz.z;
    if (sum < EPSILON) {
        return float3(D65_WHITE_XY.x, D65_WHITE_XY.y, 0.0);
    }
    return float3(xyz.x / sum, xyz.y / sum, xyz.y);
}

/**
 * @brief Converts CIE xyY chromaticity coordinates back to CIE XYZ.
 */
public float3 xyy_to_xyz(float3 xyy)
{
    if (xyy.y < EPSILON) {
        return float3(0.0);
    }
    return float3((xyy.x * xyy.z) / xyy.y, xyy.z, ((1.0 - xyy.x - xyy.y) * xyy.z) / xyy.y);
}

static float f_lab(float t)
{
    const float delta = 6.0 / 29.0;
    return t > (delta * delta * delta) ? pow(t, 1.0 / 3.0) : (t / (3.0 * delta * delta)) + (4.0 / 29.0);
}

static float f_inv_lab(float t)
{
    const float delta = 6.0 / 29.0;
    return t > delta ? (t * t * t) : 3.0 * delta * delta * (t - 4.0 / 29.0);
}

/**
 * @brief Converts CIE XYZ to CIE L*a*b* (perceptually uniform color space).
 */
public float3 xyz_to_lab(float3 xyz, float3 white_point = D65_WHITE_XYZ)
{
    float fx = f_lab(xyz.x / white_point.x);
    float fy = f_lab(xyz.y / white_point.y);
    float fz = f_lab(xyz.z / white_point.z);

    float L = 116.0 * fy - 16.0;
    float a = 500.0 * (fx - fy);
    float b = 200.0 * (fy - fz);
    return float3(L, a, b);
}

public float3 lab_to_xyz(float3 lab, float3 white_point = D65_WHITE_XYZ)
{
    float fy = (lab.x + 16.0) / 116.0;
    float fx = fy + (lab.y / 500.0);
    float fz = fy - (lab.z / 200.0);

    return float3(
        white_point.x * f_inv_lab(fx),
        white_point.y * f_inv_lab(fy),
        white_point.z * f_inv_lab(fz)
    );
}

/**
 * @brief Euclidean CIE76 color difference (Delta E) between two colors in Lab space.
 * Thresholds: Delta E < 1.0 is imperceptible to the human eye.
 */
public float delta_e_cie76(float3 lab1, float3 lab2)
{
    return distance(lab1, lab2);
}

// =============================================================================
// 4. Color Gamut Conversions (ACES, Rec.709, DCI-P3, Rec.2020)
// =============================================================================

/**
 * @brief Converts Linear sRGB / Rec.709 to ACEScg (AP1 primaries).
 */
public float3 srgb_to_acescg(float3 rgb)
{
    return float3(
        dot(rgb, float3(0.613097, 0.339523, 0.047379)),
        dot(rgb, float3(0.070194, 0.916354, 0.013452)),
        dot(rgb, float3(0.020616, 0.109570, 0.869814))
    );
}

/**
 * @brief Converts ACEScg (AP1 primaries) to Linear sRGB / Rec.709.
 */
public float3 acescg_to_srgb(float3 acescg)
{
    return float3(
        dot(acescg, float3( 1.704859, -0.621715, -0.083299)),
        dot(acescg, float3(-0.130078,  1.140733, -0.010560)),
        dot(acescg, float3(-0.023964, -0.128975,  1.153014))
    );
}

/**
 * @brief Converts Linear sRGB to DCI-P3 (Display P3 with D65 white point).
 */
public float3 srgb_to_dci_p3(float3 rgb)
{
    return float3(
        dot(rgb, float3(0.822462, 0.177538, 0.000000)),
        dot(rgb, float3(0.033194, 0.966806, 0.000000)),
        dot(rgb, float3(0.017083, 0.072397, 0.910520))
    );
}

/**
 * @brief Converts DCI-P3 (D65) to Linear sRGB.
 */
public float3 dci_p3_to_srgb(float3 p3)
{
    return float3(
        dot(p3, float3( 1.224940, -0.224940,  0.000000)),
        dot(p3, float3(-0.042057,  1.042057,  0.000000)),
        dot(p3, float3(-0.019638, -0.078636,  1.098274))
    );
}

/**
 * @brief Converts Linear sRGB to Rec.2020 / BT.2020 wide color gamut.
 */
public float3 srgb_to_rec2020(float3 rgb)
{
    return float3(
        dot(rgb, float3(0.627404, 0.329283, 0.043313)),
        dot(rgb, float3(0.069097, 0.919540, 0.011362)),
        dot(rgb, float3(0.016391, 0.088013, 0.895595))
    );
}

/**
 * @brief Converts Rec.2020 / BT.2020 to Linear sRGB.
 */
public float3 rec2020_to_srgb(float3 rec2020)
{
    return float3(
        dot(rec2020, float3( 1.660491, -0.587641, -0.072850)),
        dot(rec2020, float3(-0.124550,  1.132899, -0.008349)),
        dot(rec2020, float3(-0.018151, -0.100579,  1.118730))
    );
}

/**
 * @brief Converts ACEScg (AP1) to ACES2065-1 (AP0 interchange format).
 */
public float3 acescg_to_aces2065_1(float3 ap1)
{
    return float3(
        dot(ap1, float3(0.695491, 0.140679, 0.163830)),
        dot(ap1, float3(0.044798, 0.859671, 0.095531)),
        dot(ap1, float3(-0.005526, 0.004025, 1.001501))
    );
}

public float3 aces2065_1_to_acescg(float3 ap0)
{
    return float3(
        dot(ap0, float3( 1.451439, -0.236511, -0.214929)),
        dot(ap0, float3(-0.076554,  1.176230, -0.099676)),
        dot(ap0, float3( 0.008316, -0.006032,  0.997716))
    );
}

// =============================================================================
// 5. Alternate Color Representations (HSV, HSL, YCbCr)
// =============================================================================

/**
 * @brief Converts Linear RGB to HSV (Hue in [0, 1], Saturation [0, 1], Value [0, 1]).
 */
public float3 rgb_to_hsv(float3 rgb)
{
    float4 k = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(rgb.bg, k.wz), float4(rgb.gb, k.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));

    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

/**
 * @brief Converts HSV to Linear RGB.
 */
public float3 hsv_to_rgb(float3 hsv)
{
    float4 k = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + k.xyz) * 6.0 - k.www);
    return hsv.z * lerp(k.xxx, clamp(p - k.xxx, float3(0.0), float3(1.0)), hsv.y);
}

/**
 * @brief Converts Linear RGB to ITU-R BT.709 YCbCr.
 */
public float3 rgb_to_ycbcr_rec709(float3 rgb)
{
    float y  =  0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
    float cb = -0.1146 * rgb.r - 0.3854 * rgb.g + 0.5000 * rgb.b;
    float cr =  0.5000 * rgb.r - 0.4542 * rgb.g - 0.0458 * rgb.b;
    return float3(y, cb, cr);
}

public float3 ycbcr_to_rgb_rec709(float3 ycbcr)
{
    return float3(
        ycbcr.x                    + 1.5748 * ycbcr.z,
        ycbcr.x - 0.1873 * ycbcr.y - 0.4681 * ycbcr.z,
        ycbcr.x + 1.8556 * ycbcr.y
    );
}

// =============================================================================
// 6. Physical Blackbody Radiation (Kelvin Color Temperature to RGB)
// =============================================================================

/**
 * @brief Evaluates the Planckian blackbody radiation spectrum for a color temperature in Kelvin.
 * Tanner Helland / Mitchell Charity polynomial approximation.
 * @param temperature_kelvin Temperature in Kelvin [1000K .. 15000K]
 *   - Candle flame: ~1850K
 *   - Tungsten incandescent bulb: ~2800K
 *   - Halogen lamp: ~3200K
 *   - Fluorescent tube: ~4000K
 *   - Direct sunlight: ~5500K
 *   - D65 standard daylight: ~6500K
 *   - Overcast sky: ~7500K
 *   - Clear blue sky: ~10000K - 15000K
 * @return Normalized linear RGB emission color
 */
public float3 kelvin_to_rgb(float temperature_kelvin)
{
    float temp = clamp(temperature_kelvin, 1000.0, 15000.0) / 100.0;
    float r, g, b;

    // Red calculation
    if (temp <= 66.0) {
        r = 1.0;
    } else {
        r = temp - 60.0;
        r = 329.698727446 * pow(max(r, 0.001), -0.1332047592);
        r = clamp(r / 255.0, 0.0, 1.0);
    }

    // Green calculation
    if (temp <= 66.0) {
        g = temp;
        g = 99.4708025861 * log(max(g, 0.001)) - 161.1195681661;
        g = clamp(g / 255.0, 0.0, 1.0);
    } else {
        g = temp - 60.0;
        g = 288.1221695283 * pow(max(g, 0.001), -0.0755148492);
        g = clamp(g / 255.0, 0.0, 1.0);
    }

    // Blue calculation
    if (temp >= 66.0) {
        b = 1.0;
    } else if (temp <= 19.0) {
        b = 0.0;
    } else {
        b = temp - 10.0;
        b = 138.5177312231 * log(max(b, 0.001)) - 305.0447927307;
        b = clamp(b / 255.0, 0.0, 1.0);
    }

    // Convert from perceptual non-linear curve to linear physical radiance
    float3 srgb = float3(r, g, b);
    return srgb_to_linear(srgb);
}
