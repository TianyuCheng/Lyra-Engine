module Denoising;

static const float EPSILON = 1e-6;

// =============================================================================
// 1. Color Space Representations for Denoising (YCoCg)
// =============================================================================

/**
 * @brief Converts Linear RGB to YCoCg (Luminance, Chrominance Orange, Chrominance Green).
 * De-correlates color channels to prevent color fringing in TAA and spatial filtering.
 */
public float3 rgb_to_ycocg(float3 rgb)
{
    float y  =  0.25 * rgb.r + 0.50 * rgb.g + 0.25 * rgb.b;
    float co =  0.50 * rgb.r                - 0.50 * rgb.b;
    float cg = -0.25 * rgb.r + 0.50 * rgb.g - 0.25 * rgb.b;
    return float3(y, co, cg);
}

/**
 * @brief Converts YCoCg back to Linear RGB.
 */
public float3 ycocg_to_rgb(float3 ycocg)
{
    float y  = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;
    return float3(
        y + co - cg,
        y + cg,
        y - co - cg
    );
}

// =============================================================================
// 2. Edge-Stopping Functions (Joint Bilateral & SVGF)
// =============================================================================

/**
 * @brief Depth edge-stopping weight using relative distance.
 */
public float edge_stopping_depth(float depth_center, float depth_sample, float sigma_z = 1.0)
{
    return exp(-abs(depth_center - depth_sample) / max(sigma_z * abs(depth_center), 1e-4));
}

/**
 * @brief Normal edge-stopping weight using cosine power.
 */
public float edge_stopping_normal(float3 normal_center, float3 normal_sample, float sigma_n = 32.0)
{
    float cos_angle = max(dot(normal_center, normal_sample), 0.0);
    return pow(cos_angle, sigma_n);
}

/**
 * @brief Luminance edge-stopping weight scaled by local variance (SVGF formulation).
 */
public float edge_stopping_luminance(float lum_center, float lum_sample, float variance, float sigma_l = 4.0)
{
    float std_dev = sqrt(max(variance, 0.0));
    return exp(-abs(lum_center - lum_sample) / max(sigma_l * std_dev + 1e-4, 1e-4));
}

/**
 * @brief Roughness edge-stopping weight (avoids blurring across sharp specular transitions).
 */
public float edge_stopping_roughness(float rough_center, float rough_sample, float sigma_r = 0.2)
{
    return exp(-abs(rough_center - rough_sample) / max(sigma_r, 1e-3));
}

/**
 * @brief Combined joint bilateral edge-stopping weight for cross-filtering.
 */
public float edge_stopping_weight(
    float  depth_center,   float  depth_sample,
    float3 normal_center,  float3 normal_sample,
    float  lum_center,     float  lum_sample,
    float  variance,
    float  sigma_z = 1.0,
    float  sigma_n = 32.0,
    float  sigma_l = 4.0)
{
    float wz = edge_stopping_depth(depth_center, depth_sample, sigma_z);
    float wn = edge_stopping_normal(normal_center, normal_sample, sigma_n);
    float wl = edge_stopping_luminance(lum_center, lum_sample, variance, sigma_l);
    return wz * wn * wl;
}

// =============================================================================
// 3. À-Trous Wavelet Filter Kernel (B3-Spline)
// =============================================================================

// 1D cubic B-spline weights at offsets [-2, -1, 0, 1, 2]
static const float ATROUS_KERNEL_1D[5] = { 1.0 / 16.0, 1.0 / 4.0, 3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0 };

/**
 * @brief Evaluates 2D separable À-Trous B3-spline filter weight at integer offsets (-2..2, -2..2).
 */
public float atrous_kernel_weight(int x, int y)
{
    return ATROUS_KERNEL_1D[x + 2] * ATROUS_KERNEL_1D[y + 2];
}

// =============================================================================
// 4. Temporal Reprojection, Disocclusion & Accumulation
// =============================================================================

/**
 * @brief Detects disocclusion between current and reprojected historical samples.
 * @return True if disoccluded (geometry moved/revealed), false if history is valid.
 */
public bool is_disoccluded(
    float  curr_depth, float  prev_depth,
    float3 curr_normal, float3 prev_normal,
    float  depth_rel_threshold = 0.03,
    float  normal_threshold = 0.85)
{
    float depth_diff = abs(curr_depth - prev_depth) / max(curr_depth, 1e-4);
    if (depth_diff > depth_rel_threshold) return true;
    if (dot(curr_normal, prev_normal) < normal_threshold) return true;
    return false;
}

/**
 * @brief Blends current sample with historical accumulation using dynamic alpha based on history length.
 */
public float3 accumulate_temporal(
    float3 history,
    float3 current,
    float  history_length,
    float  max_history = 32.0,
    float  min_alpha = 0.05)
{
    float clamped_len = clamp(history_length, 1.0, max_history);
    float alpha = max(1.0 / clamped_len, min_alpha);
    return lerp(history, current, alpha);
}

public float accumulate_temporal_scalar(
    float history,
    float current,
    float history_length,
    float max_history = 32.0,
    float min_alpha = 0.05)
{
    float clamped_len = clamp(history_length, 1.0, max_history);
    float alpha = max(1.0 / clamped_len, min_alpha);
    return lerp(history, current, alpha);
}

// =============================================================================
// 5. TAA Color Clamping & Variance Clipping
// =============================================================================

/**
 * @brief Simple axis-aligned bounding box (AABB) clamping.
 */
public float3 clip_to_aabb(float3 history, float3 aabb_min, float3 aabb_max)
{
    return clamp(history, aabb_min, aabb_max);
}

/**
 * @brief Variance clipping in YCoCg space (Salvi 2016 / Karis 2014).
 * Intersects history vector with a statistical bounding box [mean - gamma * stddev, mean + gamma * stddev].
 */
public float3 clip_variance_aabb(float3 history_ycocg, float3 mean_ycocg, float3 stddev_ycocg, float gamma = 1.25)
{
    float3 aabb_min = mean_ycocg - gamma * stddev_ycocg;
    float3 aabb_max = mean_ycocg + gamma * stddev_ycocg;

    float3 p_clip = 0.5 * (aabb_max + aabb_min);
    float3 e_clip = 0.5 * (aabb_max - aabb_min) + 1e-5;

    float3 v_clip = history_ycocg - p_clip;
    float3 v_unit = v_clip / e_clip;
    float3 a_unit = abs(v_unit);
    float  max_elem = max(max(a_unit.x, a_unit.y), a_unit.z);

    if (max_elem > 1.0) {
        return p_clip + v_clip / max_elem;
    }
    return history_ycocg;
}

// =============================================================================
// 6. Firefly Outlier Suppression
// =============================================================================

/**
 * @brief Suppresses extreme Monte Carlo specular fireflies based on local neighborhood statistics.
 */
public float3 suppress_fireflies(float3 color, float3 local_mean, float3 local_stddev, float max_sigma = 3.0)
{
    float3 max_allowed = local_mean + max_sigma * local_stddev;
    return min(color, max_allowed);
}

// =============================================================================
// 7. Spatial & Temporal Variance Estimation
// =============================================================================

/**
 * @brief Estimates local variance from first and second moments: Var(X) = E[X^2] - (E[X])^2.
 */
public float compute_variance(float moment1, float moment2)
{
    return max(0.0, moment2 - moment1 * moment1);
}

public float3 compute_variance_color(float3 moment1, float3 moment2)
{
    return max(float3(0.0), moment2 - moment1 * moment1);
}
