module Shadows;

// references: Fernando 2005 (PCSS), Reeves 1987 (PCF), Engel 2006 (CSM)
static const float PI = 3.14159265358979323846;

// 16-sample Vogel / Poisson disk distribution for uniform circular sampling
static const float2 POISSON_DISK_16[16] = {
    float2(-0.94201624, -0.39906216),
    float2( 0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870),
    float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845),
    float2( 0.97484398,  0.75648379),
    float2( 0.44323325, -0.97511554),
    float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2( 0.79197514,  0.19090188),
    float2(-0.24188840,  0.99706507),
    float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367),
    float2( 0.14383161, -0.14100790)
};

// =============================================================================
// 1. Geometric Shadow Bias Formulations
// =============================================================================

/**
 * @brief Computes geometry-aware receiver position offset along the surface normal.
 * Eliminates shadow acne on steep grazing angles without detaching contact shadows (peter-panning).
 * Formula: p' = p + n * bias * (1 - max(0, n . l))
 *
 * @param world_pos Surface world position
 * @param normal Geometric unit surface normal
 * @param light_dir Direction pointing towards light source
 * @param bias_scale Base offset bias in world units (e.g. 0.01 .. 0.05)
 * @return Biased world position to transform into shadow clip space
 */
public float3 apply_normal_offset_bias(
    float3 world_pos,
    float3 normal,
    float3 light_dir,
    float  bias_scale)
{
    float NoL = max(dot(normal, light_dir), 0.0);
    float normal_offset = bias_scale * (1.0 - NoL);
    return world_pos + normal * normal_offset;
}

/**
 * @brief Slope-scaled depth bias to prevent self-shadowing on steep geometric slopes.
 *
 * @param NoL Cosine of incident light angle
 * @param min_bias Baseline constant depth bias
 * @param max_bias Maximum clamped depth bias
 * @return Slope-scaled depth bias to subtract from receiver depth
 */
public float compute_slope_scaled_bias(
    float NoL,
    float min_bias = 0.0005,
    float max_bias = 0.005)
{
    float cos_theta = max(NoL, 0.001);
    float tan_theta = sqrt(1.0 - cos_theta * cos_theta) / cos_theta;
    return clamp(min_bias * tan_theta, min_bias, max_bias);
}

// =============================================================================
// 2. Percentage-Closer Filtering (PCF)
// =============================================================================

/**
 * @brief 16-sample Poisson disk PCF filter with per-pixel rotation.
 *
 * @param shadow_map Shadow depth texture
 * @param shadow_sampler Hardware comparison sampler
 * @param shadow_coord UV coordinates and receiver depth in shadow map space (XY = [0,1], Z = depth)
 * @param filter_radius Filter footprint in UV units (e.g. radius / map_resolution)
 * @param rotation_angle Random per-pixel jitter angle [0, 2*PI]
 * @return Filtered shadow visibility scalar [0.0 = occluded, 1.0 = lit]
 */
public float sample_shadow_pcf_poisson_16(
    Texture2D<float>       shadow_map,
    SamplerComparisonState shadow_sampler,
    float3                 shadow_coord,
    float                  filter_radius,
    float                  rotation_angle = 0.0)
{
    float cos_r = cos(rotation_angle);
    float sin_r = sin(rotation_angle);
    float2x2 rot = float2x2(cos_r, -sin_r, sin_r, cos_r);

    float shadow = 0.0;
    for (int i = 0; i < 16; ++i) {
        float2 offset = mul(rot, POISSON_DISK_16[i]) * filter_radius;
        shadow += shadow_map.SampleCmpLevelZero(
            shadow_sampler,
            shadow_coord.xy + offset,
            shadow_coord.z);
    }
    return shadow / 16.0;
}

/**
 * @brief 9-tap 3x3 uniform box PCF filter.
 */
public float sample_shadow_pcf_3x3(
    Texture2D<float>       shadow_map,
    SamplerComparisonState shadow_sampler,
    float3                 shadow_coord,
    float2                 texel_size)
{
    float shadow = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float2 offset = float2(float(x), float(y)) * texel_size;
            shadow += shadow_map.SampleCmpLevelZero(
                shadow_sampler,
                shadow_coord.xy + offset,
                shadow_coord.z);
        }
    }
    return shadow / 9.0;
}

// =============================================================================
// 3. Percentage-Closer Soft Shadows (PCSS - Fernando 2005)
// =============================================================================

public struct BlockerSearchResult
{
    public float avg_blocker_depth;
    public int   num_blockers;
};

/**
 * @brief Step 1: Searches neighborhood around receiver to find average occluder/blocker distance.
 *
 * @param shadow_map Raw depth texture
 * @param point_sampler Point/clamp sampler
 * @param uv Receiver projected shadow UV
 * @param receiver_depth Receiver linear depth in shadow space
 * @param search_radius Neighborhood search radius in UV units
 */
