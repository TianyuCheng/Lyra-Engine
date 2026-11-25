module Lighting;

public struct LightData
{
    public float3 position;
    public float  range;

    public float3 direction;
    public float  intensity;

    public float3 color;
    public uint   type; // 0 = directional, 1 = point, 2 = spot

    public float  spot_inner_angle;
    public float  spot_outer_angle;
    public uint   cast_shadows;
    public uint   profile; // IES photometric light profile index (0 = uniform)
};

public struct ClusterRecord
{
    public uint offset;
    public uint count;
};

public struct EvaluatedLight
{
    public float3 direction; // unit vector from surface toward light
    public float3 radiance;  // light.color * light.intensity * atten
};

/**
 * @brief Computes 1D cluster index from screen pixel coordinates and view-space depth Z.
 * Discretizes screen into 16x16 pixel tiles and exponential logarithmic depth slices.
 */
public uint compute_cluster_index(
    float2 screen_pos,
    float  view_depth_z,
    uint3  grid_dim,
    float  near_plane,
    float  far_plane)
{
    uint tile_x = uint(screen_pos.x / 16.0);
    uint tile_y = uint(screen_pos.y / 16.0);

    float linear_z  = max(abs(view_depth_z), near_plane);
    float log_ratio = log(max(far_plane, near_plane + 0.1) / max(near_plane, 0.001));
    float slice_f   = (log(linear_z / max(near_plane, 0.001)) / log_ratio) * float(grid_dim.z);
    uint  tile_z    = uint(clamp(slice_f, 0.0, float(grid_dim.z - 1)));

    uint3 cluster_coord = uint3(
        clamp(tile_x, 0u, grid_dim.x - 1u),
        clamp(tile_y, 0u, grid_dim.y - 1u),
        tile_z
    );

    return cluster_coord.x +
           cluster_coord.y * grid_dim.x +
           cluster_coord.z * grid_dim.x * grid_dim.y;
}

/**
 * @brief Evaluates analytical point or spot light attenuation and incident radiance.
 * @param light Analytical light data.
 * @param world_pos Surface position in world space.
 * @param eval Output evaluated direction and incident radiance.
 * @return True if light contributes non-zero illumination to the surface.
 */
public bool evaluate_analytical_light(LightData light, float3 world_pos, out EvaluatedLight eval)
{
    eval = {};
    if (light.type == 0) {
        return false; // Directional sun is evaluated explicitly
    }

    float3 to_light = light.position - world_pos;
    float dist = length(to_light);
    if (dist >= light.range || dist < 1e-4) {
        return false;
    }

    eval.direction = to_light / dist;
    float norm_dist = dist / light.range;
    float window = saturate(1.0 - norm_dist * norm_dist * norm_dist * norm_dist);
    float atten = (window * window) / (dist * dist + 1.0);

    // Spot light cone attenuation
    if (light.type == 2) {
        float cos_theta = dot(-eval.direction, normalize(light.direction));
        float cos_outer = cos(light.spot_outer_angle);
        float cos_inner = cos(light.spot_inner_angle);
        float spot_factor = saturate((cos_theta - cos_outer) / max(cos_inner - cos_outer, 1e-4));
        atten *= spot_factor * spot_factor;
    }

    eval.radiance = light.color * light.intensity * atten;
    return true;
}

static const float PI = 3.14159265359;
static const float HALF_PI = 1.57079632679;

/**
 * @brief Evaluates an IES photometric light profile texture lookup.
 *
 * @param ies_atlas 2D atlas containing IES profiles (width = angle, height = profiles)
 * @param smp Sampler state (bilinear, clamp)
 * @param light_to_surface Normalized vector from light position to surface position
 * @param light_direction Primary pointing direction vector of the light
 * @param profile_index Profile row index [0..total_profiles-1]
 * @param total_profiles Total rows in atlas (default: 16)
 * @return Normalized luminous intensity factor in [0, 1]
 */
public float evaluate_ies_profile(
    Texture2D<float4> ies_atlas,
    SamplerState      smp,
    float3            light_to_surface,
    float3            light_direction,
    uint              profile_index,
    uint              total_profiles = 16)
{
    float cos_theta = clamp(dot(light_direction, light_to_surface), -1.0, 1.0);
    float theta     = acos(cos_theta);
    float u         = theta / PI;
    float v         = (float(profile_index) + 0.5) / float(total_profiles);
    return ies_atlas.SampleLevel(smp, float2(u, v), 0.0).r;
}

/**
 * @brief Computes normalized luminous intensity for standard photometric profiles given polar angle theta.
 *
 * @param profile Profile index [0..15]
 * @param theta Polar angle from luminaire pointing vector in radians [0..PI]
 * @return Normalized photometric intensity factor in [0, 1]
 */
public float compute_ies_profile(uint profile, float theta)
{
    float cos_theta = cos(theta);

    switch (profile)
    {
    case 0:
        return 1.0;
    case 1:
        return pow(max(cos_theta, 0.0), 128.0);
    case 2:
        return pow(max(cos_theta, 0.0), 32.0);
    case 3:
        return pow(max(cos_theta, 0.0), 8.0);
    case 4:
        return pow(max(cos_theta, 0.0), 2.5);
    case 5:
        return max(cos_theta, 0.0);
    case 6:
        return max(pow(max(cos_theta, 0.0), 8.0), pow(max(-cos_theta, 0.0), 8.0));
    case 7:
        if (theta > HALF_PI) return 0.0;
        return clamp((sin(theta) / (pow(theta - 0.60, 2.0) + 0.15)) * 0.28, 0.0, 1.0);
    case 8:
        return pow(max(-cos_theta, 0.0), 4.0);
    case 9:
        return pow(max(cos_theta, 0.0), 64.0) * smoothstep(PI * 0.25, 0.0, theta);
    case 10:
        if (theta > HALF_PI) return 0.0;
        return pow(max(cos_theta, 0.0), 1.5) * (1.0 + 0.5 * sin(theta * 2.0));
    case 11:
        return clamp(pow(max(cos_theta, 0.0), 64.0) + exp(-pow((theta - 0.5) / 0.08, 2.0)) * 0.35, 0.0, 1.0);
    case 12:
        return pow(max(cos_theta, 0.0), 1.2);
    case 13:
        return 0.2 + 0.8 * smoothstep(PI * 0.9, PI * 0.2, theta);
    case 14:
        return pow(max(-cos_theta, 0.0), 2.0) * (0.5 + 0.5 * sin(theta));
    case 15:
    default:
        return pow(max(cos_theta, 0.0), 16.0);
    }
}

/**
 * @brief Analytical evaluation of standard IES luminaire profiles (zero texture bandwidth).
 */
public float evaluate_ies_analytical(uint profile, float3 light_to_surface, float3 light_direction)
{
    float cos_theta = clamp(dot(light_direction, light_to_surface), -1.0, 1.0);
    float theta     = acos(cos_theta);
    return compute_ies_profile(profile, theta);
}
