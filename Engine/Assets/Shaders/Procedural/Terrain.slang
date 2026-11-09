module Terrain;

// references: Musgrave et al. 1989 (fractal terrain), Geiss 2007 (GPU terrain)
import Common.Noise;
import Common.Mapping;

public struct TerrainBiomeConfig
{
    public float water_level;   // height below which is water / sand
    public float grass_level;   // height of vegetation
    public float rock_level;    // height of rock treeline
    public float snow_level;    // height of permanent snow peaks
    public float cliff_slope;   // slope threshold (cos angle) for vertical rock cliffs (e.g. 0.7)
};

/**
 * @brief Default mountainous terrain biome elevation distribution.
 */
public TerrainBiomeConfig default_mountain_biome()
{
    TerrainBiomeConfig c;
    c.water_level = 0.0;
    c.grass_level = 150.0;
    c.rock_level  = 600.0;
    c.snow_level  = 1200.0;
    c.cliff_slope = 0.707; // ~45 degrees
    return c;
}

// =============================================================================
// 1. Multi-Fractal Terrain Elevation
// =============================================================================

/**
 * @brief Ridged multi-fractal noise elevation function for sharp mountain peaks and valleys.
 * Reference: Musgrave et al. (1989).
 *
 * @param xz Horizontal coordinates in world units
 * @param octaves Fractal iteration count (typically 4 .. 8)
 * @param lacunarity Frequency scaling per octave (e.g. 2.0)
 * @param gain Amplitude multiplier per octave (e.g. 0.5)
 * @return Normalized elevation factor [0, 1]
 */
public float evaluate_ridged_elevation(
    float2 xz,
    int    octaves = 6,
    float  lacunarity = 2.0,
    float  gain = 0.5)
{
    float total = 0.0;
    float frequency = 0.001;
    float amplitude = 1.0;
    float weight = 1.0;

    for (int i = 0; i < octaves; ++i) {
        float n = 1.0 - abs(simplex_noise(float3(xz * frequency, 0.0)));
        n = n * n; // sharpen ridges
        total += n * amplitude * weight;
        weight = clamp(n * 2.0, 0.0, 1.0); // signal-dependent damping (valleys are smoother)

        frequency *= lacunarity;
        amplitude *= gain;
    }

    return total;
}

/**
 * @brief Computes analytical terrain surface normal via finite difference gradients.
 *
 * @param xz Horizontal world coordinates
 * @param height_scale Vertical scale multiplier (e.g. 800.0)
 * @param eps Finite difference step size in world units (e.g. 1.0m)
 * @return Surface normal unit vector in world space
 */
public float3 evaluate_terrain_normal(
    float2 xz,
    float  height_scale = 800.0,
    float  eps = 1.0)
{
    float h_c = evaluate_ridged_elevation(xz) * height_scale;
    float h_r = evaluate_ridged_elevation(xz + float2(eps, 0.0)) * height_scale;
    float h_u = evaluate_ridged_elevation(xz + float2(0.0, eps)) * height_scale;

    float3 d_x = float3(eps, h_r - h_c, 0.0);
    float3 d_z = float3(0.0, h_u - h_c, eps);

    return normalize(cross(d_z, d_x));
}

// =============================================================================
// 2. Slope & Altitude Biome Material Splatting
// =============================================================================

public struct TerrainSplatWeights
{
    public float sand;
    public float grass;
    public float cliff;
    public float snow;
};

/**
 * @brief Computes 4-layer material splatting weights based on surface altitude and slope steepness.
 * Ensures cliffs get vertical rock textures regardless of height, preventing texture stretching.
 *
 * @param height Surface world Y elevation
 * @param world_normal Surface normal unit vector
 * @param config Biome transition thresholds
 * @return Normalized splat weights summing to 1.0
 */
public TerrainSplatWeights evaluate_terrain_splat_weights(
    float              height,
    float3             world_normal,
    TerrainBiomeConfig config)
{
    TerrainSplatWeights w;
    w.sand  = 0.0;
    w.grass = 0.0;
    w.cliff = 0.0;
    w.snow  = 0.0;

    // Slope steepness (normal.y = 1.0 is flat horizontal, normal.y = 0.0 is vertical cliff)
    float slope = world_normal.y;
    float cliff_factor = 1.0 - smoothstep(config.cliff_slope - 0.1, config.cliff_slope + 0.1, slope);

    // Altitude layers
    float sand_factor  = 1.0 - smoothstep(config.water_level, config.grass_level, height);
    float grass_factor = smoothstep(config.water_level, config.grass_level, height) *
                         (1.0 - smoothstep(config.rock_level, config.snow_level, height));
    float snow_factor  = smoothstep(config.rock_level + 200.0, config.snow_level, height);

    // Cliff takes priority on steep angles
    w.cliff = cliff_factor;
    float non_cliff = 1.0 - cliff_factor;

    w.sand  = sand_factor * non_cliff;
    w.grass = grass_factor * non_cliff;
    w.snow  = snow_factor * non_cliff;

    // Normalize weights
    float sum = w.sand + w.grass + w.cliff + w.snow;
    if (sum > 0.0) {
        w.sand  /= sum;
        w.grass /= sum;
        w.cliff /= sum;
        w.snow  /= sum;
    } else {
        w.grass = 1.0;
    }

    return w;
}