public BlockerSearchResult pcss_blocker_search(
    Texture2D<float> shadow_map,
    SamplerState     point_sampler,
    float2           uv,
    float            receiver_depth,
    float            search_radius,
    float            rotation_angle = 0.0)
{
    BlockerSearchResult result;
    result.avg_blocker_depth = 0.0;
    result.num_blockers = 0;

    float cos_r = cos(rotation_angle);
    float sin_r = sin(rotation_angle);
    float2x2 rot = float2x2(cos_r, -sin_r, sin_r, cos_r);

    for (int i = 0; i < 16; ++i) {
        float2 offset = mul(rot, POISSON_DISK_16[i]) * search_radius;
        float blocker_depth = shadow_map.SampleLevel(point_sampler, uv + offset, 0.0).r;
        if (blocker_depth < receiver_depth) {
            result.avg_blocker_depth += blocker_depth;
            result.num_blockers++;
        }
    }

    if (result.num_blockers > 0) {
        result.avg_blocker_depth /= float(result.num_blockers);
    }
    return result;
}

/**
 * @brief Step 2: Computes physical penumbra radius from receiver and average blocker depths.
 * Formula: w_penumbra = (d_receiver - d_blocker) / d_blocker * light_size
 *
 * @param receiver_depth Depth of surface receiver
 * @param avg_blocker_depth Average depth of occluders found in Step 1
 * @param light_size Virtual light source radius in shadow projection units
 * @return Filter radius in shadow UV space
 */
public float pcss_penumbra_size(
    float receiver_depth,
    float avg_blocker_depth,
    float light_size)
{
    float penumbra = (receiver_depth - avg_blocker_depth) * light_size / max(avg_blocker_depth, 1e-4);
    return max(penumbra, 0.0);
}

/**
 * @brief Complete Percentage-Closer Soft Shadows (PCSS) evaluation pass.
 *
 * @param shadow_map Shadow depth texture
 * @param cmp_sampler Comparison sampler for PCF filtering
 * @param point_sampler Point sampler for blocker depth search
 * @param shadow_coord Projected shadow coordinate (XY = UV, Z = receiver depth)
 * @param light_size Virtual light source radius (e.g. 0.02)
 * @param max_filter_radius Maximum penumbra blur cap
 * @param rotation_angle Random per-pixel rotation
 * @return Physically soft shadow visibility [0.0 = occluded, 1.0 = fully lit]
 */
public float sample_shadow_pcss(
    Texture2D<float>       shadow_map,
    SamplerComparisonState cmp_sampler,
    SamplerState           point_sampler,
    float3                 shadow_coord,
    float                  light_size,
    float                  max_filter_radius = 0.05,
    float                  rotation_angle = 0.0)
{
    // Step 1: Blocker Search
    float search_radius = light_size * (shadow_coord.z - 0.01) / max(shadow_coord.z, 0.01);
    search_radius = clamp(search_radius, 0.001, max_filter_radius);

    BlockerSearchResult blocker = pcss_blocker_search(
        shadow_map,
        point_sampler,
        shadow_coord.xy,
        shadow_coord.z,
        search_radius,
        rotation_angle);

    // Early exit: no blockers -> fully lit
    if (blocker.num_blockers == 0) {
        return 1.0;
    }

    // Step 2: Penumbra Size Estimation
    float penumbra_radius = pcss_penumbra_size(
        shadow_coord.z,
        blocker.avg_blocker_depth,
        light_size);
    penumbra_radius = clamp(penumbra_radius, 0.001, max_filter_radius);

    // Step 3: Variable-Radius PCF Filtering
    return sample_shadow_pcf_poisson_16(
        shadow_map,
        cmp_sampler,
        shadow_coord,
        penumbra_radius,
        rotation_angle);
}

// =============================================================================
// 4. Cascaded Shadow Maps (CSM) Cascade Selection & Blend Bands
// =============================================================================

/**
 * @brief Selects active shadow cascade from view-space linear depth with smooth dithered split blending.
 *
 * @param view_depth Linear view distance along camera forward axis (Z > 0)
 * @param cascade_splits Split distance boundaries for 4 cascades (e.g. float4(10, 30, 100, 300))
 * @param blend_band_fraction Percentage of cascade border used for cross-fading (e.g. 0.15 = 15%)
 * @param out_cascade_index Output index of primary cascade [0..3]
 * @param out_blend_factor Output blend factor [0..1] for smooth blending into next cascade
 */
public void select_csm_cascade(
    float      view_depth,
    float4     cascade_splits,
    float      blend_band_fraction,
    out int    out_cascade_index,
    out float  out_blend_factor)
{
    out_cascade_index = 3;
    out_blend_factor  = 0.0;

    float prev_split = 0.0;
    for (int i = 0; i < 4; ++i) {
        float split = cascade_splits[i];
        if (view_depth < split) {
            out_cascade_index = i;
            float cascade_range = split - prev_split;
            float blend_threshold = split - cascade_range * blend_band_fraction;
            if (view_depth > blend_threshold && i < 3) {
                out_blend_factor = (view_depth - blend_threshold) / (split - blend_threshold);
            }
            return;
        }
        prev_split = split;
    }
}
