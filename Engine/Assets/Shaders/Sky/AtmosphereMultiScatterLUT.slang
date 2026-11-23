import lyra;
import Sky.Atmosphere;

[[lyra::binding(0, 0)]]
Texture2D<float4> transmittance_lut;

[[lyra::binding(1, 0)]]
SamplerState linear_sampler;

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

float4 compute_multi_scattering(float2 uv)
{
    AtmosphereParameters atmo = default_earth_atmosphere();

    float mu_s = clamp(uv.x * 2.0 - 1.0, -1.0, 1.0);
    float r = clamp(atmo.planet_radius + uv.y * (atmo.atmosphere_radius - atmo.planet_radius), atmo.planet_radius, atmo.atmosphere_radius);

    float3 p = float3(0.0, r, 0.0);
    float3 sun_dir = float3(sqrt(max(0.0, 1.0 - mu_s * mu_s)), mu_s, 0.0);

    const int NUM_ELEV = 8;
    const int NUM_AZIM = 8;
    float3 l_2_sum = float3(0.0);
    float3 f_ms_sum = float3(0.0);

    for (int ei = 0; ei < NUM_ELEV; ++ei) {
        float theta = PI * (float(ei) + 0.5) / float(NUM_ELEV);
        float cos_theta = cos(theta);
        float sin_theta = sin(theta);
        float d_omega = (2.0 * PI / float(NUM_AZIM)) * (PI / float(NUM_ELEV)) * sin_theta;

        for (int ai = 0; ai < NUM_AZIM; ++ai) {
            float phi = 2.0 * PI * (float(ai) + 0.5) / float(NUM_AZIM);
            float3 d_dir = float3(sin_theta * cos(phi), cos_theta, sin_theta * sin(phi));

            float t_min, t_max;
            if (!intersect_atmosphere(p, d_dir, atmo.planet_radius, atmo.atmosphere_radius, t_min, t_max)) {
                continue;
            }

            bool hit_ground = false;
            float b_g = dot(p, d_dir);
            float c_g = dot(p, p) - atmo.planet_radius * atmo.planet_radius;
            float d_g = b_g * b_g - c_g;
            if (d_dir.y < 0.0 && d_g >= 0.0) {
                float t_g = -b_g - sqrt(d_g);
                if (t_g > 0.0) {
                    t_max = min(t_max, t_g);
                    hit_ground = true;
                }
            }

            const int NUM_STEPS = 16;
            float step_size = (t_max - t_min) / float(NUM_STEPS);
            float3 ray_l1 = float3(0.0);
            float3 ray_f_ms = float3(0.0);
            float3 optical_depth = float3(0.0);
            float cos_alpha = dot(d_dir, sun_dir);
            float p_r = rayleigh_phase(cos_alpha);
            float p_m = mie_phase(cos_alpha, atmo.mie_asymmetry);

            for (int s = 0; s < NUM_STEPS; ++s) {
                float t = t_min + (float(s) + 0.5) * step_size;
                float3 sample_p = p + d_dir * t;
                float h = max(length(sample_p) - atmo.planet_radius, 0.0);
                float r_s = length(sample_p);

                float d_r = exp(-h / atmo.rayleigh_scale_height) * step_size;
                float d_m = exp(-h / atmo.mie_scale_height) * step_size;
                float d_o = max(0.0, 1.0 - abs(h - 25.0) / 15.0) * step_size;
                optical_depth += float3(d_r, d_m, d_o);

                float mu_sample = dot(normalize(sample_p), sun_dir);
                float3 trans_sun = sample_transmittance_lut(transmittance_lut, linear_sampler, r_s, mu_sample, atmo.planet_radius, atmo.atmosphere_radius);

                float3 tau_view = atmo.rayleigh_scattering * optical_depth.x +
                                  atmo.mie_extinction * optical_depth.y +
                                  atmo.ozone_absorption * optical_depth.z;
                float3 trans_view = exp(-tau_view);

                float3 s_single = (d_r * atmo.rayleigh_scattering * p_r +
                                   d_m * atmo.mie_scattering * p_m) * trans_sun;
                ray_l1 += s_single * trans_view;

                float3 beta_s = d_r * atmo.rayleigh_scattering + d_m * atmo.mie_scattering;
                ray_f_ms += beta_s * trans_view;
            }

            if (hit_ground) {
                float3 p_g = p + d_dir * t_max;
                float3 n_g = normalize(p_g);
                float mu_s_g = dot(n_g, sun_dir);
                float3 trans_sun_g = sample_transmittance_lut(transmittance_lut, linear_sampler, atmo.planet_radius, mu_s_g, atmo.planet_radius, atmo.atmosphere_radius);
                float3 sun_irrad = max(mu_s_g, 0.0) * trans_sun_g;
                float3 g_rad = (atmo.ground_albedo / PI) * sun_irrad;

                float3 tau_view = atmo.rayleigh_scattering * optical_depth.x +
                                  atmo.mie_extinction * optical_depth.y +
                                  atmo.ozone_absorption * optical_depth.z;
                float3 trans_view = exp(-tau_view);
                ray_l1 += g_rad * trans_view;
                ray_f_ms += atmo.ground_albedo * trans_view;
            }

            l_2_sum += ray_l1 * d_omega;
            f_ms_sum += ray_f_ms * d_omega;
        }
    }

    float3 l_2 = l_2_sum / (4.0 * PI);
    float3 f_ms = f_ms_sum / (4.0 * PI);
    float3 l_multi = l_2 / max(float3(1.0) - f_ms, float3(0.01));

    return float4(l_multi, 1.0);
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return compute_multi_scattering(input.uv);
}
