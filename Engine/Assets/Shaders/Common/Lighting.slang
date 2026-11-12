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
