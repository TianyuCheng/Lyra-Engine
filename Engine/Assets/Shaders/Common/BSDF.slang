module BSDF;

// references: Cook-Torrance (1982), GGX (Walter 2007), Heitz (2014), Burley (2012), Filament PBR
import Sampling;

static const float PI        = 3.14159265358979323846;
static const float TWO_PI    = 6.28318530717958647692;
static const float INV_PI    = 0.31830988618379067154;
static const float EPSILON   = 1e-6;

// -----------------------------------------------------------------------------
// 1. Fresnel Formulations
// -----------------------------------------------------------------------------

/**
 * @brief Computes dielectric / conductor F0 from base color, metallic, and perceptual reflectance.
 * F0 = 0.16 * reflectance^2 * (1 - metallic) + baseColor * metallic
 */
public float3 compute_f0(float3 base_color, float metallic, float reflectance = 0.5)
{
    float3 dielectric_f0 = float3(0.16 * reflectance * reflectance);
    return lerp(dielectric_f0, base_color, metallic);
}

/**
 * @brief Schlick approximation for Fresnel reflection.
 */
public float3 fresnel_schlick(float cos_theta, float3 F0)
{
    float f = pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
    return F0 + (1.0 - F0) * f;
}

/**
 * @brief Schlick approximation with grazing reflectance F90.
 */
public float3 fresnel_schlick_f90(float cos_theta, float3 F0, float F90)
{
    float f = pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
    return F0 + (float3(F90) - F0) * f;
}

/**
 * @brief Roughness-dependent Fresnel for indirect image-based lighting (Karis 2013).
 */
