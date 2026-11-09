import lyra;

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

    float3 zenith_color  = float3(0.18, 0.42, 0.85); // deep atmospheric blue
    float3 horizon_color = float3(0.70, 0.78, 0.90); // warm pale horizon haze
    float3 ground_color  = float3(0.13, 0.13, 0.15); // dark ground at nadir

    float3 color;
    if (v <= 0.5) {
        // upper sky hemisphere
        float h = 1.0 - (v * 2.0);
        float t = pow(1.0 - h, 2.2); // Rayleigh optical depth curve
        color = lerp(zenith_color, horizon_color, t);
    } else {
        // lower ground hemisphere
        float g = (v - 0.5) * 2.0;
        float t = min(1.0, g * 1.8);
        color = lerp(horizon_color, ground_color, t);
    }

    out_skymap[pixel] = float4(color, 1.0);
}
