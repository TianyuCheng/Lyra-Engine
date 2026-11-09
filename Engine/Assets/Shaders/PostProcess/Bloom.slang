import lyra;
import Common.Colorspace;

struct BloomParams
{
    float2 inv_src_size;  // 1.0 / source_texture_dimensions
    float2 inv_dst_size;  // 1.0 / dest_texture_dimensions
    float4 threshold;     // x: threshold, y: threshold - knee, z: 2*knee, w: 0.25/knee
    float  bloom_intensity;
    float  upsample_radius;
    float2 padding;
};

[[lyra::binding(0, 0)]]
ConstantBuffer<BloomParams> params;

[[lyra::binding(1, 0)]]
Texture2D<float4> src_texture;

[[lyra::binding(2, 0)]]
SamplerState linear_sampler;

[[lyra::binding(3, 0)]]
RWTexture2D<float4> out_texture;

// =============================================================================
// Helper Functions: Karis Anti-Flicker & Quadratic Threshold
// =============================================================================

float karis_weight(float3 c)
{
    float lum = linear_srgb_to_luminance(c);
    return 1.0 / (1.0 + lum);
}

float3 quadratic_threshold(float3 color, float threshold, float3 curve)
{
    // Quadratic soft knee curve from Next Generation Post Processing (Jimenez 2014)
    float br = max(max(color.r, color.g), color.b);
    float rq = clamp(br - curve.x, 0.0, curve.y);
    rq = curve.z * rq * rq;
    return color * max(rq, br - threshold) / max(br, 1e-4);
}

// =============================================================================
// 1. Downsample Pass: 13-Tap Dual-Filter with Karis Weighting (Jimenez 2014)
// =============================================================================

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_downsample(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    uint2 dst_dim;
    out_texture.GetDimensions(dst_dim.x, dst_dim.y);

    if (pixel.x >= dst_dim.x || pixel.y >= dst_dim.y) {
        return;
    }

    float2 uv = (float2(pixel) + 0.5) * params.inv_dst_size;
    float2 d  = params.inv_src_size;

    // 13-tap bilinear pattern
    // Center 4 samples
    float3 a = src_texture.SampleLevel(linear_sampler, uv + float2(-d.x, -d.y), 0.0).rgb;
    float3 b = src_texture.SampleLevel(linear_sampler, uv + float2( d.x, -d.y), 0.0).rgb;
    float3 c = src_texture.SampleLevel(linear_sampler, uv + float2(-d.x,  d.y), 0.0).rgb;
    float3 d_s = src_texture.SampleLevel(linear_sampler, uv + float2( d.x,  d.y), 0.0).rgb;

    // Outer 4 corner samples
    float3 e = src_texture.SampleLevel(linear_sampler, uv + float2(-2.0 * d.x, -2.0 * d.y), 0.0).rgb;
    float3 f = src_texture.SampleLevel(linear_sampler, uv + float2( 2.0 * d.x, -2.0 * d.y), 0.0).rgb;
    float3 g = src_texture.SampleLevel(linear_sampler, uv + float2(-2.0 * d.x,  2.0 * d.y), 0.0).rgb;
    float3 h = src_texture.SampleLevel(linear_sampler, uv + float2( 2.0 * d.x,  2.0 * d.y), 0.0).rgb;

    // Inner 4 cross samples
    float3 i = src_texture.SampleLevel(linear_sampler, uv + float2( 0.0, -2.0 * d.y), 0.0).rgb;
    float3 j = src_texture.SampleLevel(linear_sampler, uv + float2( 0.0,  2.0 * d.y), 0.0).rgb;
    float3 k = src_texture.SampleLevel(linear_sampler, uv + float2(-2.0 * d.x,  0.0), 0.0).rgb;
    float3 l = src_texture.SampleLevel(linear_sampler, uv + float2( 2.0 * d.x,  0.0), 0.0).rgb;

    // Karis weighted partial averages to prevent single-pixel specular shimmer
    float3 group0 = (a + b + c + d_s) * 0.25;
    float3 group1 = (e + f + i + a) * 0.25;
    float3 group2 = (f + h + b + l) * 0.25;
    float3 group3 = (g + e + k + c) * 0.25;
    float3 group4 = (h + g + j + d_s) * 0.25;

    float w0 = karis_weight(group0);
    float w1 = karis_weight(group1);
    float w2 = karis_weight(group2);
    float w3 = karis_weight(group3);
    float w4 = karis_weight(group4);

    float3 downsampled = (group0 * w0 + group1 * w1 + group2 * w2 + group3 * w3 + group4 * w4) /
                         (w0 + w1 + w2 + w3 + w4);

    // Apply bright-pass knee curve if on base mip (threshold.x > 0)
    if (params.threshold.x > 0.0) {
        downsampled = quadratic_threshold(downsampled, params.threshold.x, params.threshold.yzw);
    }

    out_texture[pixel] = float4(downsampled, 1.0);
}

// =============================================================================
// 2. Upsample Pass: 9-Tap 3x3 Tent Filter with Additive Blend
// =============================================================================

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_upsample(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    uint2 dst_dim;
    out_texture.GetDimensions(dst_dim.x, dst_dim.y);

    if (pixel.x >= dst_dim.x || pixel.y >= dst_dim.y) {
        return;
    }

    float2 uv = (float2(pixel) + 0.5) * params.inv_dst_size;
    float2 d  = params.inv_src_size * params.upsample_radius;

    // 9-tap 3x3 tent filter (weights: 4 for center, 2 for cross, 1 for corners, sum = 16)
    float3 a = src_texture.SampleLevel(linear_sampler, uv + float2(-d.x, -d.y), 0.0).rgb;
    float3 b = src_texture.SampleLevel(linear_sampler, uv + float2( 0.0, -d.y), 0.0).rgb * 2.0;
    float3 c = src_texture.SampleLevel(linear_sampler, uv + float2( d.x, -d.y), 0.0).rgb;

    float3 d_s = src_texture.SampleLevel(linear_sampler, uv + float2(-d.x,  0.0), 0.0).rgb * 2.0;
    float3 e   = src_texture.SampleLevel(linear_sampler, uv + float2( 0.0,  0.0), 0.0).rgb * 4.0;
    float3 f   = src_texture.SampleLevel(linear_sampler, uv + float2( d.x,  0.0), 0.0).rgb * 2.0;

    float3 g = src_texture.SampleLevel(linear_sampler, uv + float2(-d.x,  d.y), 0.0).rgb;
    float3 h = src_texture.SampleLevel(linear_sampler, uv + float2( 0.0,  d.y), 0.0).rgb * 2.0;
    float3 i = src_texture.SampleLevel(linear_sampler, uv + float2( d.x,  d.y), 0.0).rgb;

    float3 upsampled = (a + b + c + d_s + e + f + g + h + i) * (1.0 / 16.0);

    // Read previous accumulation at this destination level and additively blend
    float3 current_dst = out_texture[pixel].rgb;
    out_texture[pixel] = float4(current_dst + upsampled * params.bloom_intensity, 1.0);
}
