import lyra;
import Sky.Atmosphere;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

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

float4 compute_transmittance(float2 uv)
{
    AtmosphereParameters atmo = default_earth_atmosphere();

    float r, mu;
    uv_to_transmittance(uv, atmo.planet_radius, atmo.atmosphere_radius, r, mu);

    float b = r * mu;
    float c = r * r - atmo.planet_radius * atmo.planet_radius;
    float d = b * b - c;
    if (mu < 0.0 && d >= 0.0) {
        // ray intersects ground body
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    // intersection with top of atmosphere
    float t_max = -r * mu + sqrt(max(0.0, r * r * (mu * mu - 1.0) + atmo.atmosphere_radius * atmo.atmosphere_radius));

    const int STEP_COUNT = 40;
    float step_size = t_max / float(STEP_COUNT);
    float3 od = float3(0.0);

    float3 p = float3(0.0, r, 0.0);
    float3 sun_dir = float3(sqrt(max(0.0, 1.0 - mu * mu)), mu, 0.0);

    for (int i = 0; i < STEP_COUNT; ++i) {
        float t = (float(i) + 0.5) * step_size;
        float3 sample_p = p + sun_dir * t;
        float h = max(length(sample_p) - atmo.planet_radius, 0.0);

        od.x += exp(-h / atmo.rayleigh_scale_height) * step_size;
        od.y += exp(-h / atmo.mie_scale_height) * step_size;
        od.z += max(0.0, 1.0 - abs(h - 25.0) / 15.0) * step_size;
    }

    float3 tau = atmo.rayleigh_scattering * od.x +
                 atmo.mie_extinction * od.y +
                 atmo.ozone_absorption * od.z;

    return float4(exp(-tau), 1.0);
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return compute_transmittance(input.uv);
}
