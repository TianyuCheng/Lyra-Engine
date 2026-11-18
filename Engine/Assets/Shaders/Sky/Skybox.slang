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
    float3   camera_pos;
    float    sun_intensity;
    float3   sun_dir;
    float    sun_angular_radius;
    float3   sun_color;
    float    exposure_scale;
    float    halo_size;
    float    halo_falloff;
    float    limb_darkening;
    float    ground_roughness;
    float4   ground_albedo;
};

[[lyra::binding(0, 0)]]
ConstantBuffer<SkyboxUniform> skybox;

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
    // position at far plane (z = 1.0 in clip space)
    output.position = float4(p, 1.0, 1.0);

    // unproject far clip point to world space to compute ray direction
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

    atmo.sun_intensity      = skybox.sun_intensity;
    atmo.sun_angular_radius = skybox.sun_angular_radius;
    float3 sun_dir          = normalize(skybox.sun_dir);

    // camera altitude in kilometers (scene coordinates in meters)
    float cam_alt_km  = max(skybox.camera_pos.y * 0.001, 0.0);
    float3 camera_pos = float3(0.0, atmo.planet_radius + 0.001 + cam_alt_km, 0.0);

    // direct solar beam transmittance through the atmospheric column along sun path
    float2 optical_depth_sun = integrate_optical_depth(atmo, camera_pos, sun_dir, 16);
    float3 tau_sun           = atmo.rayleigh_scattering * optical_depth_sun.x +
                               atmo.mie_extinction * optical_depth_sun.y;
    float3 sun_transmittance = exp(-tau_sun);

    // reference horizon sky radiance to guarantee seamless continuity at the horizon
    float3 horizon_dir = normalize(float3(dir.x, 0.002, dir.z));
    float3 horizon_sky = evaluate_sky_radiance(atmo, camera_pos, horizon_dir, sun_dir, 16);

    float3 color;
    if (dir.y >= 0.0)
    {
        // 1. Upper hemisphere: atmospheric Rayleigh & Mie scattering + physical sun disc & corona
        color = evaluate_sky_radiance(atmo, camera_pos, dir, sun_dir, 16);
        color += render_physical_sun(atmo, dir, sun_dir, sun_transmittance,
                                     skybox.halo_size, skybox.halo_falloff, skybox.limb_darkening) * skybox.sun_color;
    }
    else
    {
        // 2. Lower hemisphere (ground / earth surface):
        // direct sun illumination on horizontal ground plane attenuated by transmittance
        float sun_elev = max(sun_dir.y, 0.0);
        float3 sun_irradiance = skybox.sun_color * (sun_elev * skybox.sun_intensity) * sun_transmittance;

        // ambient skylight arriving at the ground
        float3 sky_ambient = horizon_sky * 0.25;

        // diffuse ground reflectance (dark neutral tone matching editor ground, Lambertian 1/PI)
        float3 ground_radiance = (skybox.ground_albedo.rgb / PI) * sun_irradiance + skybox.ground_albedo.rgb * sky_ambient;

        // atmospheric aerial perspective (distance haze)
        // near the horizon, thick air column matches horizon haze seamlessly;
        // looking down towards nadir, ground albedo dominates.
        float haze = exp(-6.0 * max(-dir.y, 0.0));
        color = lerp(ground_radiance, horizon_sky, haze);
    }

    // Apply physical camera exposure scaling before ACES filmic tonemapping
    color *= skybox.exposure_scale;

    // ACES filmic tonemapping for rich HDR response
    color = tonemap_aces_film(color);

    return float4(color, 1.0);
}
