#include "IBLCooker.h"
#include "TextureUtils.h"
#include <Lyra/JobSystem/JobScheduler.h>
#include <Lyra/Utilities/Sync.h>
#include <algorithm>
#include <cmath>
#include <thread>

using namespace lyra;
using namespace lyra::texture;

namespace
{
    constexpr float PI = 3.14159265358979323846f;

    // Hammersley 2D low-discrepancy sequence
    inline float radical_inverse_vdc(uint bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f; // / 0x100000000
    }

    inline Vector2 hammersley_2d(uint i, uint n)
    {
        return Vector2(static_cast<float>(i) / static_cast<float>(n), radical_inverse_vdc(i));
    }

    // Direction vector for face f in [0..5] and normalized cube coords u, v in [-1, 1]
    inline Vector3 get_cube_direction(int face, float u, float v)
    {
        Vector3 dir(0.0f);
        // clang-format off
        switch (face) {
            case 0: dir = Vector3(1.0f, -v, -u); break;  // +X
            case 1: dir = Vector3(-1.0f, -v, u); break;  // -X
            case 2: dir = Vector3(u, 1.0f, v); break;   // +Y
            case 3: dir = Vector3(u, -1.0f, -v); break;  // -Y
            case 4: dir = Vector3(u, -v, 1.0f); break;   // +Z
            case 5: dir = Vector3(-u, -v, -1.0f); break; // -Z
            default: break;
        }
        // clang-format on
        return glm::normalize(dir);
    }

    // Bilinear sample from equirectangular HDR image
    inline Vector4 sample_equirectangular(const float* hdr_rgba, int width, int height, const Vector3& dir)
    {
        if (!hdr_rgba || width <= 0 || height <= 0) {
            return Vector4(0.0f);
        }

        float phi   = std::atan2(dir.z, dir.x);
        float theta = std::asin(std::clamp(dir.y, -1.0f, 1.0f));

        float u = phi / (2.0f * PI) + 0.5f;
        float v = theta / PI + 0.5f;

        u = u - std::floor(u);
        v = std::clamp(v, 0.0f, 1.0f);

        float fx = u * static_cast<float>(width) - 0.5f;
        float fy = v * static_cast<float>(height) - 0.5f;

        int x0 = static_cast<int>(std::floor(fx));
        int y0 = static_cast<int>(std::floor(fy));
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        float wx = fx - static_cast<float>(x0);
        float wy = fy - static_cast<float>(y0);

        x0 = (x0 % width + width) % width;
        x1 = (x1 % width + width) % width;
        y0 = std::clamp(y0, 0, height - 1);
        y1 = std::clamp(y1, 0, height - 1);

        auto fetch = [&](int x, int y) -> Vector4 {
            const float* p = &hdr_rgba[(y * width + x) * 4];
            return Vector4(p[0], p[1], p[2], p[3]);
        };

        Vector4 c00 = fetch(x0, y0);
        Vector4 c10 = fetch(x1, y0);
        Vector4 c01 = fetch(x0, y1);
        Vector4 c11 = fetch(x1, y1);

        Vector4 c0 = glm::mix(c00, c10, wx);
        Vector4 c1 = glm::mix(c01, c11, wx);
        return glm::mix(c0, c1, wy);
    }

    // Importance sample GGX direction
    inline auto importance_sample_ggx(const Vector2& xi, const Vector3& N, float roughness) -> Vector3
    {
        float a = roughness * roughness;

        float phi       = 2.0f * PI * xi.x;
        float cos_theta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
        float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));

        Vector3 H_local(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta);

        Vector3 up        = std::abs(N.y) < 0.999f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, 0.0f, 1.0f);
        Vector3 tangent   = glm::normalize(glm::cross(up, N));
        Vector3 bitangent = glm::cross(N, tangent);

        return glm::normalize(tangent * H_local.x + bitangent * H_local.y + N * H_local.z);
    }
} // namespace

