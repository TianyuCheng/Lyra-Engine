import lyra;
import Sky.Atmosphere;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
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
Texture2D<float4> transmittance_lut;

[[lyra::binding(2, 0)]]
Texture2D<float4> multiscatter_lut;

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
    output.position = float4(p, 0.0, 1.0);
    output.uv       = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    AtmosphereParameters atmo = default_earth_atmosphere();

    float exposed_sun_intensity = max(skybox.sun_intensity * skybox.exposure_scale, 0.01f);
    atmo.sun_intensity = clamp(exposed_sun_intensity * 1.8f, 5.0f, 40.0f);
    float3 sun_dir     = normalize(skybox.sun_dir);

    float cam_alt_km = max(skybox.camera_pos.y * 0.001, 0.0);
    float r = atmo.planet_radius + 0.001 + cam_alt_km;

    float theta, phi;
    uv_to_sky_view(input.uv, r, atmo.planet_radius, theta, phi);

    float3 forward = normalize(float3(sun_dir.x, 0.0, sun_dir.z));
    if (dot(forward, forward) < 1e-6) forward = float3(0.0, 0.0, 1.0);
    float3 right = float3(-forward.z, 0.0, forward.x);

    float sin_theta = sin(theta);
    float cos_theta = cos(theta);
    float3 rd = float3(sin_theta * (cos(phi) * forward.x + sin(phi) * right.x),
                       cos_theta,
                       sin_theta * (cos(phi) * forward.z + sin(phi) * right.z));
    rd = normalize(rd);

    float3 ro = float3(0.0, r, 0.0);

    float t_min, t_max;
    if (!intersect_atmosphere(ro, rd, atmo.planet_radius, atmo.atmosphere_radius, t_min, t_max)) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    bool hit_ground = false;
    float b_g = dot(ro, rd);
    float c_g = dot(ro, ro) - atmo.planet_radius * atmo.planet_radius;
    float d_g = b_g * b_g - c_g;
    if (rd.y < 0.0 && d_g >= 0.0) {
        float t_g = -b_g - sqrt(d_g);
        if (t_g > 0.0) {
            t_max = min(t_max, t_g);
            hit_ground = true;
        }
    }

    float cos_alpha = dot(rd, sun_dir);
    float p_rayleigh = rayleigh_phase(cos_alpha);
    float p_mie      = mie_phase(cos_alpha, atmo.mie_asymmetry);

    const int NUM_STEPS = 24;
    float3 acc_in_scatter = float3(0.0);
    float3 optical_depth_view = float3(0.0);

    for (int i = 0; i < NUM_STEPS; ++i) {
        float u0 = float(i) / float(NUM_STEPS);
        float u1 = float(i + 1) / float(NUM_STEPS);
        float t0 = t_min + (t_max - t_min) * (u0 * u0);
        float t1 = t_min + (t_max - t_min) * (u1 * u1);
        float step_size = t1 - t0;
        float t = 0.5 * (t0 + t1);

        float3 sample_p = ro + rd * t;
        float h = max(length(sample_p) - atmo.planet_radius, 0.0);
        float r_sample = length(sample_p);

        float d_r = exp(-h / atmo.rayleigh_scale_height) * step_size;
        float d_m = exp(-h / atmo.mie_scale_height) * step_size;
        float d_o = max(0.0, 1.0 - abs(h - 25.0) / 15.0) * step_size;
        optical_depth_view += float3(d_r, d_m, d_o);

        float mu_s = dot(normalize(sample_p), sun_dir);
        float3 trans_sun = sample_transmittance_lut(transmittance_lut, linear_sampler, r_sample, mu_s, atmo.planet_radius, atmo.atmosphere_radius);
        float3 l_multi = sample_multiscatter_lut(multiscatter_lut, linear_sampler, r_sample, mu_s, atmo.planet_radius, atmo.atmosphere_radius);

        float3 tau_view = atmo.rayleigh_scattering * optical_depth_view.x +
                          atmo.mie_extinction * optical_depth_view.y +
                          atmo.ozone_absorption * optical_depth_view.z;
        float3 trans_view = exp(-tau_view);

        float3 s_single = (d_r * atmo.rayleigh_scattering * p_rayleigh +
                           d_m * atmo.mie_scattering * p_mie) * (atmo.sun_intensity * trans_sun);
        float3 s_multi = (d_r * atmo.rayleigh_scattering + d_m * atmo.mie_scattering) * (l_multi * atmo.sun_intensity);

        acc_in_scatter += (s_single + s_multi) * trans_view;
    }

    if (hit_ground) {
        float3 horizon_dir = normalize(float3(rd.x, 0.002, rd.z));
        float3 horizon_sky = evaluate_sky_radiance(atmo, ro, horizon_dir, sun_dir, 8);

        float3 p_ground = ro + rd * t_max;
        float3 n_ground = normalize(p_ground);
        float mu_s_ground = dot(n_ground, sun_dir);
        float3 trans_sun_ground = sample_transmittance_lut(transmittance_lut, linear_sampler, atmo.planet_radius, mu_s_ground, atmo.planet_radius, atmo.atmosphere_radius);
        float3 ground_sun_irradiance = (atmo.sun_intensity * max(mu_s_ground, 0.0)) * trans_sun_ground;
        float3 ground_ambient = sample_multiscatter_lut(multiscatter_lut, linear_sampler, atmo.planet_radius, mu_s_ground, atmo.planet_radius, atmo.atmosphere_radius) * (atmo.sun_intensity * 2.0 * PI);
        float3 ground_radiance = (skybox.ground_albedo.rgb / PI) * ground_sun_irradiance + skybox.ground_albedo.rgb * ground_ambient * 0.25;

        // natural atmospheric aerial perspective / horizon haze blend:
        // near the horizon, thick air column matches horizon haze seamlessly;
        // looking down towards nadir, ground albedo dominates.
        float haze = exp(-6.0 * max(-rd.y, 0.0));
        acc_in_scatter = lerp(ground_radiance, horizon_sky, haze);
    }

    return float4(acc_in_scatter, 1.0);
}
