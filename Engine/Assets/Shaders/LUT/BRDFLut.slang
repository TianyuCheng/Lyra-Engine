import lyra;
import Common.BSDF;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_lut;

[shader("compute")]
[numthreads(16, 16, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 256 || pixel.y >= 256) {
        return;
    }

    float NoV       = max((float(pixel.x) + 0.5) / 256.0, 1e-4);
    float roughness = max((float(pixel.y) + 0.5) / 256.0, 1e-4);

    float2 integrated = integrate_brdf(NoV, roughness, 1024);

    out_lut[pixel] = float4(integrated.x, integrated.y, 0.0, 1.0);
}
