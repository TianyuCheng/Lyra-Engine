#include <cmath>
#include <limits>
#include <algorithm>
#include <filesystem>
#include <stb_image_write.h>

#include "ModelUtils.h"
#include "ModelRasterizer.h"

using namespace lyra;
using namespace lyra::model;

bool lyra::model::generate_model_thumbnail(
    JSON&                   metadata,
    const Vector<Vector3>&  positions,
    const Vector<Vector3>&  normals,
    const Vector<uint32_t>& indices,
    OSPath                  caches_root)
{
    RasterizerMesh mesh;
    mesh.positions = positions;
    mesh.normals   = normals;
    mesh.indices   = indices;
    mesh.transform = Matrix4x4(1.0f);
    return generate_model_thumbnail(metadata, Vector<RasterizerMesh>{mesh}, caches_root);
}

bool lyra::model::generate_model_thumbnail(
    JSON&                         metadata,
    const Vector<RasterizerMesh>& meshes,
    OSPath                        caches_root)
{
    // 1. Calculate cumulative bounding box
    Vector3 aabb_min(std::numeric_limits<float>::max());
    Vector3 aabb_max(std::numeric_limits<float>::lowest());
    size_t  total_indices = 0;

    for (const auto& mesh : meshes) {
        total_indices += mesh.indices.size();
        for (const auto& p : mesh.positions) {
            Vector4 world_p = mesh.transform * Vector4(p, 1.0f);
            Vector3 wp      = Vector3(world_p) / (world_p.w != 0.0f ? world_p.w : 1.0f);
            aabb_min        = glm::min(aabb_min, wp);
            aabb_max        = glm::max(aabb_max, wp);
        }
    }

    if (total_indices == 0 || aabb_min.x > aabb_max.x) {
        return false;
    }

    Vector3 center     = (aabb_min + aabb_max) * 0.5f;
    Vector3 extents    = aabb_max - aabb_min;
    float   max_extent = std::max({extents.x, extents.y, extents.z});
    if (max_extent < 1e-6f) {
        return false;
    }

    // 2. Normalization transform (center at origin, scale largest extent to [-1, 1])
    float norm_scale = 2.0f / max_extent;

    // 3. Setup Camera (canonical 3/4 perspective)
    constexpr float fov_deg = 35.0f;
    float           fov_rad = glm::radians(fov_deg);
    // Distance to frame bounding sphere of radius ~1.732 (sqrt(3)) with 15% safety margin
    float dist = (1.732f / std::tan(fov_rad * 0.5f)) * 1.15f;

    float   pitch = glm::radians(25.0f);
    float   yaw   = glm::radians(45.0f);
    Vector3 eye(
        dist * std::cos(pitch) * std::sin(yaw),
        dist * std::sin(pitch),
        dist * std::cos(pitch) * std::cos(yaw));
    Vector3 target(0.0f, 0.0f, 0.0f);
    Vector3 up(0.0f, 1.0f, 0.0f);

    Matrix4x4 view = glm::lookAt(eye, target, up);
    Matrix4x4 proj = glm::perspective(fov_rad, 1.0f, 0.1f, dist * 4.0f);
    Matrix4x4 vp   = proj * view;

    // 4. Rasterization setup (Render at 256x256 for 2x SSAA, downsample to 128x128)
    constexpr int RENDER_W = 256;
    constexpr int RENDER_H = 256;
    constexpr int THUMB_W  = 128;
    constexpr int THUMB_H  = 128;

    Vector<float>   depth_buffer(RENDER_W * RENDER_H, 1.0f);
    Vector<uint8_t> color_buffer(RENDER_W * RENDER_H * 4, 0);

    Vector3 light_key  = glm::normalize(Vector3(0.577f, 0.707f, 0.408f));
    Vector3 light_fill = glm::normalize(Vector3(-0.577f, -0.2f, -0.707f));
    Vector3 clay_color = Vector3(0.78f, 0.81f, 0.86f);

    // Process each mesh
    for (const auto& mesh : meshes) {
        if (mesh.indices.empty() || mesh.positions.empty()) continue;

        // Combined model matrix: Normalize(Translate * MeshTransform)
        Matrix4x4 model = glm::scale(Matrix4x4(1.0f), Vector3(norm_scale)) *
                          glm::translate(Matrix4x4(1.0f), -center) *
                          mesh.transform;
        Matrix4x4 mvp           = vp * model;
        Matrix3x3 normal_matrix = glm::transpose(glm::inverse(Matrix3x3(model)));

        const size_t tri_count = mesh.indices.size() / 3;
        for (size_t t = 0; t < tri_count; ++t) {
            uint32_t i0 = mesh.indices[t * 3 + 0];
            uint32_t i1 = mesh.indices[t * 3 + 1];
            uint32_t i2 = mesh.indices[t * 3 + 2];

            if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() || i2 >= mesh.positions.size()) {
                continue;
            }

            Vector4 p0 = mvp * Vector4(mesh.positions[i0], 1.0f);
            Vector4 p1 = mvp * Vector4(mesh.positions[i1], 1.0f);
            Vector4 p2 = mvp * Vector4(mesh.positions[i2], 1.0f);

            // Near plane clip
            if (p0.w <= 0.001f || p1.w <= 0.001f || p2.w <= 0.001f) {
                continue;
            }

            // Perspective divide to NDC
            Vector3 ndc0 = Vector3(p0) / p0.w;
            Vector3 ndc1 = Vector3(p1) / p1.w;
            Vector3 ndc2 = Vector3(p2) / p2.w;

            // Frustum culling
            if ((ndc0.x < -1.0f && ndc1.x < -1.0f && ndc2.x < -1.0f) ||
                (ndc0.x > 1.0f && ndc1.x > 1.0f && ndc2.x > 1.0f) ||
                (ndc0.y < -1.0f && ndc1.y < -1.0f && ndc2.y < -1.0f) ||
                (ndc0.y > 1.0f && ndc1.y > 1.0f && ndc2.y > 1.0f) ||
                (ndc0.z < -1.0f && ndc1.z < -1.0f && ndc2.z < -1.0f) ||
                (ndc0.z > 1.0f && ndc1.z > 1.0f && ndc2.z > 1.0f)) {
                continue;
            }

            // Screen coordinates
            float sx0 = (ndc0.x + 1.0f) * 0.5f * (RENDER_W - 1);
            float sy0 = (1.0f - ndc0.y) * 0.5f * (RENDER_H - 1);
            float sx1 = (ndc1.x + 1.0f) * 0.5f * (RENDER_W - 1);
            float sy1 = (1.0f - ndc1.y) * 0.5f * (RENDER_H - 1);
            float sx2 = (ndc2.x + 1.0f) * 0.5f * (RENDER_W - 1);
            float sy2 = (1.0f - ndc2.y) * 0.5f * (RENDER_H - 1);

            float area2 = (sx1 - sx0) * (sy2 - sy0) - (sy1 - sy0) * (sx2 - sx0);
            if (std::abs(area2) < 1e-5f) {
                continue;
            }

            // Normals
            Vector3 n0, n1, n2;
            if (!mesh.normals.empty() && i0 < mesh.normals.size() && i1 < mesh.normals.size() && i2 < mesh.normals.size()) {
                n0 = glm::normalize(normal_matrix * mesh.normals[i0]);
                n1 = glm::normalize(normal_matrix * mesh.normals[i1]);
                n2 = glm::normalize(normal_matrix * mesh.normals[i2]);
            } else {
                Vector3 world_p0 = Vector3(model * Vector4(mesh.positions[i0], 1.0f));
                Vector3 world_p1 = Vector3(model * Vector4(mesh.positions[i1], 1.0f));
                Vector3 world_p2 = Vector3(model * Vector4(mesh.positions[i2], 1.0f));
                Vector3 fn       = glm::normalize(glm::cross(world_p1 - world_p0, world_p2 - world_p0));
                n0 = n1 = n2 = fn;
            }

            int min_x = std::max(0, static_cast<int>(std::floor(std::min({sx0, sx1, sx2}))));
            int max_x = std::min(RENDER_W - 1, static_cast<int>(std::ceil(std::max({sx0, sx1, sx2}))));
            int min_y = std::max(0, static_cast<int>(std::floor(std::min({sy0, sy1, sy2}))));
            int max_y = std::min(RENDER_H - 1, static_cast<int>(std::ceil(std::max({sy0, sy1, sy2}))));

            float inv_area = 1.0f / area2;

            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    float px = static_cast<float>(x);
                    float py = static_cast<float>(y);

                    float w0 = ((sx1 - px) * (sy2 - py) - (sy1 - py) * (sx2 - px)) * inv_area;
                    float w1 = ((sx2 - px) * (sy0 - py) - (sy2 - py) * (sx0 - px)) * inv_area;
                    float w2 = 1.0f - w0 - w1;

                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                        float z         = w0 * ndc0.z + w1 * ndc1.z + w2 * ndc2.z;
                        int   pixel_idx = y * RENDER_W + x;

                        if (z < depth_buffer[pixel_idx]) {
                            depth_buffer[pixel_idx] = z;

                            Vector3 norm = glm::normalize(w0 * n0 + w1 * n1 + w2 * n2);
                            // Two-sided shading: flip normal if pointing away from camera
                            Vector3 view_dir = glm::normalize(eye);
                            if (glm::dot(norm, view_dir) < 0.0f) {
                                norm = -norm;
                            }

                            float diff_key  = std::max(0.0f, glm::dot(norm, light_key)) * 0.70f;
                            float diff_fill = std::max(0.0f, glm::dot(norm, light_fill)) * 0.25f;
                            float ambient   = 0.25f;
                            float light     = ambient + diff_key + diff_fill;

                            float rim = std::pow(1.0f - std::max(0.0f, glm::dot(norm, view_dir)), 3.0f) * 0.15f;

                            Vector3 final_color = glm::clamp(clay_color * light + Vector3(rim), 0.0f, 1.0f);

                            size_t c_idx            = static_cast<size_t>(pixel_idx) * 4;
                            color_buffer[c_idx + 0] = static_cast<uint8_t>(final_color.r * 255.0f);
                            color_buffer[c_idx + 1] = static_cast<uint8_t>(final_color.g * 255.0f);
                            color_buffer[c_idx + 2] = static_cast<uint8_t>(final_color.b * 255.0f);
                            color_buffer[c_idx + 3] = 255;
                        }
                    }
                }
            }
        }
    }

    // 5. Downsample 256x256 -> 128x128 with 2x2 box filter (SSAA)
    Vector<uint8_t> thumb_pixels(THUMB_W * THUMB_H * 4, 0);
    for (int y = 0; y < THUMB_H; ++y) {
        for (int x = 0; x < THUMB_W; ++x) {
            int src_x = x * 2;
            int src_y = y * 2;

            uint32_t r = 0, g = 0, b = 0, a = 0;
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    size_t idx = static_cast<size_t>((src_y + dy) * RENDER_W + (src_x + dx)) * 4;
                    r += color_buffer[idx + 0];
                    g += color_buffer[idx + 1];
                    b += color_buffer[idx + 2];
                    a += color_buffer[idx + 3];
                }
            }

            size_t dst_idx            = static_cast<size_t>(y * THUMB_W + x) * 4;
            thumb_pixels[dst_idx + 0] = static_cast<uint8_t>(r / 4);
            thumb_pixels[dst_idx + 1] = static_cast<uint8_t>(g / 4);
            thumb_pixels[dst_idx + 2] = static_cast<uint8_t>(b / 4);
            thumb_pixels[dst_idx + 3] = static_cast<uint8_t>(a / 4);
        }
    }

    // 6. Save PNG to caches/thumbnails/<guid>.thumb.png
    if (!metadata.contains("guid") || !metadata["guid"].is_number()) {
        return false;
    }

    auto guid      = metadata["guid"].get<AssetID>();
    Path thumb_dir = Path(caches_root) / "thumbnails";
    fs::create_directories(thumb_dir);

    Path thumb_path = thumb_dir / (std::to_string(guid) + ".thumb.png");
    if (!stbi_write_png(thumb_path.string().c_str(), THUMB_W, THUMB_H, 4, thumb_pixels.data(), THUMB_W * 4)) {
        get_logger()->warn("Failed to write model thumbnail: {}", thumb_path.string());
        return false;
    }

    metadata["thumbnail"] = fs::relative(thumb_path, caches_root).string();
    return true;
}
