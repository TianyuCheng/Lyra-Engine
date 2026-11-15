import lyra;
import Sky.Atmosphere;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_skymap;

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 64 || pixel.y >= 64) {
        return;
    }

    float u = (float(pixel.x) + 0.5) / 64.0;
    float v = (float(pixel.y) + 0.5) / 64.0;

    // equirectangular spherical direction mapping
    float phi   = (u * 2.0 - 1.0) * 3.14159265;
    float theta = (v - 0.5) * 3.14159265;
    float3 dir  = normalize(float3(cos(theta) * sin(phi), sin(theta), cos(theta) * cos(phi)));

    AtmosphereParameters atmo = default_earth_atmosphere();
    float3 sun_dir    = normalize(float3(0.3, 0.6, 0.4));
    float3 camera_pos = float3(0.0, atmo.planet_radius + 0.001, 0.0);

    float3 color = evaluate_sky_radiance(atmo, camera_pos, dir, sun_dir, 16);
    color += render_sun_disc(atmo, dir, sun_dir);

    out_skymap[pixel] = float4(color, 1.0);
}
