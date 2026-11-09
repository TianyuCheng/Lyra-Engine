module Packing;

// references: Meyer et al. 2010 (octahedral normal encoding), DXGI R11G11B10F

// -----------------------------------------------------------------------------
// 1. UNORM & SNORM 8-bit / 16-bit Packing
// -----------------------------------------------------------------------------

/**
 * @brief Packs four normalized floats in [0, 1] into a single 32-bit uint (8 bits per channel).
 */
public uint pack_unorm_4x8(float4 v)
{
    uint4 u = uint4(clamp(v, 0.0, 1.0) * 255.0 + 0.5);
    return (u.x) | (u.y << 8) | (u.z << 16) | (u.w << 24);
}

/**
 * @brief Unpacks four normalized floats in [0, 1] from a 32-bit uint.
 */
public float4 unpack_unorm_4x8(uint packed)
{
    float4 v;
    v.x = float(packed & 0xFF) / 255.0;
    v.y = float((packed >> 8) & 0xFF) / 255.0;
    v.z = float((packed >> 16) & 0xFF) / 255.0;
    v.w = float((packed >> 24) & 0xFF) / 255.0;
    return v;
}

/**
 * @brief Packs four signed normalized floats in [-1, 1] into a 32-bit uint.
 */
public uint pack_snorm_4x8(float4 v)
{
    int4 i = int4(clamp(v, -1.0, 1.0) * 127.0 + select(v >= 0.0, float4(0.5), float4(-0.5)));
    return (uint(i.x & 0xFF)) | (uint(i.y & 0xFF) << 8) | (uint(i.z & 0xFF) << 16) | (uint(i.w & 0xFF) << 24);
}

/**
 * @brief Unpacks four signed normalized floats in [-1, 1] from a 32-bit uint.
 */
public float4 unpack_snorm_4x8(uint packed)
{
    int4 i;
    i.x = int((packed << 24)) >> 24;
    i.y = int(((packed >> 8) << 24)) >> 24;
    i.z = int(((packed >> 16) << 24)) >> 24;
    i.w = int(packed) >> 24;
    return clamp(float4(i) / 127.0, -1.0, 1.0);
}

/**
 * @brief Packs two normalized floats in [0, 1] into a 32-bit uint (16 bits per channel).
 */
public uint pack_unorm_2x16(float2 v)
{
    uint2 u = uint2(clamp(v, 0.0, 1.0) * 65535.0 + 0.5);
    return (u.x) | (u.y << 16);
}

/**
 * @brief Unpacks two normalized floats in [0, 1] from a 32-bit uint.
 */
public float2 unpack_unorm_2x16(uint packed)
{
    return float2(
        float(packed & 0xFFFF) / 65535.0,
        float((packed >> 16) & 0xFFFF) / 65535.0);
}

/**
 * @brief Packs two signed normalized floats in [-1, 1] into a 32-bit uint.
 */
public uint pack_snorm_2x16(float2 v)
{
    int2 i = int2(clamp(v, -1.0, 1.0) * 32767.0 + select(v >= 0.0, float2(0.5), float2(-0.5)));
    return (uint(i.x & 0xFFFF)) | (uint(i.y & 0xFFFF) << 16);
}

/**
 * @brief Unpacks two signed normalized floats in [-1, 1] from a 32-bit uint.
 */
public float2 unpack_snorm_2x16(uint packed)
{
    int2 i;
    i.x = int((packed << 16)) >> 16;
    i.y = int(packed) >> 16;
    return clamp(float2(i) / 32767.0, -1.0, 1.0);
}

// =============================================================================
// 2. 16-Bit Half-Float Packing
// =============================================================================

/**
 * @brief Packs two 32-bit floats into two 16-bit IEEE 754 half floats packed in a 32-bit uint.
 */
public uint pack_half_2x16(float2 v)
{
    uint2 h = uint2(f32tof16(v.x), f32tof16(v.y));
    return (h.x & 0xFFFF) | ((h.y & 0xFFFF) << 16);
}

/**
 * @brief Unpacks two 16-bit IEEE 754 half floats from a 32-bit uint.
 */
public float2 unpack_half_2x16(uint packed)
{
    return float2(
        f16tof32(packed & 0xFFFF),
        f16tof32((packed >> 16) & 0xFFFF));
}

// =============================================================================
// 3. R11G11B10F Color Encoding
// =============================================================================

uint float_to_ufloat(float val, int num_mantissa_bits, int num_exponent_bits)
{
    if (val <= 0.0) return 0;

    uint uval = asuint(val);
    int exp = int((uval >> 23) & 0xFF) - 127;
    uint mantissa = uval & 0x7FFFFF;

    int exp_bias = (1 << (num_exponent_bits - 1)) - 1;
    int max_exp = (1 << num_exponent_bits) - 1;

    int new_exp = exp + exp_bias;
    if (new_exp <= 0) {
        return 0; // subnormal / underflow
    }
    if (new_exp >= max_exp) {
        return uint((max_exp << num_mantissa_bits)); // clamp to max float
    }

    uint new_mantissa = mantissa >> (23 - num_mantissa_bits);
    return uint(new_exp << num_mantissa_bits) | new_mantissa;
}