public float3 fresnel_schlick_roughness(float cos_theta, float3 F0, float roughness)
{
    float3 max_f0 = max(float3(1.0 - roughness), F0);
    return F0 + (max_f0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

/**
 * @brief Clear coat Fresnel interface: polyurethane with IOR = 1.5 -> F0 = 0.04.
 */
public float fresnel_clear_coat(float cos_theta)
{
    float f0 = 0.04;
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

/**
 * @brief Exact Snell's law Fresnel for unpolarized dielectric interfaces.
 */
public float fresnel_dielectric(float cos_theta_i, float eta_i, float eta_t)
{
    cos_theta_i = clamp(cos_theta_i, -1.0, 1.0);
    bool entering = cos_theta_i > 0.0;
    if (!entering) {
        float temp = eta_i;
        eta_i = eta_t;
        eta_t = temp;
        cos_theta_i = abs(cos_theta_i);
    }

    float sin_theta_i = max(0.0, 1.0 - cos_theta_i * cos_theta_i);
    float sin_theta_t = (eta_i / eta_t) * (eta_i / eta_t) * sin_theta_i;
    if (sin_theta_t >= 1.0) return 1.0; // total internal reflection

    float cos_theta_t = sqrt(max(0.0, 1.0 - sin_theta_t));
    float r_parl = ((eta_t * cos_theta_i) - (eta_i * cos_theta_t)) / ((eta_t * cos_theta_i) + (eta_i * cos_theta_t));
    float r_perp = ((eta_i * cos_theta_i) - (eta_t * cos_theta_t)) / ((eta_i * cos_theta_i) + (eta_t * cos_theta_t));
    return 0.5 * (r_parl * r_parl + r_perp * r_perp);
}

// -----------------------------------------------------------------------------
// 2. Normal Distribution Functions (NDF D)
// -----------------------------------------------------------------------------

/**
 * @brief Trowbridge-Reitz / GGX microfacet normal distribution function.
 * Clamps alpha to 0.001 to prevent floating-point underflow at low roughness.
 */
public float distribution_ggx(float NoH, float roughness)
{
    float alpha = max(roughness * roughness, 0.001);
    float a = NoH * alpha;
    float k = alpha / (1.0 - NoH * NoH + a * a);
    return k * k * INV_PI;
}

/**
 * @brief Anisotropic GGX NDF with independent tangent (at) and bitangent (ab) roughness.
 */
public float distribution_anisotropic_ggx(float NoH, float ToH, float BoH, float at, float ab)
{
    at = max(at, 0.001);
    ab = max(ab, 0.001);
    float a2 = at * ab;
    float3 d = float3(ab * ToH, at * BoH, a2 * NoH);
    float d2 = dot(d, d);
    float b2 = a2 / max(d2, EPSILON);
    return a2 * b2 * b2 * INV_PI;
}

/**
 * @brief Charlie NDF for cloth and sheen microfacets (Estevez & Kulla).
 */
public float distribution_charlie(float NoH, float roughness)
{
    float alpha = max(roughness * roughness, 0.001);
    float inv_alpha = 1.0 / alpha;
    float cos2h = NoH * NoH;
    float sin2h = max(1.0 - cos2h, 0.0078125); // 2^-7
    return (2.0 + inv_alpha) * pow(sin2h, inv_alpha * 0.5) / (2.0 * PI);
}

// -----------------------------------------------------------------------------
// 3. Geometric Masking & Visibility Functions (V = G / (4 * NoV * NoL))
// -----------------------------------------------------------------------------

/**
 * @brief Exact height-correlated Smith GGX visibility function (Heitz 2014).
 * V = G / (4 * NoV * NoL)
 */
public float visibility_smith_ggx_correlated(float NoV, float NoL, float roughness)
{
    float a2 = max(pow(roughness * roughness, 2.0), 1e-5);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(GGXV + GGXL, EPSILON);
}

/**
 * @brief Fast approximate height-correlated Smith GGX visibility function.
 * Avoids two square roots: V = 0.5 / lerp(2 * NoL * NoV, NoL + NoV, alpha)
 */
public float visibility_smith_ggx_fast(float NoV, float NoL, float roughness)
{
    float alpha = max(roughness * roughness, 0.001);
    return 0.5 / max(lerp(2.0 * NoL * NoV, NoL + NoV, alpha), EPSILON);
}

/**
 * @brief Anisotropic height-correlated Smith GGX visibility function.
 */
public float visibility_anisotropic_smith_ggx(
    float NoV, float NoL,
    float ToV, float BoV,
    float ToL, float BoL,
    float at,  float ab)
{
    float lambdaV = NoL * length(float3(at * ToV, ab * BoV, NoV));
    float lambdaL = NoV * length(float3(at * ToL, ab * BoL, NoL));
    return 0.5 / max(lambdaV + lambdaL, EPSILON);
}

/**
 * @brief Kelemen & Szirmay-Kalos visibility function specifically formulated for Clear Coat.
 * V = 0.25 / (LoH)^2
 */
public float visibility_kelemen(float LoH)
{
    return 0.25 / max(LoH * LoH, EPSILON);
}

/**
 * @brief Neubelt & Pettineo visibility function for cloth and sheen.
 */
public float visibility_neubelt(float NoV, float NoL)
{
    return 1.0 / max(4.0 * (NoL + NoV - NoL * NoV), EPSILON);
}

// -----------------------------------------------------------------------------
// 4. Diffuse Reflectance Formulations (BRDF)
// -----------------------------------------------------------------------------

/**
 * @brief Standard Lambertian diffuse reflection.
 */
public float3 diffuse_lambert(float3 diffuse_color)
{
    return diffuse_color * INV_PI;
}

/**
 * @brief Burley / Disney diffuse reflection with retro-reflection at grazing angles.
 */
public float3 diffuse_burley(float3 diffuse_color, float NoV, float NoL, float LoH, float roughness)
{
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float light_scatter = 1.0 + (f90 - 1.0) * pow(clamp(1.0 - NoL, 0.0, 1.0), 5.0);
    float view_scatter  = 1.0 + (f90 - 1.0) * pow(clamp(1.0 - NoV, 0.0, 1.0), 5.0);
    return diffuse_color * (INV_PI * light_scatter * view_scatter);
}

// -----------------------------------------------------------------------------
// 5. Specular Dielectric Transmittance (BTDF)
// -----------------------------------------------------------------------------

/**
 * @brief Walter et al. microfacet dielectric transmission (refraction).
 * @param ior_i Incident medium refractive index (e.g. 1.0 for air)
 * @param ior_t Transmitted medium refractive index (e.g. 1.5 for glass)
 */
public float3 btdf_dielectric(float3 N, float3 V, float3 L, float roughness, float ior_i, float ior_t)
{
    float eta = ior_i / ior_t;
    float3 H = -normalize(V + L * eta);

    float NdotV = dot(N, V);
    float NdotL = dot(N, L);

    if (NdotV * NdotL >= 0.0) {
        return float3(0.0); // same hemisphere: reflection, not transmission
    }

    float VdotH = dot(V, H);
    float LdotH = dot(L, H);

    float D = distribution_ggx(abs(dot(N, H)), roughness);
    float G = visibility_smith_ggx_correlated(abs(NdotV), abs(NdotL), roughness) * (4.0 * abs(NdotV) * abs(NdotL));
    float F = fresnel_dielectric(abs(VdotH), ior_i, ior_t);

    float denom = (VdotH + eta * LdotH);
    denom = denom * denom;

    float factor = abs(VdotH) * abs(LdotH) / (abs(NdotV) * abs(NdotL));
    float value  = (factor * (1.0 - F) * D * G * (eta * eta)) / max(denom, EPSILON);

    return float3(value);
}

// -----------------------------------------------------------------------------
// 6. Clear Coat Layer
// -----------------------------------------------------------------------------

public struct ClearCoatParameters
{
    public float  factor;           // clear coat strength [0, 1]
    public float  roughness;        // perceptual roughness of clear coat
    public float3 tint;             // absorption color of clear coat layer
    public float  thickness;        // layer thickness for absorption
};

/**
 * @brief Evaluates clear coat specular reflection and absorption attenuation.
 */
public void evaluate_clear_coat(
    ClearCoatParameters params,
    float  NoH,
    float  NoV,
    float  NoL,
    float  VoH,
    float  LoH,
    out float3 out_clear_coat_specular,
    out float3 out_base_attenuation)
{
    if (params.factor <= 0.0) {
        out_clear_coat_specular = float3(0.0);
        out_base_attenuation = float3(1.0);
        return;
    }

    // 1. clear coat specular: GGX NDF + Kelemen visibility + polyurethane Fresnel (F0 = 0.04)
    float D = distribution_ggx(NoH, params.roughness);
    float V = visibility_kelemen(LoH);
    float F = fresnel_clear_coat(VoH);

    float cc_spec = params.factor * (D * V * F);
    out_clear_coat_specular = float3(cc_spec);

    // 2. base layer attenuation: energy conservation (1 - F) and optional absorption Beer-Lambert law
    float3 trans = float3(1.0 - params.factor * F);
    if (params.thickness > 0.0) {
        float path_length = (1.0 / max(NoV, 0.1)) + (1.0 / max(NoL, 0.1));
        float3 absorption = exp(-params.tint * (path_length * params.thickness));
        trans *= absorption;
    }
    out_base_attenuation = trans;
}

// -----------------------------------------------------------------------------
// 7. Slang Advanced BSDF Interface & Modular Implementations
// -----------------------------------------------------------------------------

/**
 * @brief Unified interface for all Bidirectional Scattering Distribution Functions.
 * Provides evaluation for direct analytical rasterization and sampling/pdf for Monte Carlo ray tracing.
 */
public interface IBSDF
{
    float3 evaluate(float3 V, float3 L, float3 N);
    float3 sample(float3 V, float3 N, float2 u, out float3 out_L, out float out_pdf);
    float  pdf(float3 V, float3 L, float3 N);
};

/**
 * @brief Lambertian diffuse reflection component.
 */
public struct LambertianDiffuse : IBSDF
{
    public float3 albedo;

    public float3 evaluate(float3 V, float3 L, float3 N)
    {
        float NoL = max(dot(N, L), 0.0);
        return diffuse_lambert(albedo) * NoL;
    }

    public float3 sample(float3 V, float3 N, float2 u, out float3 out_L, out float out_pdf)
    {
        float3 local_L = sample_hemisphere_cosine(u);
        out_L = tangent_to_world(local_L, N);
        float NoL = max(dot(N, out_L), 0.0);
        out_pdf = pdf_hemisphere_cosine(NoL);
        return diffuse_lambert(albedo) * NoL;
    }

    public float pdf(float3 V, float3 L, float3 N)
    {
        float NoL = max(dot(N, L), 0.0);
        return pdf_hemisphere_cosine(NoL);
    }
};

/**
 * @brief Burley / Disney diffuse reflection component.
 */
public struct BurleyDiffuse : IBSDF
{
    public float3 albedo;
    public float  roughness;

    public float3 evaluate(float3 V, float3 L, float3 N)
    {
        float NoV = max(dot(N, V), 0.0);
        float NoL = max(dot(N, L), 0.0);
        if (NoV <= 0.0 || NoL <= 0.0) return float3(0.0);

        float3 H  = normalize(V + L);
        float LoH = max(dot(L, H), 0.0);
        return diffuse_burley(albedo, NoV, NoL, LoH, roughness) * NoL;
    }

    public float3 sample(float3 V, float3 N, float2 u, out float3 out_L, out float out_pdf)
    {
        float3 local_L = sample_hemisphere_cosine(u);
        out_L = tangent_to_world(local_L, N);
        float NoL = max(dot(N, out_L), 0.0);
        out_pdf = pdf_hemisphere_cosine(NoL);
        return evaluate(V, out_L, N);
    }

    public float pdf(float3 V, float3 L, float3 N)
    {
        float NoL = max(dot(N, L), 0.0);
        return pdf_hemisphere_cosine(NoL);
    }
};

/**
 * @brief Cook-Torrance microfacet GGX specular reflection component.
 */
public struct GGXSpecular : IBSDF
{
    public float3 F0;
    public float  roughness;

    public float3 evaluate(float3 V, float3 L, float3 N)
    {
        float NoV = max(dot(N, V), 0.0);
        float NoL = max(dot(N, L), 0.0);
        if (NoV <= 0.0 || NoL <= 0.0) return float3(0.0);

        float3 H = normalize(V + L);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);

        float  D = distribution_ggx(NoH, roughness);
        float  V_term = visibility_smith_ggx_correlated(NoV, NoL, roughness);
        float3 F = fresnel_schlick(VoH, F0);

        return (D * V_term * F) * NoL;
    }

    public float3 sample(float3 V, float3 N, float2 u, out float3 out_L, out float out_pdf)
    {
        float3 H = sample_ggx_vndf(u, roughness, V, N);
        out_L = normalize(2.0 * dot(V, H) * H - V);

        float NoV = max(dot(N, V), 0.0);
        float NoL = max(dot(N, out_L), 0.0);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);

        if (NoL <= 0.0 || VoH <= 0.0) {
            out_pdf = 0.0;
            return float3(0.0);
        }

        float D = distribution_ggx(NoH, roughness);
        out_pdf = pdf_ggx(D, NoH, VoH);

        return evaluate(V, out_L, N);
    }

    public float pdf(float3 V, float3 L, float3 N)
    {
        float NoV = max(dot(N, V), 0.0);
        float NoL = max(dot(N, L), 0.0);
        if (NoV <= 0.0 || NoL <= 0.0) return 0.0;

        float3 H = normalize(V + L);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);

        float D = distribution_ggx(NoH, roughness);
        return pdf_ggx(D, NoH, VoH);
    }
};

