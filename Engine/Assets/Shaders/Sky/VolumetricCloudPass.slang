import lyra;
import Sky.Atmosphere;
import Sky.Clouds;
import Common.Tonemapping;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float3 ray_dir  : TEXCOORD1;
};

struct VolumetricCloudUniform
{
    float4x4 proj;
    float4x4 view;
    float4x4 inv_proj;
    float4x4 inv_view;
    float4   camera_pos_and_time;
    float4   sun_dir_and_intensity;
    float4   sun_color_and_powder;
    float4   cloud_altitudes;
    float4   cloud_density_and_shape;
    float4   cloud_detail_and_wind;
    float4   cloud_wind_dir_and_shear;
    float4   cloud_lighting_params;
    float4   ambient_sky_color;
    float4   ambient_ground_color_and_intensity;
    uint4    render_params;

    property float3 camera_pos { get { return camera_pos_and_time.xyz; } }
    property float  time { get { return camera_pos_and_time.w; } }
    property float3 sun_dir { get { return sun_dir_and_intensity.xyz; } }
    property float  sun_intensity { get { return sun_dir_and_intensity.w; } }
    property float3 sun_color { get { return sun_color_and_powder.xyz; } }
    property float  powder_scale { get { return sun_color_and_powder.w; } }

    property float  bottom_altitude { get { return cloud_altitudes.x; } }
    property float  top_altitude { get { return cloud_altitudes.y; } }
    property float  max_march_distance { get { return cloud_altitudes.z; } }
    property float  planet_radius { get { return cloud_altitudes.w; } }

    property float  coverage { get { return cloud_density_and_shape.x; } }
    property float  density_scale { get { return cloud_density_and_shape.y; } }
    property float  cloud_type { get { return cloud_density_and_shape.z; } }
    property float  shape_scale { get { return cloud_density_and_shape.w; } }

    property float  detail_scale { get { return cloud_detail_and_wind.x; } }
    property float  detail_strength { get { return cloud_detail_and_wind.y; } }
    property float  curl_strength { get { return cloud_detail_and_wind.z; } }
    property float  wind_speed { get { return cloud_detail_and_wind.w; } }

    property float3 wind_direction { get { return cloud_wind_dir_and_shear.xyz; } }
    property float  height_shear { get { return cloud_wind_dir_and_shear.w; } }

    property float  phase_g_forward { get { return cloud_lighting_params.x; } }
    property float  phase_g_backward { get { return cloud_lighting_params.y; } }
    property float  phase_blend { get { return cloud_lighting_params.z; } }
    property float  multi_scatter_boost { get { return cloud_lighting_params.w; } }

    property float3 ambient_sky { get { return ambient_sky_color.xyz; } }
    property float3 ambient_ground { get { return ambient_ground_color_and_intensity.xyz; } }
    property float  ambient_intensity { get { return ambient_ground_color_and_intensity.w; } }

    property uint   max_steps { get { return render_params.x; } }
    property uint   enable_tonemap { get { return render_params.y; } }
};

[[lyra::binding(0, 0)]]
ConstantBuffer<VolumetricCloudUniform> cloud_cb;

[[lyra::binding(1, 0)]]
Texture2D<float> scene_depth_tex;

[[lyra::binding(2, 0)]]
SamplerState linear_sampler;

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

    float4 p_far_view  = mul(float4(p, 1.0, 1.0), cloud_cb.inv_proj);
    p_far_view        /= p_far_view.w;
    float3 p_far_world = mul(float4(p_far_view.xyz, 1.0), cloud_cb.inv_view).xyz;

    output.ray_dir = normalize(p_far_world - cloud_cb.camera_pos);
    return output;
}

// Interleaved gradient noise for raymarching jitter
float bayer_dither(float2 screen_pos)
{
    return frac(52.9829189 * frac(dot(screen_pos, float2(0.06711056, 0.00583715))));
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float3 dir = normalize(input.ray_dir);

    // 1. Setup cloud simulation parameters from uniform buffer
    CloudParameters params = default_earth_clouds();
    params.planet_radius         = cloud_cb.planet_radius;
    params.cloud_bottom_altitude = cloud_cb.bottom_altitude;
    params.cloud_top_altitude    = cloud_cb.top_altitude;
    params.max_march_distance    = cloud_cb.max_march_distance;

    params.cloud_coverage        = cloud_cb.coverage;
    params.cloud_density_scale   = cloud_cb.density_scale;
    params.cloud_type            = cloud_cb.cloud_type;
    params.shape_scale           = cloud_cb.shape_scale;
    params.detail_scale          = cloud_cb.detail_scale;
    params.detail_strength       = cloud_cb.detail_strength;
    params.curl_strength         = cloud_cb.curl_strength;

    params.wind_direction        = normalize(cloud_cb.wind_direction);
    params.wind_speed            = cloud_cb.wind_speed;
    params.time                  = cloud_cb.time;
    params.height_shear          = cloud_cb.height_shear;

    params.phase_g_forward       = cloud_cb.phase_g_forward;
    params.phase_g_backward      = cloud_cb.phase_g_backward;
    params.phase_blend           = cloud_cb.phase_blend;
    params.powder_scale          = cloud_cb.powder_scale;
    params.multi_scatter_factor  = cloud_cb.multi_scatter_boost;

    params.sun_color             = cloud_cb.sun_color;
    params.sun_intensity         = cloud_cb.sun_intensity;
    params.ambient_sky_color     = cloud_cb.ambient_sky;
    params.ambient_ground_color  = cloud_cb.ambient_ground;
    params.ambient_intensity     = cloud_cb.ambient_intensity;

    // 2. Camera position in planetary shell coordinates (in km)
    float3 cam_pos_meters = cloud_cb.camera_pos;
    float  cam_alt_km     = max(cam_pos_meters.y * 0.001, 0.0);
    float3 ro             = float3(cam_pos_meters.x * 0.001, params.planet_radius + 0.001 + cam_alt_km, cam_pos_meters.z * 0.001);

    // 3. Reconstruct depth and calculate max ray distance in km
    float depth = scene_depth_tex.SampleLevel(linear_sampler, input.uv, 0.0);
    float max_distance_km = 1e6;
    if (depth < 0.9999 && depth > 0.0001) {
        float2 ndc_xy     = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
        float4 clip_pos   = float4(ndc_xy, depth, 1.0);
        float4 view_pos   = mul(clip_pos, cloud_cb.inv_proj);
        view_pos         /= max(view_pos.w, 1e-6);
        float4 world_pos  = mul(view_pos, cloud_cb.inv_view);
        max_distance_km   = length(world_pos.xyz - cam_pos_meters) * 0.001;
    }

    // 4. Jitter and step count
    float dither    = bayer_dither(input.position.xy);
    int   max_steps = int(max(cloud_cb.max_steps, 8u));

    // 5. Raymarch volumetric clouds
    CloudRaymarchResult clouds = raymarch_volumetric_clouds(
        params,
        ro,
        dir,
        normalize(cloud_cb.sun_dir),
        max_distance_km,
        max_steps,
        dither
    );

    if (clouds.alpha <= 1e-4) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    // 6. Un-premultiply for alpha blending over background
    float3 cloud_rgb = clouds.radiance / max(clouds.alpha, 1e-4);
    if (cloud_cb.enable_tonemap != 0) {
        cloud_rgb = tonemap_aces_film(cloud_rgb);
    }

    return float4(cloud_rgb, clouds.alpha);
}
