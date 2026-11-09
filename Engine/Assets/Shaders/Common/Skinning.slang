module Skinning;

static const float EPSILON = 1e-6;

// =============================================================================
// 1. Skinned Vertex Output Structure
// =============================================================================

public struct SkinnedVertex
{
    public float3 position;
    public float3 normal;
    public float3 tangent;
};

// =============================================================================
// 2. Weight Normalization Utilities
// =============================================================================

public float4 normalize_weights_4(float4 weights)
{
    float sum = dot(weights, float4(1.0));
    return sum > EPSILON ? (weights / sum) : float4(1.0, 0.0, 0.0, 0.0);
}

public void normalize_weights_8(inout float4 weights0, inout float4 weights1)
{
    float sum = dot(weights0, float4(1.0)) + dot(weights1, float4(1.0));
    if (sum > EPSILON) {
        float inv_sum = 1.0 / sum;
        weights0 *= inv_sum;
        weights1 *= inv_sum;
    } else {
        weights0 = float4(1.0, 0.0, 0.0, 0.0);
        weights1 = float4(0.0);
    }
}

// =============================================================================
// 3. Linear Blend Skinning (LBS) - Standard 4x4 Matrices
// =============================================================================

/**
 * @brief 4-bone Linear Blend Skinning (LBS) using standard 4x4 bone matrices.
 */
public SkinnedVertex skin_lbs_4(
    float3 position,
    float3 normal,
    float3 tangent,
    uint4  joints,
    float4 weights,
    StructuredBuffer<float4x4> bone_matrices)
{
    weights = normalize_weights_4(weights);

    float4x4 skin_matrix =
        bone_matrices[joints.x] * weights.x +
        bone_matrices[joints.y] * weights.y +
        bone_matrices[joints.z] * weights.z +
        bone_matrices[joints.w] * weights.w;

    SkinnedVertex out_v;
    out_v.position = mul(skin_matrix, float4(position, 1.0)).xyz;
    out_v.normal   = normalize(mul((float3x3)skin_matrix, normal));
    out_v.tangent  = normalize(mul((float3x3)skin_matrix, tangent));
    return out_v;
}

/**
 * @brief 8-bone Linear Blend Skinning (LBS) for high-fidelity characters and faces.
 */
public SkinnedVertex skin_lbs_8(
    float3 position,
    float3 normal,
    float3 tangent,
    uint4  joints0,
    uint4  joints1,
    float4 weights0,
    float4 weights1,
    StructuredBuffer<float4x4> bone_matrices)
{
    normalize_weights_8(weights0, weights1);

    float4x4 skin_matrix =
        bone_matrices[joints0.x] * weights0.x +
        bone_matrices[joints0.y] * weights0.y +
        bone_matrices[joints0.z] * weights0.z +
        bone_matrices[joints0.w] * weights0.w +
        bone_matrices[joints1.x] * weights1.x +
        bone_matrices[joints1.y] * weights1.y +
        bone_matrices[joints1.z] * weights1.z +
        bone_matrices[joints1.w] * weights1.w;

    SkinnedVertex out_v;
    out_v.position = mul(skin_matrix, float4(position, 1.0)).xyz;
    out_v.normal   = normalize(mul((float3x3)skin_matrix, normal));
    out_v.tangent  = normalize(mul((float3x3)skin_matrix, tangent));
    return out_v;
}

// =============================================================================
// 4. Linear Blend Skinning (LBS) - Compact 3x4 Affine Matrices (25% Bandwidth Saving)
// =============================================================================

/**
 * @brief 4-bone LBS using compact 3x4 affine matrices (3 rows, 4 columns).
 */
public SkinnedVertex skin_lbs_compact_4(
    float3 position,
    float3 normal,
    float3 tangent,
    uint4  joints,
    float4 weights,
    StructuredBuffer<float3x4> bone_matrices)
{
    weights = normalize_weights_4(weights);

    float3x4 skin_matrix =
        bone_matrices[joints.x] * weights.x +
        bone_matrices[joints.y] * weights.y +
        bone_matrices[joints.z] * weights.z +
        bone_matrices[joints.w] * weights.w;

    SkinnedVertex out_v;
    out_v.position = mul(skin_matrix, float4(position, 1.0));
    out_v.normal   = normalize(mul(skin_matrix, float4(normal, 0.0)));
    out_v.tangent  = normalize(mul(skin_matrix, float4(tangent, 0.0)));
    return out_v;
}

// =============================================================================
// 5. Dual Quaternion Skinning (DQS) - Eliminates Candy-Wrapper Volume Loss
// =============================================================================

public struct DualQuat
{
    public float4 real; // rotation quaternion (x, y, z, w)
    public float4 dual; // translation dual quaternion (x, y, z, w)

