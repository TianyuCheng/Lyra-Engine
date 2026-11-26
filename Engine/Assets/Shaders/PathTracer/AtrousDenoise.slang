import lyra;
import Common.Denoising;

// 5x5 A-Trous Wavelet Filter compute pass with edge-stopping functions and variance estimation
public struct AtrousPushConstants
{
    public uint  width;
    public uint  height;
    public int   step_size; // 1, 2, 4, 8, 16 (2^i)
    public float sigma_l;   // luminance edge-stopping weight scale (default ~4.0)
    public float sigma_n;   // normal edge-stopping exponent (default ~32.0)
    public float sigma_z;   // depth edge-stopping scale (default ~1.0)
    public uint  iteration; // iteration index 0..N-1
    public uint  reserved;
};

[[lyra::immediates]]
ConstantBuffer<AtrousPushConstants> push;

[[lyra::binding(0, 0)]]
Texture2D<float4> input_texture;

[[lyra::binding(1, 0)]]
RWTexture2D<float4> output_texture;

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_atrous(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_id.xy;
    if (pixel.x >= push.width || pixel.y >= push.height) return;

    float4 center_val   = input_texture[pixel];
    float3 center_rgb   = center_val.rgb;
    float3 center_ycocg = rgb_to_ycocg(center_rgb);
    float  center_lum   = center_ycocg.x;

    // don't filter background / empty space
    if (center_lum <= 1e-6) {
        output_texture[pixel] = center_val;
        return;
    }

    // estimate local luminance variance over the 3x3 local neighborhood
    float sum_lum      = 0.0;
    float sum_lum_sq   = 0.0;
    float sample_count = 0.0;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int2 tap_pos   = clamp(int2(pixel) + int2(dx, dy), int2(0, 0), int2(push.width - 1, push.height - 1));
            float3 tap_rgb = input_texture[tap_pos].rgb;
            float tap_lum  = rgb_to_ycocg(tap_rgb).x;
            sum_lum       += tap_lum;
            sum_lum_sq    += tap_lum * tap_lum;
            sample_count  += 1.0;
        }
    }

    float mean_lum = sum_lum / max(sample_count, 1.0);
    float var_lum  = max(0.0, (sum_lum_sq / max(sample_count, 1.0)) - mean_lum * mean_lum);

    // filter using 5x5 B3-spline kernel and edge-stopping weights
    float3 accum_color  = float3(0.0);
    float  accum_weight = 0.0;

    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            int2 tap_pos   = clamp(int2(pixel) + int2(dx, dy) * push.step_size, int2(0, 0), int2(push.width - 1, push.height - 1));
            float4 tap_val = input_texture[tap_pos];
            float3 tap_rgb = tap_val.rgb;
            float  tap_lum = rgb_to_ycocg(tap_rgb).x;

            if (tap_lum <= 1e-6) continue;

            float ker_w = atrous_kernel_weight(dx, dy);
            float w_l   = edge_stopping_luminance(center_lum, tap_lum, var_lum, push.sigma_l);
            float w     = ker_w * w_l;

            accum_color  += tap_rgb * w;
            accum_weight += w;
        }
    }

    float3 final_color    = accum_color / max(accum_weight, 1e-5);
    output_texture[pixel] = float4(final_color, center_val.a);
}