void lyra::texture::compute_sh9_from_equirectangular(
    const float*       hdr_rgba,
    int                width,
    int                height,
    Array<Vector4, 9>& out_sh)
{
    out_sh.fill(Vector4(0.0f));

    if (!hdr_rgba || width <= 0 || height <= 0) {
        return;
    }

    // Riemann sum integration over the sphere
    Array<Vector3, 9> accum{};
    accum.fill(Vector3(0.0f));

    for (int y = 0; y < height; ++y) {
        float theta   = (static_cast<float>(y) + 0.5f) / static_cast<float>(height) * PI - (PI * 0.5f);
        float d_omega = std::cos(theta) * (2.0f * PI / static_cast<float>(width)) * (PI / static_cast<float>(height));

        for (int x = 0; x < width; ++x) {
            float phi = (static_cast<float>(x) + 0.5f) / static_cast<float>(width) * 2.0f * PI - PI;

            Vector3 d(std::cos(theta) * std::cos(phi), std::sin(theta), std::cos(theta) * std::sin(phi));
            d = glm::normalize(d);

            const float* p = &hdr_rgba[(y * width + x) * 4];
            Vector3      radiance(p[0], p[1], p[2]);

            // 9 real SH basis functions Y_lm
            float sh[9];
            sh[0] = 0.28209479f;
            sh[1] = 0.48860251f * d.y;
            sh[2] = 0.48860251f * d.z;
            sh[3] = 0.48860251f * d.x;
            sh[4] = 1.09254843f * (d.x * d.y);
            sh[5] = 1.09254843f * (d.y * d.z);
            sh[6] = 0.31539156f * (3.0f * d.z * d.z - 1.0f);
            sh[7] = 1.09254843f * (d.x * d.z);
            sh[8] = 0.54627421f * (d.x * d.x - d.y * d.y);

            for (int i = 0; i < 9; ++i) {
                accum[i] += radiance * (sh[i] * d_omega);
            }
        }
    }

    // Convolve with clamped cosine lobe to convert radiance into irradiance coefficients:
    // A_0 = PI, A_1 = 2*PI / 3, A_2 = PI / 4
    constexpr float A0 = PI;
    constexpr float A1 = (2.0f * PI) / 3.0f;
    constexpr float A2 = PI / 4.0f;

    out_sh[0] = Vector4(accum[0] * A0, 1.0f);

    out_sh[1] = Vector4(accum[1] * A1, 1.0f);
    out_sh[2] = Vector4(accum[2] * A1, 1.0f);
    out_sh[3] = Vector4(accum[3] * A1, 1.0f);

    out_sh[4] = Vector4(accum[4] * A2, 1.0f);
    out_sh[5] = Vector4(accum[5] * A2, 1.0f);
    out_sh[6] = Vector4(accum[6] * A2, 1.0f);
    out_sh[7] = Vector4(accum[7] * A2, 1.0f);
    out_sh[8] = Vector4(accum[8] * A2, 1.0f);
}

void lyra::texture::prefilter_environment_cubemap(
    const float*     hdr_rgba,
    int              src_width,
    int              src_height,
    int              base_face_size,
    int              num_mips,
    CubemapMipChain& out_cubemap)
{
    out_cubemap.init(static_cast<uint>(base_face_size), static_cast<uint>(num_mips));

    auto process_face = [&](uint mip, uint face) {
        int   face_size = std::max(1, base_face_size >> mip);
        float roughness = (num_mips > 1) ? static_cast<float>(mip) / static_cast<float>(num_mips - 1) : 0.0f;

        auto   slice      = out_cubemap.get_face_slice(mip, face);
        float* dst_pixels = slice.data();

        for (int y = 0; y < face_size; ++y) {
            float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(face_size) * 2.0f - 1.0f;

            for (int x = 0; x < face_size; ++x) {
                float   u = (static_cast<float>(x) + 0.5f) / static_cast<float>(face_size) * 2.0f - 1.0f;
                Vector3 N = get_cube_direction(static_cast<int>(face), u, v);
                Vector3 R = N;
                Vector3 V = R;

                if (roughness <= 1e-4f) {
                    Vector4 s           = sample_equirectangular(hdr_rgba, src_width, src_height, R);
                    int     idx         = (y * face_size + x) * 4;
                    dst_pixels[idx + 0] = s.r;
                    dst_pixels[idx + 1] = s.g;
                    dst_pixels[idx + 2] = s.b;
                    dst_pixels[idx + 3] = 1.0f;
                } else {
                    constexpr uint NUM_SAMPLES = 64;
                    Vector3        prefiltered(0.0f);
                    float          total_weight = 0.0f;

                    for (uint i = 0; i < NUM_SAMPLES; ++i) {
                        Vector2 xi = hammersley_2d(i, NUM_SAMPLES);
                        Vector3 H  = importance_sample_ggx(xi, N, roughness);
                        Vector3 L  = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                        float NdotL = std::max(glm::dot(N, L), 0.0f);
                        if (NdotL > 0.0f) {
                            Vector4 sample_val = sample_equirectangular(hdr_rgba, src_width, src_height, L);
                            prefiltered += Vector3(sample_val.r, sample_val.g, sample_val.b) * NdotL;
                            total_weight += NdotL;
                        }
                    }

                    if (total_weight > 0.0f) {
                        prefiltered /= total_weight;
                    } else {
                        Vector4 s   = sample_equirectangular(hdr_rgba, src_width, src_height, R);
                        prefiltered = Vector3(s.r, s.g, s.b);
                    }

                    int idx             = (y * face_size + x) * 4;
                    dst_pixels[idx + 0] = prefiltered.r;
                    dst_pixels[idx + 1] = prefiltered.g;
                    dst_pixels[idx + 2] = prefiltered.b;
                    dst_pixels[idx + 3] = 1.0f;
                }
            }
        }
    };

    if (JobScheduler::is_initialized()) {
        Atomic<uint> remaining_tasks{static_cast<uint>(num_mips * 6)};
        for (uint mip = 0; mip < static_cast<uint>(num_mips); ++mip) {
            for (uint face = 0; face < 6; ++face) {
                JobScheduler::schedule(JobPriority::HIGH, [&, mip, face]() {
                    process_face(mip, face);
                    remaining_tasks.fetch_sub(1, memory_order_release);
                });
            }
        }

        while (remaining_tasks.load(memory_order_acquire) > 0) {
            if (!assist_work()) {
                std::this_thread::yield();
            }
        }
    } else {
        for (uint mip = 0; mip < static_cast<uint>(num_mips); ++mip) {
            for (uint face = 0; face < 6; ++face) {
                process_face(mip, face);
            }
        }
    }
}

