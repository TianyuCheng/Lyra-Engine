module ScreenSpace;

// references: McGuire & Mara 2014 (screen-space ray tracing), Jimenez et al. 2016 (GTAO)
static const float PI = 3.14159265358979323846;

// -----------------------------------------------------------------------------
// 1. View-Space Position Reconstruction & Screen Helpers
// -----------------------------------------------------------------------------

/**
 * @brief Reconstructs view-space 3D position from screen UV and linear view depth.
 *
 * @param uv Normalized screen UV [0, 1]
 * @param linear_depth Linear depth along camera forward Z axis (Z > 0)
 * @param inv_proj_scale Scale factor = 1.0 / float2(proj[0][0], proj[1][1])
 * @return View-space position float3(X, Y, Z)
 */
public float3 reconstruct_view_position(
    float2 uv,
    float  linear_depth,
    float2 inv_proj_scale)
{
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y; // invert Y for standard top-left screen coordinates
    return float3(ndc * inv_proj_scale * linear_depth, linear_depth);
}

/**
 * @brief Computes a smooth boundary vignette attenuation factor to eliminate screen border reflection seams.
 *
 * @param uv Normalized screen UV [0, 1]
 * @param fade_start Normalized border distance where fade begins (e.g. 0.85)
 * @return Attenuation scalar [0.0 at edge, 1.0 at center]
 */
public float screen_edge_fade(float2 uv, float fade_start = 0.85)
{
    float2 border = smoothstep(float2(0.0, 0.0), float2(1.0 - fade_start, 1.0 - fade_start), min(uv, 1.0 - uv));
    return border.x * border.y;
}

// =============================================================================
// 2. Morgan McGuire Screen-Space Ray Marching (SSR)
// =============================================================================

public struct SSRResult
{
    public float2 hit_uv;
    public float3 hit_point_vs;
    public bool   hit;
    public float  confidence;
};

/**
 * @brief Morgan McGuire 2D screen-space ray tracing algorithm.
 * Marches through screen UV coordinates with adaptive step size and binary refinement.
 *
 * @param depth_texture Linear view-space depth texture
 * @param samp Point/clamp sampler
 * @param ray_origin_vs Ray start position in view space
 * @param ray_dir_vs Ray direction unit vector in view space
 * @param view_to_screen_matrix Projection matrix mapping view space to screen UV [0, 1]
 * @param max_distance Maximum ray tracing distance in world/view units
 * @param num_steps Maximum linear ray marching steps (e.g. 32 .. 64)
 * @param thickness Surface thickness tolerance to avoid ray stepping through occluded background
 * @param jitter Random per-pixel stride jitter [0, 1]
 */
public SSRResult trace_screen_ray(
    Texture2D<float> depth_texture,
    SamplerState     samp,
    float3           ray_origin_vs,
    float3           ray_dir_vs,
    float4x4         view_to_screen_matrix,
    float            max_distance = 20.0,
    int              num_steps = 32,
    float            thickness = 0.5,
    float            jitter = 0.0)
{
    SSRResult result;
    result.hit = false;
    result.hit_uv = float2(0.0);
    result.hit_point_vs = float3(0.0);
    result.confidence = 0.0;

    float3 ray_end_vs = ray_origin_vs + ray_dir_vs * max_distance;

    // project endpoints to screen space
    float4 p0_clip = mul(view_to_screen_matrix, float4(ray_origin_vs, 1.0));
    float4 p1_clip = mul(view_to_screen_matrix, float4(ray_end_vs, 1.0));

    float3 p0_screen = p0_clip.xyz / p0_clip.w;
    float3 p1_screen = p1_clip.xyz / p1_clip.w;

    float3 delta_screen = (p1_screen - p0_screen) / float(num_steps);
    float3 current_pos = p0_screen + delta_screen * (1.0 + jitter);

    for (int i = 0; i < num_steps; ++i) {
        // check out of screen bounds
        if (any(current_pos.xy < 0.0) || any(current_pos.xy > 1.0)) {
            break;
        }

        float scene_depth = depth_texture.SampleLevel(samp, current_pos.xy, 0.0).r;
        float ray_depth   = current_pos.z;

        // check if ray depth is behind surface within surface thickness
        if (ray_depth >= scene_depth && ray_depth <= scene_depth + thickness) {
            // hit detected! Binary search refinement
            float3 start = current_pos - delta_screen;
            float3 end   = current_pos;

            for (int b = 0; b < 4; ++b) {
                float3 mid = (start + end) * 0.5;
                float d = depth_texture.SampleLevel(samp, mid.xy, 0.0).r;
                if (mid.z >= d) {
                    end = mid;
                } else {
                    start = mid;
                }
            }

            result.hit = true;
            result.hit_uv = end.xy;
            result.confidence = screen_edge_fade(end.xy);
            return result;
        }

        current_pos += delta_screen;
    }

    return result;
}

// =============================================================================
// 3. Ground Truth Ambient Occlusion (GTAO - Jimenez 2016)
// =============================================================================

/**
 * @brief Fast inner loop for GTAO horizon integration along a screen-space projection slice.
 * Computes projected horizon angles h1 and h2 and integrates the clamped cosine lobe:
 * AO = 0.5 * (cos(theta - 2*h1) + cos(theta - 2*h2))
 *
 * @param h1 Minimum clamped horizon angle in radians
 * @param h2 Maximum clamped horizon angle in radians
 * @param n_proj Projected surface normal angle in slice plane
 * @return Ambient occlusion visibility contribution [0..1]
 */
public float gtao_integrate_arc(float h1, float h2, float n_proj)
{
    return 0.25 * (
        -cos(2.0 * h1 - n_proj) + cos(n_proj) + 2.0 * h1 * sin(n_proj) -
         cos(2.0 * h2 - n_proj) + cos(n_proj) + 2.0 * h2 * sin(n_proj));
}
