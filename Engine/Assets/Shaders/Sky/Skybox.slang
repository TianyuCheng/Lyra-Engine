import lyra;
import Sky.Atmosphere;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 ray_dir  : TEXCOORD0;
};

struct Camera
{
    float4x4 proj;
    float4x4 view;
    float4x4 inv_proj;
    float4x4 inv_view;
    float3   pos;
    float    padding;
};

ConstantBuffer<Camera> camera;

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
    float4 p_far_view  = mul(float4(p, 1.0, 1.0), camera.inv_proj);
    p_far_view        /= p_far_view.w;
    float3 p_far_world = mul(float4(p_far_view.xyz, 1.0), camera.inv_view).xyz;

    output.ray_dir = normalize(p_far_world - camera.pos);
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    float3 dir = normalize(input.ray_dir);
    AtmosphereParameters atmo = default_earth_atmosphere();

    // normalized sun direction vector (e.g. standard sunlight direction)
    float3 sun_dir    = normalize(float3(0.3, 0.6, 0.4));
    float3 camera_pos = float3(0.0, atmo.planet_radius + 0.001, 0.0);

    // evaluate physical Rayleigh + Mie atmospheric scattering along view ray
    float3 sky = evaluate_sky_radiance(atmo, camera_pos, dir, sun_dir, 16);
    sky += render_sun_disc(atmo, dir, sun_dir);

    return float4(sky, 1.0);
}