/**
 * @brief Slang Generic Decorator: layers a polyurethane clear coat over any base BSDF.
 */
public struct ClearCoatDecorator<TBase : IBSDF> : IBSDF
{
    public TBase base_bsdf;
    public ClearCoatParameters clear_coat;

    public float3 evaluate(float3 V, float3 L, float3 N)
    {
        float3 base_result = base_bsdf.evaluate(V, L, N);
        if (clear_coat.factor <= 0.0) {
            return base_result;
        }

        float3 H = normalize(V + L);
        float NoH = max(dot(N, H), 0.0);
        float NoV = max(dot(N, V), 0.0);
        float NoL = max(dot(N, L), 0.0);
        float VoH = max(dot(V, H), 0.0);
        float LoH = max(dot(L, H), 0.0);

        float3 cc_spec;
        float3 base_attenuation;
        evaluate_clear_coat(clear_coat, NoH, NoV, NoL, VoH, LoH, cc_spec, base_attenuation);

        return base_result * base_attenuation + cc_spec * NoL;
    }

    public float3 sample(float3 V, float3 N, float2 u, out float3 out_L, out float out_pdf)
    {
        float3 result = base_bsdf.sample(V, N, u, out_L, out_pdf);
        float VoN = max(dot(V, N), 0.0);
        float F = fresnel_clear_coat(VoN);
        return result * (1.0 - clear_coat.factor * F);
    }

