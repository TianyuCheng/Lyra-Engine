module Mapping;

// references: Geiss 2007 (triplanar mapping), Tatarchuk 2006 (POM), Sigg & Hadwiger 2005 (Catmull-Rom)

// -----------------------------------------------------------------------------
// 1. Triplanar Texture Mapping & UDN Normal Blending
// -----------------------------------------------------------------------------

/**
 * @brief Computes normalized triplanar blending weights along World X, Y, Z axes.
 *
 * @param world_normal Surface geometric normal in world space
 * @param blend_sharpness Power exponent controlling transition sharpness (typically 4.0 .. 8.0)
 * @return Normalized weights (sum(w) == 1.0) for X, Y, Z planes
 */
public float3 triplanar_weights(float3 world_normal, float blend_sharpness = 4.0)
{
    float3 weights = pow(abs(world_normal), float3(blend_sharpness));
    float sum = weights.x + weights.y + weights.z;
    return weights / max(sum, 1e-5);
}

/**
 * @brief Samples a color texture using triplanar projection.
 */
public float4 sample_triplanar_color(
    Texture2D<float4> tex,
    SamplerState      samp,
    float3            world_pos,
    float3            weights,
    float             texture_scale)
{
    float2 uv_x = world_pos.zy * texture_scale;
    float2 uv_y = world_pos.xz * texture_scale;
    float2 uv_z = world_pos.xy * texture_scale;

    float4 col_x = tex.Sample(samp, uv_x);
    float4 col_y = tex.Sample(samp, uv_y);
    float4 col_z = tex.Sample(samp, uv_z);

    return col_x * weights.x + col_y * weights.y + col_z * weights.z;
}

/**
 * @brief Samples and blends normal maps across three planar projections using UDN blending.
 *
 * @param normal_map Tangent-space normal map texture
 * @param samp Linear sampler
 * @param world_pos World position
 * @param world_normal Geometric surface unit normal
 * @param weights Normalized triplanar weights
 * @param texture_scale World to UV scale factor
 * @return Perturbed world-space surface normal unit vector
 */
public float3 sample_triplanar_normal(
    Texture2D<float4> normal_map,
    SamplerState      samp,
    float3            world_pos,
    float3            world_normal,
    float3            weights,
    float             texture_scale)
{
    float2 uv_x = world_pos.zy * texture_scale;
    float2 uv_y = world_pos.xz * texture_scale;
    float2 uv_z = world_pos.xy * texture_scale;

    // unpack tangent normals from [0, 1] to [-1, 1]
    float3 tnormal_x = normal_map.Sample(samp, uv_x).xyz * 2.0 - 1.0;
    float3 tnormal_y = normal_map.Sample(samp, uv_y).xyz * 2.0 - 1.0;
    float3 tnormal_z = normal_map.Sample(samp, uv_z).xyz * 2.0 - 1.0;

    // swizzle projected axes into world orientations with geometric sign correction
    float3 r_x = float3(0.0, tnormal_x.y, tnormal_x.x) * sign(world_normal.x);
    float3 r_y = float3(tnormal_y.x, 0.0, tnormal_y.y) * sign(world_normal.y);
    float3 r_z = float3(tnormal_z.x, tnormal_z.y, 0.0) * sign(world_normal.z);

    // UDN / Whiteout normal blending with geometric normal
    float3 blended = r_x * weights.x + r_y * weights.y + r_z * weights.z + world_normal;
    return normalize(blended);
}

// =============================================================================
// 2. Parallax Occlusion Mapping (POM)
// =============================================================================

/**
 * @brief Parallax Occlusion Mapping ray marcher with linear layer search and secant root finding.
 *
 * @param height_map Texture containing surface displacement height in red channel [0 = bottom, 1 = top]
 * @param samp Bilinear sampler
 * @param tex_coords Base mesh UV coordinates
 * @param view_dir_ts View direction vector in tangent space (pointing towards camera, normalized)
 * @param height_scale Displacement depth scale (e.g. 0.05)
 * @param min_layers Minimum ray marching step count (at perpendicular angles)
 * @param max_layers Maximum ray marching step count (at steep grazing angles)
 * @return Displaced UV coordinates
 */
