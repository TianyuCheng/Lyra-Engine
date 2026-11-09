import lyra;

static const float PI = 3.14159265358979323846;
static const float HALF_PI = 1.57079632679489661923;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_ies_atlas;

/**
 * @brief Computes normalized luminous intensity for standard photometric profiles.
 *
 * @param profile Profile index [0..15]
 * @param theta Polar angle from luminaire pointing vector in radians [0..PI]
 * @return Normalized photometric intensity factor [0..1]
 */
float compute_ies_profile(uint profile, float theta)
{
    float cos_theta = cos(theta);

    switch (profile)
    {
    case 0:
        // 0: Isotropic / Point Light (Uniform 360-degree sphere)
        return 1.0;

    case 1:
        // 1: Narrow Spot (15-degree beam angle, high peak candela)
        return pow(max(cos_theta, 0.0), 128.0);

    case 2:
        // 2: Medium Spot (30-degree beam angle)
        return pow(max(cos_theta, 0.0), 32.0);

    case 3:
        // 3: Downlight / Recessed Can (60-degree beam angle with soft falloff)
        return pow(max(cos_theta, 0.0), 8.0);

    case 4:
        // 4: Wide Flood (90-degree broad luminaire)
        return pow(max(cos_theta, 0.0), 2.5);

    case 5:
        // 5: Hemispherical Diffuse (Cosine / Lambertian emitter)
        return max(cos_theta, 0.0);

    case 6:
        // 6: Bi-directional / Architectural Wall Sconce (Up & Down lobes)
        return max(pow(max(cos_theta, 0.0), 8.0), pow(max(-cos_theta, 0.0), 8.0));

    case 7:
        // 7: Batwing (Commercial office / Classroom troffer to reduce direct glare)
        // Peak intensity is angled outwards (~35 degrees / 0.6 rad) rather than directly below
        if (theta > HALF_PI) return 0.0;
        float batwing = sin(theta) / (pow(theta - 0.60, 2.0) + 0.15);
        return clamp(batwing * 0.28, 0.0, 1.0);

    case 8:
        // 8: Torchiere / Indirect Uplight (Ceiling bounce)
        return pow(max(-cos_theta, 0.0), 4.0);

    case 9:
        // 9: Grazing Light (Sharp asymmetric falloff for textured walls)
        return pow(max(cos_theta, 0.0), 64.0) * smoothstep(PI * 0.25, 0.0, theta);

    case 10:
        // 10: Streetlight Type II (Medium lateral spread)
        if (theta > HALF_PI) return 0.0;
        return pow(max(cos_theta, 0.0), 1.5) * (1.0 + 0.5 * sin(theta * 2.0));

    case 11:
        // 11: Parabolic Reflector (Concentrated center with wide halo ring)
        float center_beam = pow(max(cos_theta, 0.0), 64.0);
        float ring = exp(-pow((theta - 0.5) / 0.08, 2.0)) * 0.35;
        return clamp(center_beam + ring, 0.0, 1.0);

    case 12:
        // 12: Soft Box / Diffused Studio Light
        return pow(max(cos_theta, 0.0), 1.2);

    case 13:
        // 13: Candelabra / Decorative Flame (Slight downward shadow from base)
        return 0.2 + 0.8 * smoothstep(PI * 0.9, PI * 0.2, theta);

    case 14:
        // 14: Linear Cove Lighting (Asymmetric indirect upward wash)
        return pow(max(-cos_theta, 0.0), 2.0) * (0.5 + 0.5 * sin(theta));

    case 15:
    default:
        // 15: Oval Accent Beam
        return pow(max(cos_theta, 0.0), 16.0);
    }
}

[shader("compute")]
[numthreads(16, 16, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 256 || pixel.y >= 16) {
        return;
    }

    // u corresponds to polar angle theta from 0 to PI
    float u = (float(pixel.x) + 0.5) / 256.0;
    float theta = u * PI;
    uint profile = pixel.y;

    float intensity = compute_ies_profile(profile, theta);

    out_ies_atlas[pixel] = float4(intensity, intensity, intensity, 1.0);
}
