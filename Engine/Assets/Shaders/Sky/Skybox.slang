import lyra;
import Sky.Atmosphere;
import Common.Tonemapping;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 ray_dir  : TEXCOORD0;
};

struct SkyboxUniform
{
    float4x4 proj;
    float4x4 view;
    float4x4 inv_proj;
    float4x4 inv_view;
    float4   camera_pos_and_sun_intensity;
    float4   sun_dir_and_angular_radius;
    float4   sun_color_and_exposure_scale;
    float4   sky_params;
    float4   ground_albedo;

    property float3 camera_pos { get { return camera_pos_and_sun_intensity.xyz; } }
    property float  sun_intensity { get { return camera_pos_and_sun_intensity.w; } }
    property float3 sun_dir { get { return sun_dir_and_angular_radius.xyz; } }
    property float  sun_angular_radius { get { return sun_dir_and_angular_radius.w; } }
    property float3 sun_color { get { return sun_color_and_exposure_scale.xyz; } }
    property float  exposure_scale { get { return sun_color_and_exposure_scale.w; } }
    property float  halo_size { get { return sky_params.x; } }
    property float  halo_falloff { get { return sky_params.y; } }
    property float  limb_darkening { get { return sky_params.z; } }
    property float  ground_roughness { get { return sky_params.w; } }
};

[[lyra::binding(0, 0)]]
ConstantBuffer<SkyboxUniform> skybox;

[[lyra::binding(1, 0)]]
Texture2D<float4> sky_view_lut;

[[lyra::binding(2, 0)]]
Texture2D<float4> transmittance_lut;

[[lyra::binding(3, 0)]]
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
    output.position = float4(p, 0.999999, 1.0);

    float4 p_far_view  = mul(float4(p, 1.0, 1.0), skybox.inv_proj);
    p_far_view        /= p_far_view.w;
    float3 p_far_world = mul(float4(p_far_view.xyz, 1.0), skybox.inv_view).xyz;

    output.ray_dir = normalize(p_far_world - skybox.camera_pos);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float3 dir = normalize(input.ray_dir);
    AtmosphereParameters atmo = default_earth_atmosphere();

    float exposed_sun_intensity = max(skybox.sun_intensity * skybox.exposure_scale, 0.01f);
    atmo.sun_intensity          = clamp(exposed_sun_intensity * 1.8f, 5.0f, 40.0f);
    atmo.sun_angular_radius     = skybox.sun_angular_radius;
    float3 sun_dir              = normalize(skybox.sun_dir);

    // camera altitude in kilometers
    float cam_alt_km = max(skybox.camera_pos.y * 0.001, 0.0);
    float r          = atmo.planet_radius + 0.001 + cam_alt_km;

    // 1. Sample Sky-View LUT using non-linear horizon-packed spherical coordinate mapping
    float2 uv       = sky_view_to_uv(dir, sun_dir, r, atmo.planet_radius);
    float3 radiance = sky_view_lut.SampleLevel(linear_sampler, uv, 0.0).rgb;

    // 2. Direct solar beam transmittance from Transmittance LUT
    float mu_s               = dot(float3(0.0, 1.0, 0.0), sun_dir);
    float3 sun_transmittance = sample_transmittance_lut(transmittance_lut, linear_sampler, r, mu_s, atmo.planet_radius, atmo.atmosphere_radius);

    // 3. Render analytical physical solar disc with limb darkening and corona
    float3 sun_radiance = render_physical_sun(
        atmo,
        dir,
        sun_dir,
        sun_transmittance,
        skybox.halo_size,
        skybox.halo_falloff,
        skybox.limb_darkening);

    float3 color = radiance;
    if (dir.y >= 0.0) {
        color += sun_radiance * skybox.sun_color;
    }

    // ACES filmic tonemapping for rich HDR response
    color = tonemap_aces_film(color);

    return float4(color, 1.0);
}