    public float pdf(float3 V, float3 L, float3 N)
    {
        return base_bsdf.pdf(V, L, N);
    }
};

/**
 * @brief Standard production PBR metallic-roughness material model implementing IBSDF.
 */
public struct PBRMaterial : IBSDF
{
    public float3 base_color;
    public float  metallic;
    public float  roughness;
    public float  reflectance;
    public ClearCoatParameters clear_coat;

    public float3 evaluate(float3 V, float3 L, float3 N)
    {
        float NoV = max(dot(N, V), 0.0);
        float NoL = max(dot(N, L), 0.0);
        if (NoV <= 0.0 || NoL <= 0.0) {
            return float3(0.0);
        }

        float3 H = normalize(V + L);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);
        float LoH = max(dot(L, H), 0.0);

        float3 diffuse_color = base_color * (1.0 - metallic);
        float3 F0 = compute_f0(base_color, metallic, reflectance);

        // base specular (Cook-Torrance with height-correlated Smith GGX)
        float  D = distribution_ggx(NoH, roughness);
        float  V_term = visibility_smith_ggx_correlated(NoV, NoL, roughness);
        float3 F = fresnel_schlick(VoH, F0);
        float3 base_specular = D * V_term * F;

        // base diffuse (Burley)
        float3 base_diffuse = diffuse_burley(diffuse_color, NoV, NoL, LoH, roughness);
        float3 base_result  = (base_diffuse + base_specular) * NoL;