    public DualQuat negate()
    {
        DualQuat dq;
        dq.real = -real;
        dq.dual = -dual;
        return dq;
    }

    public float3 transform_point(float3 p)
    {
        // Optimized Rodrigues rotation formula: p' = p + 2 * cross(q0.xyz, cross(q0.xyz, p) + q0.w * p)
        float3 t0 = 2.0 * cross(real.xyz, cross(real.xyz, p) + real.w * p);
        // Translation: 2 * (q0.w * qe.xyz - qe.w * q0.xyz + cross(q0.xyz, qe.xyz))
        float3 trans = 2.0 * (real.w * dual.xyz - dual.w * real.xyz + cross(real.xyz, dual.xyz));
        return p + t0 + trans;
    }

    public float3 transform_vector(float3 v)
    {
        // Pure rotation
        return v + 2.0 * cross(real.xyz, cross(real.xyz, v) + real.w * v);
    }
};

/**
 * @brief Blends 4 dual quaternions with antipodal alignment (Kavan et al. 2007).
 */
public DualQuat blend_dual_quats_4(
    DualQuat dq0, DualQuat dq1, DualQuat dq2, DualQuat dq3,
    float4 weights)
{
    // Antipodal alignment: ensure all quaternions lie in the same hemisphere as dq0
    if (dot(dq0.real, dq1.real) < 0.0) dq1 = dq1.negate();
    if (dot(dq0.real, dq2.real) < 0.0) dq2 = dq2.negate();
    if (dot(dq0.real, dq3.real) < 0.0) dq3 = dq3.negate();

    DualQuat blended;
    blended.real = dq0.real * weights.x + dq1.real * weights.y + dq2.real * weights.z + dq3.real * weights.w;
    blended.dual = dq0.dual * weights.x + dq1.dual * weights.y + dq2.dual * weights.z + dq3.dual * weights.w;

    float len = length(blended.real);
    if (len > EPSILON) {
        float inv_len = 1.0 / len;
        blended.real *= inv_len;
        blended.dual *= inv_len;
    }
    return blended;
}

/**
 * @brief 4-bone Dual Quaternion Skinning (DQS).
 */
public SkinnedVertex skin_dqs_4(
    float3 position,
    float3 normal,
    float3 tangent,
    uint4  joints,
    float4 weights,
    StructuredBuffer<DualQuat> bone_dual_quats)
{
    weights = normalize_weights_4(weights);

    DualQuat dq0 = bone_dual_quats[joints.x];
    DualQuat dq1 = bone_dual_quats[joints.y];
    DualQuat dq2 = bone_dual_quats[joints.z];
    DualQuat dq3 = bone_dual_quats[joints.w];

    DualQuat blended_dq = blend_dual_quats_4(dq0, dq1, dq2, dq3, weights);

    SkinnedVertex out_v;
    out_v.position = blended_dq.transform_point(position);
    out_v.normal   = normalize(blended_dq.transform_vector(normal));
    out_v.tangent  = normalize(blended_dq.transform_vector(tangent));
    return out_v;
}

// =============================================================================
// 6. Morph Target / Blend Shape Blending
// =============================================================================

/**
 * @brief Accumulates delta positions, normals, and tangents for morph targets / facial blend shapes.
 */
public void apply_morph_target(
    inout float3 position,
    inout float3 normal,
    inout float3 tangent,
    float3 delta_position,
    float3 delta_normal,
    float3 delta_tangent,
    float  weight)
{
    position += delta_position * weight;
    normal   += delta_normal   * weight;
    tangent  += delta_tangent  * weight;
}

// =============================================================================
// 7. Skinned Motion Vector Calculation (TAA & Motion Blur)
// =============================================================================

/**
 * @brief Computes 2D screen-space velocity vector for skinned geometry across frames.
 * @param curr_skinned_pos Current frame skinned world position
 * @param prev_skinned_pos Previous frame skinned world position
 * @param curr_view_proj Current frame View-Projection matrix
 * @param prev_view_proj Previous frame View-Projection matrix
 * @return 2D screen UV motion vector (subtract from current UV to get previous UV)
 */
public float2 compute_skinned_motion_vector(
    float3   curr_skinned_pos,
    float3   prev_skinned_pos,
    float4x4 curr_view_proj,
    float4x4 prev_view_proj)
{
    float4 curr_clip = mul(curr_view_proj, float4(curr_skinned_pos, 1.0));
    float4 prev_clip = mul(prev_view_proj, float4(prev_skinned_pos, 1.0));

    float2 curr_ndc = curr_clip.xy / max(curr_clip.w, 1e-6);
    float2 prev_ndc = prev_clip.xy / max(prev_clip.w, 1e-6);

    // Convert from NDC [-1, 1] to UV [0, 1] screen motion
    return (curr_ndc - prev_ndc) * float2(0.5, -0.5);
}
