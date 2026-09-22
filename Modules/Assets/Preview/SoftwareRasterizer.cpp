#include <cmath>
#include <limits>
#include <algorithm>
#include <filesystem>
#include <stb_image_write.h>
#include <stb_image_resize2.h>

#include <Lyra/Utilities/Logger.h>
#include "SoftwareRasterizer.h"

using namespace lyra;
using namespace lyra::preview;

namespace fs = std::filesystem;

static Logger get_logger()
{
    static Logger logger = create_logger("Preview", LogLevel::trace);
    return logger;
}

struct PipelinePreviewConfig
{
    Path    caches_root;
    uint    width       = 128;
    uint    height      = 128;
    float   pitch_deg   = 25.0f;
    float   yaw_deg     = 45.0f;
    float   fov_deg     = 35.0f;
    Vector4 clear_color = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    bool    enable_ssaa = true;
};

static PipelinePreviewConfig G_CONFIG;

void lyra::preview::configure(AssetServer*, const JSON& options)
{
    if (options.contains("caches_root") && options["caches_root"].is_string()) {
        G_CONFIG.caches_root = options["caches_root"].get<String>();
    }
    if (options.contains("width") && options["width"].is_number()) {
        G_CONFIG.width = options["width"].get<uint>();
    }
    if (options.contains("height") && options["height"].is_number()) {
        G_CONFIG.height = options["height"].get<uint>();
    }
    if (options.contains("enable_ssaa") && options["enable_ssaa"].is_boolean()) {
        G_CONFIG.enable_ssaa = options["enable_ssaa"].get<bool>();
    }
    if (options.contains("fov_deg") && options["fov_deg"].is_number()) {
        G_CONFIG.fov_deg = options["fov_deg"].get<float>();
    }
    if (options.contains("pitch_deg") && options["pitch_deg"].is_number()) {
        G_CONFIG.pitch_deg = options["pitch_deg"].get<float>();
    }
    if (options.contains("yaw_deg") && options["yaw_deg"].is_number()) {
        G_CONFIG.yaw_deg = options["yaw_deg"].get<float>();
    }
}

static Vector4 sample_bilinear(const PreviewTexture& tex, Vector2 uv)
{
    if (tex.pixels.empty() || tex.width == 0 || tex.height == 0) {
        return Vector4(1.0f);
    }

    // repeat wrap
    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    float tx = u * static_cast<float>(tex.width - 1);
    float ty = v * static_cast<float>(tex.height - 1);

    int x0 = static_cast<int>(tx);
    int y0 = static_cast<int>(ty);
    int x1 = std::min(x0 + 1, static_cast<int>(tex.width - 1));
    int y1 = std::min(y0 + 1, static_cast<int>(tex.height - 1));

    float fx = tx - static_cast<float>(x0);
    float fy = ty - static_cast<float>(y0);

    auto get_pixel = [&](int x, int y) -> Vector4 {
        size_t idx = static_cast<size_t>(y * tex.width + x) * tex.channels;
        if (idx >= tex.pixels.size()) return Vector4(1.0f);

        float r = tex.pixels[idx + 0] / 255.0f;
        float g = (tex.channels >= 2 && idx + 1 < tex.pixels.size()) ? tex.pixels[idx + 1] / 255.0f : r;
        float b = (tex.channels >= 3 && idx + 2 < tex.pixels.size()) ? tex.pixels[idx + 2] / 255.0f : r;
        float a = (tex.channels >= 4 && idx + 3 < tex.pixels.size()) ? tex.pixels[idx + 3] / 255.0f : 1.0f;
        return Vector4(r, g, b, a);
    };

    Vector4 c00 = get_pixel(x0, y0);
    Vector4 c10 = get_pixel(x1, y0);
    Vector4 c01 = get_pixel(x0, y1);
    Vector4 c11 = get_pixel(x1, y1);

    Vector4 c0 = glm::mix(c00, c10, fx);
    Vector4 c1 = glm::mix(c01, c11, fx);
    return glm::mix(c0, c1, fy);
}

