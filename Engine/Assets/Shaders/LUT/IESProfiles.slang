import lyra;
import Common.Lighting;

static const float PI = 3.14159265358979323846;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_ies_atlas;

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