float ufloat_to_float(uint val, int num_mantissa_bits, int num_exponent_bits)
{
    int exp_bias = (1 << (num_exponent_bits - 1)) - 1;
    uint mantissa = val & ((1 << num_mantissa_bits) - 1);
    uint exp = val >> num_mantissa_bits;

    if (exp == 0) return 0.0;

    int full_exp = int(exp) - exp_bias + 127;
    uint full_mantissa = mantissa << (23 - num_mantissa_bits);
    return asfloat((uint(full_exp) << 23) | full_mantissa);
}

/**
 * @brief Packs an HDR RGB float3 into standard 32-bit DXGI / Vulkan R11G11B10F format.
 */
public uint pack_float3_to_r11g11b10f(float3 rgb)
{
    uint r = float_to_ufloat(max(rgb.r, 0.0), 6, 5);
    uint g = float_to_ufloat(max(rgb.g, 0.0), 6, 5);
    uint b = float_to_ufloat(max(rgb.b, 0.0), 5, 5);
    return (r & 0x7FF) | ((g & 0x7FF) << 11) | ((b & 0x3FF) << 22);
}

/**
 * @brief Unpacks an HDR RGB float3 from standard 32-bit R11G11B10F format.
 */
public float3 unpack_r11g11b10f_to_float3(uint packed)
{
    float r = ufloat_to_float(packed & 0x7FF, 6, 5);
    float g = ufloat_to_float((packed >> 11) & 0x7FF, 6, 5);
    float b = ufloat_to_float((packed >> 22) & 0x3FF, 5, 5);
    return float3(r, g, b);
}

// =============================================================================
// 4. Compressed Tangent Frames (TBN)
// =============================================================================

float2 oct_encode(float3 v)
{
    float l1 = dot(abs(v), float3(1.0));
    float2 p = v.xy * (1.0 / max(l1, 1e-6));
    if (v.z < 0.0) {
        p = (1.0 - abs(p.yx)) * select(p.xy >= 0.0, float2(1.0), float2(-1.0));
    }
    return p;
}

float3 oct_decode(float2 e)
{
    float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * select(v.xy >= 0.0, float2(1.0), float2(-1.0));
    }
    return normalize(v);
}

/**
 * @brief Packs full orthonormal tangent frame (Normal N, Tangent T, Bitangent sign) into two 32-bit uints.
 * Reduces G-Buffer normal/tangent storage from 32 bytes (2x float4) to 8 bytes (2x uint).
 *
 * @param N Surface normal unit vector
 * @param T Surface tangent unit vector
 * @param bitangent_sign Sign (+1.0 or -1.0) determining handedness of bitangent B = cross(N, T) * sign
 * @param out_packed_normal Packed octahedral normal in snorm_2x16
 * @param out_packed_tangent Packed octahedral tangent in snorm_2x16 + bitangent sign in sign bit
 */
public void pack_tangent_frame(
    float3    N,
    float3    T,
    float     bitangent_sign,
    out uint  out_packed_normal,
    out uint  out_packed_tangent)
{
    float2 oct_n = oct_encode(N);
    out_packed_normal = pack_snorm_2x16(oct_n);

    float2 oct_t = oct_encode(T);
    uint packed_t = pack_snorm_2x16(oct_t);
    // encode sign into the highest unused or bitflag
    uint sign_bit = (bitangent_sign < 0.0) ? 0x80000000u : 0u;
    out_packed_tangent = (packed_t & 0x7FFFFFFFu) | sign_bit;
}

/**
 * @brief Unpacks full orthonormal tangent frame (N, T, B) from two 32-bit uints.
 */
public void unpack_tangent_frame(
    uint      packed_normal,
    uint      packed_tangent,
    out float3 out_N,
    out float3 out_T,
    out float3 out_B)
{
    float2 oct_n = unpack_snorm_2x16(packed_normal);
    out_N = oct_decode(oct_n);

    float bitangent_sign = (packed_tangent & 0x80000000u) != 0u ? -1.0 : 1.0;
    float2 oct_t = unpack_snorm_2x16(packed_tangent & 0x7FFFFFFFu);
    out_T = oct_decode(oct_t);

    // Gram-Schmidt orthogonalization to ensure orthogonality
    out_T = normalize(out_T - out_N * dot(out_N, out_T));
    out_B = cross(out_N, out_T) * bitangent_sign;
}

// =============================================================================
// 5. Depth Linearization & Precision Utilities
// =============================================================================

/**
 * @brief Linearizes hardware standard depth (Z in [0, 1], near = 0, far = 1).
 */
public float linearize_depth(float hw_depth, float near_plane, float far_plane)
{
    return (near_plane * far_plane) / (far_plane - hw_depth * (far_plane - near_plane));
}

/**
 * @brief Linearizes hardware reverse-Z depth (Z in [1, 0], near = 1, far = 0).
 */
public float linearize_reverse_z_depth(float hw_depth, float near_plane, float far_plane)
{
    return (near_plane * far_plane) / (near_plane + hw_depth * (far_plane - near_plane));
}

/**
 * @brief Converts linear view-space depth into standard hardware depth in [0, 1].
 */
public float linear_to_hardware_depth(float linear_z, float near_plane, float far_plane)
{
    return (far_plane / (far_plane - near_plane)) * (1.0 - near_plane / max(linear_z, 1e-4));
}

/**
 * @brief Converts linear view-space depth into reverse-Z hardware depth in [1, 0].
 */
public float linear_to_reverse_z_depth(float linear_z, float near_plane, float far_plane)
{
    return (near_plane / (near_plane - far_plane)) * (1.0 - far_plane / max(linear_z, 1e-4));
}