auto lyra::preview::rasterize_scene(const PreviewScene& scene) -> PreviewTexture
{
    PreviewTexture result;
    result.width    = G_CONFIG.width;
    result.height   = G_CONFIG.height;
    result.channels = 4;
    result.pixels.resize(result.width * result.height * 4, 0);

    // fast path: 2D flat textured quad (image/texture preview)
    if (scene.is_flat && !scene.textures.empty()) {
        const auto& tex = scene.textures[0];
        if (tex.width > 0 && tex.height > 0 && !tex.pixels.empty()) {
            int thumb_w = static_cast<int>(G_CONFIG.width);
            int thumb_h = static_cast<int>(G_CONFIG.height);

            // maintain aspect ratio
            if (tex.width > tex.height) {
                thumb_h = std::max(1, static_cast<int>(static_cast<float>(tex.height) / tex.width * G_CONFIG.width));
            } else {
                thumb_w = std::max(1, static_cast<int>(static_cast<float>(tex.width) / tex.height * G_CONFIG.height));
            }

            Vector<uint8_t> resized(thumb_w * thumb_h * 4);
            stbir_pixel_layout layout = (tex.channels == 1) ? STBIR_1CHANNEL :
                                        (tex.channels == 2) ? STBIR_2CHANNEL :
                                        (tex.channels == 3) ? STBIR_RGB : STBIR_RGBA;

            stbir_resize_uint8_linear(
                tex.pixels.data(), tex.width, tex.height, 0,
                resized.data(), thumb_w, thumb_h, 0,
                STBIR_RGBA);

            result.width  = thumb_w;
            result.height = thumb_h;
            result.pixels = std::move(resized);
            return result;
        }
    }

    // 1. calculate cumulative bounds
    auto [aabb_min, aabb_max] = scene.calculate_bounds();
    if (aabb_min.x > aabb_max.x) {
        return result;
    }

    Vector3 center     = (aabb_min + aabb_max) * 0.5f;
    Vector3 extents    = aabb_max - aabb_min;
    float   max_extent = std::max({extents.x, extents.y, extents.z});
    if (max_extent < 1e-6f) {
        return result;
    }

    // 2. normalization transform
    float norm_scale = 2.0f / max_extent;

    // 3. camera setup
    float fov_rad = glm::radians(G_CONFIG.fov_deg);
    float dist    = (1.732f / std::tan(fov_rad * 0.5f)) * 1.15f;

    float   pitch = glm::radians(G_CONFIG.pitch_deg);
    float   yaw   = glm::radians(G_CONFIG.yaw_deg);
    Vector3 eye(
        dist * std::cos(pitch) * std::sin(yaw),
        dist * std::sin(pitch),
        dist * std::cos(pitch) * std::cos(yaw));

    Matrix4x4 view = glm::lookAt(eye, Vector3(0.0f), Vector3(0.0f, 1.0f, 0.0f));

    uint ssaa_factor = G_CONFIG.enable_ssaa ? 2 : 1;
    uint render_w    = G_CONFIG.width * ssaa_factor;
    uint render_h    = G_CONFIG.height * ssaa_factor;

    Matrix4x4 proj = glm::perspective(fov_rad, static_cast<float>(render_w) / static_cast<float>(render_h), 0.1f, 100.0f);
    Matrix4x4 vp   = proj * view;

    // 4. frame buffers
    Vector<float>   depth_buffer(render_w * render_h, 1.0f);
    Vector<uint8_t> color_buffer(render_w * render_h * 4, 0);

    for (uint i = 0; i < render_w * render_h; ++i) {
        color_buffer[i * 4 + 0] = static_cast<uint8_t>(G_CONFIG.clear_color.r * 255.0f);
        color_buffer[i * 4 + 1] = static_cast<uint8_t>(G_CONFIG.clear_color.g * 255.0f);
        color_buffer[i * 4 + 2] = static_cast<uint8_t>(G_CONFIG.clear_color.b * 255.0f);
        color_buffer[i * 4 + 3] = static_cast<uint8_t>(G_CONFIG.clear_color.a * 255.0f);
    }

    Vector3 light_key  = glm::normalize(Vector3( 0.5f,  0.8f,  0.6f));
    Vector3 light_fill = glm::normalize(Vector3(-0.5f, -0.2f, -0.5f));
    Vector3 clay_color = Vector3(0.72f, 0.75f, 0.78f);

    // 5. rasterize meshes
    for (const auto& mesh : scene.meshes) {
        if (mesh.positions.empty()) continue;

        const PreviewMaterial* mat = nullptr;
        if (mesh.material_id >= 0 && static_cast<size_t>(mesh.material_id) < scene.materials.size()) {
            mat = &scene.materials[mesh.material_id];
        }

        const PreviewTexture* tex = nullptr;
        if (mat && mat->albedo_texture_id >= 0 && static_cast<size_t>(mat->albedo_texture_id) < scene.textures.size()) {
            tex = &scene.textures[mat->albedo_texture_id];
        }

        bool has_normals = !mesh.normals.empty() && mesh.normals.size() == mesh.positions.size();
        bool has_uvs     = !mesh.uvs.empty() && mesh.uvs.size() == mesh.positions.size();

        size_t num_triangles = mesh.indices.empty() ? (mesh.positions.size() / 3) : (mesh.indices.size() / 3);

        for (size_t t = 0; t < num_triangles; ++t) {
            uint idx0 = mesh.indices.empty() ? static_cast<uint>(t * 3 + 0) : mesh.indices[t * 3 + 0];
            uint idx1 = mesh.indices.empty() ? static_cast<uint>(t * 3 + 1) : mesh.indices[t * 3 + 1];
            uint idx2 = mesh.indices.empty() ? static_cast<uint>(t * 3 + 2) : mesh.indices[t * 3 + 2];

            if (idx0 >= mesh.positions.size() || idx1 >= mesh.positions.size() || idx2 >= mesh.positions.size()) {
                continue;
            }

            Vector4 wp0 = mesh.transform * Vector4(mesh.positions[idx0], 1.0f);
            Vector4 wp1 = mesh.transform * Vector4(mesh.positions[idx1], 1.0f);
            Vector4 wp2 = mesh.transform * Vector4(mesh.positions[idx2], 1.0f);

            Vector3 norm_p0 = (Vector3(wp0) - center) * norm_scale;
            Vector3 norm_p1 = (Vector3(wp1) - center) * norm_scale;
            Vector3 norm_p2 = (Vector3(wp2) - center) * norm_scale;

            Vector4 clip0 = vp * Vector4(norm_p0, 1.0f);
            Vector4 clip1 = vp * Vector4(norm_p1, 1.0f);
            Vector4 clip2 = vp * Vector4(norm_p2, 1.0f);

            if (clip0.w <= 0.001f || clip1.w <= 0.001f || clip2.w <= 0.001f) {
                continue;
            }

            Vector3 ndc0 = Vector3(clip0) / clip0.w;
            Vector3 ndc1 = Vector3(clip1) / clip1.w;
            Vector3 ndc2 = Vector3(clip2) / clip2.w;

            if ((ndc0.x < -1.0f && ndc1.x < -1.0f && ndc2.x < -1.0f) ||
                (ndc0.x >  1.0f && ndc1.x >  1.0f && ndc2.x >  1.0f) ||
                (ndc0.y < -1.0f && ndc1.y < -1.0f && ndc2.y < -1.0f) ||
                (ndc0.y >  1.0f && ndc1.y >  1.0f && ndc2.y >  1.0f) ||
                (ndc0.z <  0.0f && ndc1.z <  0.0f && ndc2.z <  0.0f) ||
                (ndc0.z >  1.0f && ndc1.z >  1.0f && ndc2.z >  1.0f)) {
                continue;
            }

            auto to_screen = [&](const Vector3& ndc) -> Vector2 {
                return Vector2(
                    (ndc.x * 0.5f + 0.5f) * static_cast<float>(render_w),
                    (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(render_h));
            };

            Vector2 s0 = to_screen(ndc0);
            Vector2 s1 = to_screen(ndc1);
            Vector2 s2 = to_screen(ndc2);

            float area = (s1.x - s0.x) * (s2.y - s0.y) - (s1.y - s0.y) * (s2.x - s0.x);
            if (std::abs(area) < 1e-5f) {
                continue;
            }

            Vector3 n0, n1, n2;
            if (has_normals) {
                Matrix3x3 norm_mat = glm::transpose(glm::inverse(Matrix3x3(mesh.transform)));
                n0 = glm::normalize(norm_mat * mesh.normals[idx0]);
                n1 = glm::normalize(norm_mat * mesh.normals[idx1]);
                n2 = glm::normalize(norm_mat * mesh.normals[idx2]);
            } else {
                Vector3 geom_n = glm::normalize(glm::cross(norm_p1 - norm_p0, norm_p2 - norm_p0));
                n0 = n1 = n2 = geom_n;
            }

            Vector2 uv0 = has_uvs ? mesh.uvs[idx0] : Vector2(0.0f);
            Vector2 uv1 = has_uvs ? mesh.uvs[idx1] : Vector2(0.0f);
            Vector2 uv2 = has_uvs ? mesh.uvs[idx2] : Vector2(0.0f);

            float inv_w0 = 1.0f / clip0.w;
            float inv_w1 = 1.0f / clip1.w;
            float inv_w2 = 1.0f / clip2.w;

            Vector2 uv0_w = uv0 * inv_w0;
            Vector2 uv1_w = uv1 * inv_w1;
            Vector2 uv2_w = uv2 * inv_w2;

            int min_x = std::max(0, static_cast<int>(std::floor(std::min({s0.x, s1.x, s2.x}))));
            int max_x = std::min(static_cast<int>(render_w) - 1, static_cast<int>(std::ceil(std::max({s0.x, s1.x, s2.x}))));
            int min_y = std::max(0, static_cast<int>(std::floor(std::min({s0.y, s1.y, s2.y}))));
            int max_y = std::min(static_cast<int>(render_h) - 1, static_cast<int>(std::ceil(std::max({s0.y, s1.y, s2.y}))));

            for (int py = min_y; py <= max_y; ++py) {
                for (int px = min_x; px <= max_x; ++px) {
                    Vector2 p(static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f);

                    float w0 = ((s1.x - p.x) * (s2.y - p.y) - (s1.y - p.y) * (s2.x - p.x)) / area;
                    float w1 = ((s2.x - p.x) * (s0.y - p.y) - (s2.y - p.y) * (s0.x - p.x)) / area;
                    float w2 = 1.0f - w0 - w1;

                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                        float depth = w0 * ndc0.z + w1 * ndc1.z + w2 * ndc2.z;
                        size_t pixel_idx = static_cast<size_t>(py * render_w + px);

                        if (depth >= 0.0f && depth < depth_buffer[pixel_idx]) {
                            depth_buffer[pixel_idx] = depth;

                            Vector3 norm = glm::normalize(w0 * n0 + w1 * n1 + w2 * n2);
                            Vector3 view_dir = glm::normalize(eye);
                            if (glm::dot(norm, view_dir) < 0.0f) {
                                norm = -norm;
                            }

                            float diff_key  = std::max(0.0f, glm::dot(norm, light_key)) * 0.70f;
                            float diff_fill = std::max(0.0f, glm::dot(norm, light_fill)) * 0.25f;
                            float ambient   = 0.25f;
                            float light     = ambient + diff_key + diff_fill;

                            float rim = std::pow(1.0f - std::max(0.0f, glm::dot(norm, view_dir)), 3.0f) * 0.15f;

                            Vector4 albedo(clay_color, 1.0f);
                            if (mat) {
                                albedo = mat->base_color_factor;
                            }

                            if (has_uvs && tex) {
                                float inv_w = w0 * inv_w0 + w1 * inv_w1 + w2 * inv_w2;
                                Vector2 uv = (w0 * uv0_w + w1 * uv1_w + w2 * uv2_w) / (inv_w != 0.0f ? inv_w : 1.0f);
                                Vector4 tex_sample = sample_bilinear(*tex, uv);
                                albedo *= tex_sample;
                            }

                            Vector3 final_color = glm::clamp(Vector3(albedo) * light + Vector3(rim), 0.0f, 1.0f);

                            size_t c_idx            = static_cast<size_t>(pixel_idx) * 4;
                            color_buffer[c_idx + 0] = static_cast<uint8_t>(final_color.r * 255.0f);
                            color_buffer[c_idx + 1] = static_cast<uint8_t>(final_color.g * 255.0f);
                            color_buffer[c_idx + 2] = static_cast<uint8_t>(final_color.b * 255.0f);
                            color_buffer[c_idx + 3] = static_cast<uint8_t>(albedo.a * 255.0f);
                        }
                    }
                }
            }
        }
    }

    // 6. downsample if SSAA was enabled
    if (G_CONFIG.enable_ssaa && ssaa_factor > 1) {
        for (uint y = 0; y < G_CONFIG.height; ++y) {
            for (uint x = 0; x < G_CONFIG.width; ++x) {
                int src_x = static_cast<int>(x * 2);
                int src_y = static_cast<int>(y * 2);

                uint r = 0, g = 0, b = 0, a = 0;
                for (int dy = 0; dy < 2; ++dy) {
                    for (int dx = 0; dx < 2; ++dx) {
                        size_t idx = static_cast<size_t>((src_y + dy) * render_w + (src_x + dx)) * 4;
                        r += color_buffer[idx + 0];
                        g += color_buffer[idx + 1];
                        b += color_buffer[idx + 2];
                        a += color_buffer[idx + 3];
                    }
                }

                size_t dst_idx             = static_cast<size_t>(y * G_CONFIG.width + x) * 4;
                result.pixels[dst_idx + 0] = static_cast<uint8_t>(r / 4);
                result.pixels[dst_idx + 1] = static_cast<uint8_t>(g / 4);
                result.pixels[dst_idx + 2] = static_cast<uint8_t>(b / 4);
                result.pixels[dst_idx + 3] = static_cast<uint8_t>(a / 4);
            }
        }
    } else {
        result.pixels = std::move(color_buffer);
    }

    return result;
}

auto lyra::preview::generate_scene_thumbnail(
    const PreviewScene& scene,
    JSON&               metadata) -> Path
{
    if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
        return Path();
    }

    auto guid = metadata["guid"].get<AssetID>();
    Path caches_root = G_CONFIG.caches_root;
    if (caches_root.empty()) {
        get_logger()->error("Preview generator caches_root not configured!");
        return Path();
    }

    Path thumb_dir = caches_root / "thumbnails";
    fs::create_directories(thumb_dir);

    Path thumb_path = thumb_dir / (std::to_string(guid) + ".thumb.png");

    PreviewTexture rendered = rasterize_scene(scene);
    if (!stbi_write_png(thumb_path.string().c_str(), rendered.width, rendered.height, 4, rendered.pixels.data(), rendered.width * 4)) {
        get_logger()->warn("Failed to write preview thumbnail: {}", thumb_path.string());
        return Path();
    }

    Path rel_path = fs::relative(thumb_path, caches_root);
    metadata["thumbnail"] = rel_path.generic_string();
    return rel_path;
}