        // clear coat layer
        float3 cc_specular;
        float3 base_attenuation;
        evaluate_clear_coat(clear_coat, NoH, NoV, NoL, VoH, LoH, cc_specular, base_attenuation);

        return base_result * base_attenuation + cc_specular * NoL;
    }

    public float3 sample(float3 V, float3 N, float2 u, out float3 out_L, out float out_pdf)
    {
        if (u.x < (1.0 - metallic) * 0.5) {
            float2 u_sub = float2(u.x / ((1.0 - metallic) * 0.5), u.y);
            float3 local_L = sample_hemisphere_cosine(u_sub);
            out_L = tangent_to_world(local_L, N);
            float NoL = max(dot(N, out_L), 0.0);
            out_pdf = pdf_hemisphere_cosine(NoL);
            return evaluate(V, out_L, N);
        } else {
            float2 u_sub = float2((u.x - (1.0 - metallic) * 0.5) / (1.0 - (1.0 - metallic) * 0.5), u.y);
            float3 H = sample_ggx_vndf(u_sub, roughness, V, N);
            out_L = normalize(2.0 * dot(V, H) * H - V);

            float NoH = max(dot(N, H), 0.0);
            float VoH = max(dot(V, H), 0.0);
            float D = distribution_ggx(NoH, roughness);
            out_pdf = pdf_ggx(D, NoH, VoH);
            return evaluate(V, out_L, N);
        }
    }

    public float pdf(float3 V, float3 L, float3 N)
    {
        float NoL = max(dot(N, L), 0.0);
        if (NoL <= 0.0) return 0.0;

        float3 H = normalize(V + L);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);
        float D = distribution_ggx(NoH, roughness);

        float pdf_diff = pdf_hemisphere_cosine(NoL);
        float pdf_spec = pdf_ggx(D, NoH, VoH);

        return lerp(pdf_diff, pdf_spec, metallic);
    }
};

