import lyra;
import Common.Colorspace;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct FXAAParams
{
    float2 inv_screen_size;
    float  subpixel_quality;   // 0.75 default, 1.0 maximum sharpness
    float  edge_threshold;     // 0.166 default, 0.125 for higher edge sensitivity
    float  edge_threshold_min; // 0.0833 default, 0.0625 for low contrast edge sensitivity
    uint   screen_width;
    uint   screen_height;
    float  padding;
};

[[lyra::immediates]]
ConstantBuffer<FXAAParams> params;

[[lyra::binding(0, 0)]]
Texture2D<float4> input_texture;

[[lyra::binding(1, 0)]]
SamplerState linear_sampler;

float rgb_to_luma(float3 rgb)
{
    return linear_srgb_to_luminance(rgb);
}

[shader("vertex")]
VertexOutput vsmain(uint vertex_id : SV_VertexID)
{
    static const float2 quad_vertices[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0,  1.0)
    };

    float2 p = quad_vertices[vertex_id];

    VertexOutput output;
    output.position = float4(p, 0.0, 1.0);
    output.uv       = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float2 uv = input.uv;
    float2 d  = params.inv_screen_size;

    // 1. Fetch 5 local luma samples
    float3 color_m = input_texture.SampleLevel(linear_sampler, uv, 0.0).rgb;
    float  luma_m  = rgb_to_luma(color_m);

    float luma_n = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2(0.0, -d.y), 0.0).rgb);
    float luma_s = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2(0.0,  d.y), 0.0).rgb);
    float luma_w = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2(-d.x, 0.0), 0.0).rgb);
    float luma_e = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2( d.x, 0.0), 0.0).rgb);

    float luma_min = min(luma_m, min(min(luma_n, luma_s), min(luma_w, luma_e)));
    float luma_max = max(luma_m, max(max(luma_n, luma_s), max(luma_w, luma_e)));
    float luma_range = luma_max - luma_min;

    // If local contrast is below threshold, skip AA
    if (luma_range < max(params.edge_threshold_min, luma_max * params.edge_threshold)) {
        return float4(color_m, 1.0);
    }

    // 2. Fetch corner samples
    float luma_nw = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2(-d.x, -d.y), 0.0).rgb);
    float luma_ne = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2( d.x, -d.y), 0.0).rgb);
    float luma_sw = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2(-d.x,  d.y), 0.0).rgb);
    float luma_se = rgb_to_luma(input_texture.SampleLevel(linear_sampler, uv + float2( d.x,  d.y), 0.0).rgb);

    // 3. Compute edge direction (horizontal vs vertical)
    // Horizontal edge: high contrast vertically (between North and South)
    float edge_horz = abs((luma_nw + luma_sw) - 2.0 * luma_w) +
                      2.0 * abs((luma_n + luma_s) - 2.0 * luma_m) +
                      abs((luma_ne + luma_se) - 2.0 * luma_e);

    // Vertical edge: high contrast horizontally (between West and East)
    float edge_vert = abs((luma_nw + luma_ne) - 2.0 * luma_n) +
                      2.0 * abs((luma_w + luma_e) - 2.0 * luma_m) +
                      abs((luma_sw + luma_se) - 2.0 * luma_s);

    bool is_horizontal = edge_horz >= edge_vert;

    // 4. Select edge orientation and step towards the edge (the side with highest gradient)
    float luma_1 = is_horizontal ? luma_n : luma_w;
    float luma_2 = is_horizontal ? luma_s : luma_e;
    float grad_1 = abs(luma_1 - luma_m);
    float grad_2 = abs(luma_2 - luma_m);

    // Step towards side 1 (-d) if grad_1 >= grad_2, else side 2 (+d)
    float step_length = is_horizontal ? d.y : d.x;
    if (grad_1 >= grad_2) {
        step_length = -step_length;
    }

    // 5. Sub-pixel blending factor using low-pass 3x3 filter
    float luma_l          = (luma_n + luma_s + luma_w + luma_e) * 2.0 + (luma_nw + luma_ne + luma_sw + luma_se);
    float luma_avg        = luma_l / 12.0;
    float subpixel_offset = clamp(abs(luma_avg - luma_m) / luma_range, 0.0, 1.0);
    float blend_factor    = subpixel_offset * subpixel_offset * params.subpixel_quality;

    // Sample across the edge
    float2 blend_uv = uv;
    if (is_horizontal) {
        blend_uv.y += step_length * blend_factor;
    } else {
        blend_uv.x += step_length * blend_factor;
    }

    float3 final_color = input_texture.SampleLevel(linear_sampler, blend_uv, 0.0).rgb;
    return float4(final_color, 1.0);
}