public float2 parallax_occlusion_mapping(
    Texture2D<float> height_map,
    SamplerState     samp,
    float2           tex_coords,
    float3           view_dir_ts,
    float            height_scale,
    int              min_layers = 8,
    int              max_layers = 32)
{
    // adapt layer count based on view angle: more steps at steep grazing angles
    float num_layers = lerp(float(max_layers), float(min_layers), max(view_dir_ts.z, 0.0));
    float layer_depth = 1.0 / num_layers;
    float current_layer_depth = 0.0;

    // P vector: maximum UV displacement per unit height
    float2 p = view_dir_ts.xy / max(view_dir_ts.z, 0.1) * height_scale;
    float2 delta_tex_coords = p / num_layers;

    float2 current_tex_coords = tex_coords;
    float current_depth_map_value = 1.0 - height_map.Sample(samp, current_tex_coords).r;

    // linear search loop
    while (current_layer_depth < current_depth_map_value) {
        current_tex_coords -= delta_tex_coords;
        current_depth_map_value = 1.0 - height_map.Sample(samp, current_tex_coords).r;
        current_layer_depth += layer_depth;
    }

    // secant root finding interpolation between previous and current step
    float2 prev_tex_coords = current_tex_coords + delta_tex_coords;
    float after_depth  = current_depth_map_value - current_layer_depth;
    float before_depth = (1.0 - height_map.Sample(samp, prev_tex_coords).r) - current_layer_depth + layer_depth;

    float weight = after_depth / (after_depth - before_depth);
    return lerp(current_tex_coords, prev_tex_coords, weight);
}

/**
 * @brief Self-shadowing for Parallax Occlusion Mapping.
 * Marches from surface contact point along the tangent light direction.
 *
 * @param height_map Displacement height map
 * @param samp Bilinear sampler
 * @param initial_coords Displaced surface UVs from POM
 * @param initial_height Ray surface height [0, 1]
 * @param light_dir_ts Light direction vector in tangent space (normalized)
 * @param height_scale Displacement depth scale
 * @param num_samples Shadow march sample count (typically 16)
 * @return Shadow visibility factor [0.0 = shadowed, 1.0 = fully lit]
 */
public float parallax_soft_shadow(
    Texture2D<float> height_map,
    SamplerState     samp,
    float2           initial_coords,
    float            initial_height,
    float3           light_dir_ts,
    float            height_scale,
    int              num_samples = 16)
{
    if (light_dir_ts.z <= 0.0) {
        return 0.0;
    }

    float shadow = 1.0;
    float step_size = 1.0 / float(num_samples);
    float2 delta_uv = (light_dir_ts.xy / max(light_dir_ts.z, 0.1)) * (height_scale / float(num_samples));

    float current_height = initial_height + step_size;
    float2 current_uv = initial_coords + delta_uv;

    while (current_height < 1.0) {
        float sample_height = height_map.Sample(samp, current_uv).r;
        if (sample_height > current_height) {
            float factor = (sample_height - current_height) * (1.0 - current_height);
            shadow = min(shadow, 1.0 - factor * 4.0);
        }
        current_height += step_size;
        current_uv += delta_uv;
    }

    return clamp(shadow, 0.0, 1.0);
}

// =============================================================================
// 3. Fast Bicubic Catmull-Rom 5-Tap Filtering
// =============================================================================

/**
 * @brief 5-tap bicubic Catmull-Rom texture filter.
 * Exploits hardware bilinear filtering to reconstruct sharp cubic interpolation with only 5 texture taps.
 * Reference: Sigg & Hadwiger, GPU Gems 2 Chapter 20.
 */
public float4 sample_bicubic_catmull_rom_5tap(
    Texture2D<float4> tex,
    SamplerState      samp,
    float2            uv,
    float2            tex_size)
{
    float2 sample_pos = uv * tex_size;
    float2 tc = floor(sample_pos - 0.5) + 0.5;
    float2 f = sample_pos - tc;
    float2 f2 = f * f;
    float2 f3 = f2 * f;

    float2 w0 = f2 - 0.5 * (f3 + f);
    float2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    float2 w3 = 0.5 * (f3 - f2);
    float2 w2 = 1.0 - w0 - w1 - w3;

    float2 w12 = w1 + w2;
    float2 tc12 = tc + w2 / w12;

    float2 tc0 = tc - 1.0;
    float2 tc3 = tc + 2.0;

    float2 inv_tex_size = 1.0 / tex_size;

    float4 color = float4(0.0);
    color += tex.SampleLevel(samp, float2(tc12.x, tc0.y) * inv_tex_size, 0.0) * (w12.x * w0.y);
    color += tex.SampleLevel(samp, float2(tc0.x, tc12.y) * inv_tex_size, 0.0) * (w0.x * w12.y);
    color += tex.SampleLevel(samp, float2(tc12.x, tc12.y) * inv_tex_size, 0.0) * (w12.x * w12.y);
    color += tex.SampleLevel(samp, float2(tc3.x, tc12.y) * inv_tex_size, 0.0) * (w3.x * w12.y);
    color += tex.SampleLevel(samp, float2(tc12.x, tc3.y) * inv_tex_size, 0.0) * (w12.x * w3.y);

    return color;
}

/**
 * @brief Computes a 2D procedural checkerboard parity (0 or 1).
 *
 * @param uv Normalized or unnormalized 2D coordinates
 * @param frequency Number of checks across the domain
 * @return 0.0 or 1.0 depending on checker parity
 */
public float checkerboard(float2 uv, float frequency = 1.0)
{
    float2 f = floor(uv * frequency);
    return fmod(f.x + f.y, 2.0);
}