// -----------------------------------------------------------------------------
// 8. Direct Analytical Lighting Evaluator (BSDFResult)
// -----------------------------------------------------------------------------

public struct BSDFResult
{
    public float3 diffuse;
    public float3 specular_reflection;
    public float3 specular_transmission;
    public float3 clear_coat;

    public float3 total()
    {
        return diffuse + specular_reflection + specular_transmission + clear_coat;
    }
};

/**
 * @brief Evaluates combined standard metallic-roughness BSDF for direct analytical lighting.
 */
public BSDFResult evaluate_pbr_bsdf(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float  roughness,
    float  metallic,
    float  clear_coat_strength = 0.0,
    float  clear_coat_roughness = 0.04)
{
    BSDFResult result;

    float3 F0 = compute_f0(albedo, metallic, 0.5);
    float3 diffuse_albedo = albedo * (1.0 - metallic);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float3 H    = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    // diffuse
    result.diffuse = diffuse_burley(diffuse_albedo, NdotV, NdotL, LdotH, roughness);

    // specular reflection
    float  D = distribution_ggx(NdotH, roughness);
    float  V_term = visibility_smith_ggx_correlated(NdotV, NdotL, roughness);
    float3 F = fresnel_schlick(VdotH, F0);
    result.specular_reflection = D * V_term * F;

    // transmission
    result.specular_transmission = float3(0.0);

    // clear coat
    ClearCoatParameters cc;
    cc.factor = clear_coat_strength;
    cc.roughness = clear_coat_roughness;
    cc.tint = float3(0.0);
    cc.thickness = 0.0;

    float3 cc_spec;
    float3 attenuation;
    evaluate_clear_coat(cc, NdotH, NdotV, NdotL, VdotH, LdotH, cc_spec, attenuation);

    result.clear_coat = cc_spec;
    result.diffuse *= attenuation;
    result.specular_reflection *= attenuation;

    return result;
}

// -----------------------------------------------------------------------------
// 9. Split-Sum Environment BRDF Integration (Karis 2013)
// -----------------------------------------------------------------------------

/**
 * @brief Computes the 2D split-sum environment BRDF LUT value for a given (NoV, roughness).
 * Evaluates: scale (R) and bias (G) such that:
 * SpecularIBL = prefiltered_env_map * (F0 * scale + bias)
 * @param NoV Cosine of view angle [0, 1]
 * @param roughness Linear perceptual roughness [0, 1]
 * @param num_samples Monte Carlo sample count (typically 1024)
 * @return float2(scale, bias)
 */
public float2 integrate_brdf(float NoV, float roughness, uint num_samples = 1024)
{
    NoV = clamp(NoV, 1e-4, 1.0);
    float3 V = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);
    float3 N = float3(0.0, 0.0, 1.0);

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
            float V_pdf = (visibility_smith_ggx_correlated(NoV, NoL, roughness) * VoH) / max(NoH, EPSILON);
            float Fc = pow(1.0 - VoH, 5.0);

            scale += (1.0 - Fc) * V_pdf;
            bias  += Fc * V_pdf;
        }
    }

    return float2(scale, bias) * (4.0 / float(num_samples));
}
