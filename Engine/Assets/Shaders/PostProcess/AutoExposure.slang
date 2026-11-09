import lyra;
import Common.Colorspace;
import Common.Camera;

static const float EPSILON = 1e-6;

// 64-bin histogram configuration
static const uint  HISTOGRAM_BINS = 64;
static const float MIN_LOG_LUM    = -8.0;  // 2^-8  ~ 0.0039 nits (starlight / dark cave)
static const float MAX_LOG_LUM    = 16.0;  // 2^16  ~ 65536 nits (bright sunlight)
static const float LOG_LUM_RANGE  = MAX_LOG_LUM - MIN_LOG_LUM;

struct AutoExposureParams
{
    float min_ev;
    float max_ev;
    float speed_up;
    float speed_down;
    float low_percent;
    float high_percent;
    float delta_time;
    uint  pixel_count;
};

[[lyra::binding(0, 0)]]
ConstantBuffer<AutoExposureParams> params;

[[lyra::binding(1, 0)]]
Texture2D<float4> hdr_scene_color;

[[lyra::binding(2, 0)]]
RWStructuredBuffer<uint> histogram_buffer;

// =============================================================================
// Pass 1: Build 64-Bin Luminance Histogram
// =============================================================================

groupshared uint s_histogram[HISTOGRAM_BINS];

uint luminance_to_bin(float lum)
{
    if (lum < 1e-4) {
        return 0;
    }
    float log_lum = clamp((log2(lum) - MIN_LOG_LUM) / LOG_LUM_RANGE, 0.0, 1.0);
    return uint(log_lum * float(HISTOGRAM_BINS - 1) + 0.5);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_build_histogram(
    uint3 dispatch_thread_id : SV_DispatchThreadID,
    uint  group_index        : SV_GroupIndex)
{
    // Initialize shared histogram to zero
    if (group_index < HISTOGRAM_BINS) {
        s_histogram[group_index] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 tex_dim;
    hdr_scene_color.GetDimensions(tex_dim.x, tex_dim.y);

    if (dispatch_thread_id.x < tex_dim.x && dispatch_thread_id.y < tex_dim.y) {
        float3 rgb = hdr_scene_color[dispatch_thread_id.xy].rgb;
        // Rec.709 perceived luminance
        float lum = linear_srgb_to_luminance(rgb);
        uint bin = luminance_to_bin(lum);
        InterlockedAdd(s_histogram[bin], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    // Accumulate local histogram into global buffer
    if (group_index < HISTOGRAM_BINS) {
        InterlockedAdd(histogram_buffer[group_index], s_histogram[group_index]);
    }
}

// =============================================================================
// Pass 2: Temporal Eye Adaptation Integration
// =============================================================================

[[lyra::binding(3, 0)]]
RWTexture2D<float4> out_exposure_data; // 1x1 buffer: { exposure_scale, ev100, avg_luminance, 1.0 }

float bin_to_luminance(float bin)
{
    float log_lum = (bin / float(HISTOGRAM_BINS - 1)) * LOG_LUM_RANGE + MIN_LOG_LUM;
    return exp2(log_lum);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_adapt_exposure(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint thread_idx = dispatch_thread_id.x;

    // Use single thread 0 to compute weighted average and temporal adaptation
    if (thread_idx != 0) {
        return;
    }

    float total_pixels = float(params.pixel_count);
    float min_pixel_threshold = total_pixels * params.low_percent;
    float max_pixel_threshold = total_pixels * params.high_percent;

    float pixel_sum = 0.0;
    float weighted_sum = 0.0;
    float valid_pixel_count = 0.0;

    for (uint i = 0; i < HISTOGRAM_BINS; ++i) {
        float bin_count = float(histogram_buffer[i]);
        histogram_buffer[i] = 0; // Clear for next frame

        float prev_sum = pixel_sum;
        pixel_sum += bin_count;

        // Skip outliers outside low_percent and high_percent
        if (pixel_sum <= min_pixel_threshold) {
            continue;
        }
        if (prev_sum >= max_pixel_threshold) {
            break;
        }

        // Clamp contribution to inside the percent window
        float clamped_count = bin_count;
        if (prev_sum < min_pixel_threshold) {
            clamped_count -= (min_pixel_threshold - prev_sum);
        }
        if (pixel_sum > max_pixel_threshold) {
            clamped_count -= (pixel_sum - max_pixel_threshold);
        }

        weighted_sum += bin_to_luminance(float(i)) * clamped_count;
        valid_pixel_count += clamped_count;
    }

    float target_lum = (valid_pixel_count > 0.0) ? (weighted_sum / valid_pixel_count) : 1.0;
    target_lum = max(target_lum, 1e-4);

    // Read previous frame adapted luminance
    float4 prev_data = out_exposure_data[uint2(0, 0)];
    float prev_lum = max(prev_data.z, 1e-4);

    // Asymmetric temporal eye adaptation (pupil constricts fast to bright light, dilates slowly in dark)
    float speed = (target_lum > prev_lum) ? params.speed_up : params.speed_down;
    float adapted_lum = prev_lum + (target_lum - prev_lum) * (1.0 - exp(-params.delta_time * speed));

    // Convert average luminance to EV100: EV100 = log2(L_avg * 100 / 12.5)
    float target_ev100 = clamp(log2(adapted_lum * 8.0), params.min_ev, params.max_ev);

    // Physical exposure scale: 1.0 / (1.2 * 2^EV100)
    float exposure_scale = ev100_to_exposure(target_ev100);

    // Write out exposure state: { exposure_scale, ev100, adapted_luminance, 1.0 }
    out_exposure_data[uint2(0, 0)] = float4(exposure_scale, target_ev100, adapted_lum, 1.0);
}
