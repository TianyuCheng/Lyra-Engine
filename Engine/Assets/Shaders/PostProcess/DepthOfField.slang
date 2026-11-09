import lyra;
import Common.Packing;

static const float PI = 3.14159265358979323846;
static const float GOLDEN_ANGLE = 2.399963229728653; // PI * (3 - sqrt(5))

struct DOFParams
{
    float2 inv_screen_size;
    float  focal_distance;     // distance to in-focus plane in world units
    float  focal_length;       // lens focal length f in meters (e.g. 0.05 for 50mm lens)
    float  aperture_f_stop;    // lens f-number N (e.g. 1.4, 2.8, 4.0)
    float  max_coc_radius;     // max blur radius in pixels
    float  near_plane;
    float  far_plane;
};

[[lyra::binding(0, 0)]]
ConstantBuffer<DOFParams> params;

[[lyra::binding(1, 0)]]
Texture2D<float4> color_texture;

[[lyra::binding(2, 0)]]
Texture2D<float> depth_texture;

[[lyra::binding(3, 0)]]
SamplerState linear_sampler;

[[lyra::binding(4, 0)]]
RWTexture2D<float4> out_color;

/**
 * @brief Computes signed Circle of Confusion (CoC) in [-1, 1].
 * Negative for near field (in front of focal plane), positive for far field.
 */
float compute_coc(float linear_z)
{
    float s = params.focal_distance;
    float f = params.focal_length;
    float n = params.aperture_f_stop;

    // Thin lens formula: CoC = (A * f / (s - f)) * abs(s - z) / z
    float aperture_diameter = f / n;
    float coc_scale = (aperture_diameter * f) / max(abs(s - f), 1e-4);
    float coc = coc_scale * (linear_z - s) / max(linear_z, 1e-4);

    return clamp(coc / max(params.max_coc_radius, 1e-4), -1.0, 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_dof_bokeh(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    uint2 screen_dim;
    out_color.GetDimensions(screen_dim.x, screen_dim.y);

    if (pixel.x >= screen_dim.x || pixel.y >= screen_dim.y) {
        return;
    }

    float2 uv = (float2(pixel) + 0.5) * params.inv_screen_size;
    float raw_depth = depth_texture.Load(int3(pixel, 0));
    float center_z  = linearize_depth(raw_depth, params.near_plane, params.far_plane);
    float center_coc = compute_coc(center_z);

    // If practically in focus, skip bokeh integration
    if (abs(center_coc) < 0.05) {
        out_color[pixel] = color_texture.Load(int3(pixel, 0));
        return;
    }

    float3 acc_color = float3(0.0);
    float  acc_weight = 0.0;

    const uint SAMPLE_COUNT = 32;

    // Golden spiral disk sampling for realistic circular bokeh
    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        float r = sqrt((float(i) + 0.5) / float(SAMPLE_COUNT));
        float theta = float(i) * GOLDEN_ANGLE;
        float2 offset = float2(cos(theta), sin(theta)) * r;

        float2 sample_uv = uv + offset * params.inv_screen_size * (params.max_coc_radius * abs(center_coc));
        float sample_raw_depth = depth_texture.SampleLevel(linear_sampler, sample_uv, 0.0);
        float sample_z = linearize_depth(sample_raw_depth, params.near_plane, params.far_plane);
        float sample_coc = compute_coc(sample_z);
        float3 sample_color = color_texture.SampleLevel(linear_sampler, sample_uv, 0.0).rgb;

        // Depth-aware weighting:
        // Near-field objects (negative CoC) must bleed over background objects.
        // Far-field objects must not bleed over sharp foreground edges.
        float weight = 0.0;
        if (sample_z < center_z) {
            // Sample is in front: full weight if sample itself is blurred
            weight = smoothstep(0.0, 1.0, abs(sample_coc) / max(r, 1e-4));
        } else {
            // Sample is behind: only contribute if center itself is blurred into far field
            weight = smoothstep(0.0, 1.0, abs(center_coc) / max(r, 1e-4));
        }

        acc_color  += sample_color * weight;
        acc_weight += weight;
    }

    float3 final_color = (acc_weight > 0.0) ? (acc_color / acc_weight) : color_texture.Load(int3(pixel, 0)).rgb;
    out_color[pixel] = float4(final_color, 1.0);
}
