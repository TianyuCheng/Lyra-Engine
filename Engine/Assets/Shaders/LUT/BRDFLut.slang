import lyra;

static const float PI      = 3.14159265358979323846;
static const float TWO_PI  = 6.28318530717958647692;
static const float EPSILON = 1e-6;

float radical_inverse_vdc(uint bits)
{
    bits = reversebits(bits);
    return float(bits) * 2.3283064365386963e-10;
}

float2 hammersley_2d(uint i, uint N)
{
    return float2(float(i) / float(N), radical_inverse_vdc(i));
}

float3 sample_ggx(float2 u, float roughness, float3 N)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float phi = TWO_PI * u.x;
    float cos_theta = sqrt(max(0.0, (1.0 - u.y) / (1.0 + (a2 - 1.0) * u.y)));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

    float3 H = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float visibility_smith_ggx_correlated(float NoV, float NoL, float a2)
{
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(GGXV + GGXL, EPSILON);
}

float2 integrate_brdf(float NoV, float roughness, uint num_samples)
{
    NoV = clamp(NoV, 1e-4, 1.0);
    float3 V = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);
    float3 N = float3(0.0, 0.0, 1.0);
    float a = roughness * roughness;
    float a2 = a * a;

    float scale = 0.0;
    float bias  = 0.0;

    for (uint i = 0; i < num_samples; ++i) {
        float2 u = hammersley_2d(i, num_samples);
        float3 H = sample_ggx(u, roughness, N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NoL = max(L.z, 0.0);
        float NoH = max(H.z, 0.0);
        float VoH = max(dot(V, H), 0.0);

        if (NoL > 0.0) {
            float V_pdf = (visibility_smith_ggx_correlated(NoV, NoL, a2) * VoH) / max(NoH, EPSILON);
            float Fc = pow(1.0 - VoH, 5.0);

            scale += (1.0 - Fc) * V_pdf;
            bias  += Fc * V_pdf;
        }
    }

    return float2(scale, bias) * (4.0 / float(num_samples));
}

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_lut;

[shader("compute")]
[numthreads(16, 16, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 256 || pixel.y >= 256) {
        return;
    }

    float NoV       = max((float(pixel.x) + 0.5) / 256.0, 1e-4);
    float roughness = max((float(pixel.y) + 0.5) / 256.0, 1e-4);

    float2 integrated = integrate_brdf(NoV, roughness, 1024);

    out_lut[pixel] = float4(integrated.x, integrated.y, 0.0, 1.0);
}
