import lyra;

/**
 * @brief Computes a 2-hemisphere Rayleigh-scattering atmospheric sky gradient.
 *
 * @param v Altitude parameter [0 = zenith, 0.5 = horizon, 1 = nadir]
 * @return RGB color
 */
float3 evaluate_atmospheric_skymap(float v)
{
    float3 zenith_color  = float3(0.18, 0.42, 0.85); // deep atmospheric blue
    float3 horizon_color = float3(0.70, 0.78, 0.90); // warm pale horizon haze
    float3 ground_color  = float3(0.13, 0.13, 0.15); // dark ground at nadir

    if (v <= 0.5) {
        float h = 1.0 - (v * 2.0);
        float t = pow(1.0 - h, 2.2); // Rayleigh optical depth curve
        return lerp(zenith_color, horizon_color, t);
    } else {
        float g = (v - 0.5) * 2.0;
        float t = min(1.0, g * 1.8);
        return lerp(horizon_color, ground_color, t);
    }
}

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_skymap;

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 64 || pixel.y >= 64) {
        return;
    }

    float v = (float(pixel.y) + 0.5) / 64.0;
    float3 color = evaluate_atmospheric_skymap(v);

    out_skymap[pixel] = float4(color, 1.0);
}