bool lyra::texture::save_cubemap_to_ktx2(
    const CubemapMipChain& cubemap,
    const Path&            target_path,
    Logger                 logger)
{
    try {
        fs::create_directories(target_path.parent_path());
    } catch (const std::exception& e) {
        if (logger) logger->error("Failed to create directories for {}: {}", target_path.string(), e.what());
        return false;
    }

    ktxTexture2*         ktx_tex = nullptr;
    ktxTextureCreateInfo create_info{};
    create_info.glInternalformat = 0;
    create_info.vkFormat         = VK_FORMAT_R32G32B32A32_SFLOAT;
    create_info.baseWidth        = static_cast<uint32_t>(cubemap.base_size);
    create_info.baseHeight       = static_cast<uint32_t>(cubemap.base_size);
    create_info.baseDepth        = 1;
    create_info.numDimensions    = 2;
    create_info.numLevels        = static_cast<uint32_t>(cubemap.num_mips);
    create_info.numLayers        = 1;
    create_info.numFaces         = 6;
    create_info.isArray          = KTX_FALSE;
    create_info.generateMipmaps  = KTX_FALSE;

    KTX_error_code result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &ktx_tex);
    if (result != KTX_SUCCESS || !ktx_tex) {
        if (logger) logger->error("Failed to create KTX2 cubemap texture: {}", ktxErrorString(result));
        return false;
    }

    for (uint mip = 0; mip < cubemap.num_mips; ++mip) {
        uint face_size = std::max(1u, cubemap.base_size >> mip);
        for (uint face = 0; face < 6; ++face) {
            auto       slice     = cubemap.get_face_slice(mip, face);
            ktx_size_t data_size = face_size * face_size * 4 * sizeof(float);
            result               = ktxTexture_SetImageFromMemory(
                ktxTexture(ktx_tex),
                static_cast<uint32_t>(mip),
                0,
                static_cast<uint32_t>(face),
                reinterpret_cast<const ktx_uint8_t*>(slice.data()),
                data_size);
            if (result != KTX_SUCCESS) {
                if (logger) logger->error("Failed to set image for cubemap mip {} face {}: {}", mip, face, ktxErrorString(result));
                ktxTexture_Destroy(ktxTexture(ktx_tex));
                return false;
            }
        }
    }

    JSON dummy_meta;
    return save_to_ktx2(dummy_meta, ktx_tex, target_path, target_path.parent_path().string().c_str(), logger);
}

IBLCookResult lyra::texture::process_environment_map(
    JSON&        metadata,
    const float* hdr_rgba,
    int          width,
    int          height,
    OSPath       caches_root,
    Logger       logger)
{
    IBLCookResult res{};
    if (!hdr_rgba || width <= 0 || height <= 0) {
        return res;
    }

    if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
        if (logger) logger->error("Missing or invalid GUID in metadata");
        return res;
    }

    AssetID guid        = metadata["guid"].get<AssetID>();
    Path    target_path = Path(caches_root) / "textures" / (std::to_string(guid) + "_cubemap.ktx2");

    // 1. Compute SH9 diffuse irradiance
    compute_sh9_from_equirectangular(hdr_rgba, width, height, res.sh9);

    // 2. Prefilter cubemap mips
    CubemapMipChain cubemap;
    prefilter_environment_cubemap(hdr_rgba, width, height, res.base_resolution, res.mip_levels, cubemap);

    // 3. Save cubemap to KTX2
    if (!save_cubemap_to_ktx2(cubemap, target_path, logger)) {
        return res;
    }

    res.cubemap_path = target_path;
    res.success      = true;

    // 4. Update metadata
    metadata["path"] = fs::relative(target_path, caches_root).string();
    metadata["sh9"]  = JSON::array();
    for (int i = 0; i < 9; ++i) {
        metadata["sh9"].push_back({res.sh9[i].x, res.sh9[i].y, res.sh9[i].z, res.sh9[i].w});
    }

    return res;
}
