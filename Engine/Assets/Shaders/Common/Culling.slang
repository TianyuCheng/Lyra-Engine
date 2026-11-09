module Culling;

static const float EPSILON = 1e-6;

// =============================================================================
// 1. Geometric Primitive Bounds
// =============================================================================

public struct BoundingSphere
{
    public float3 center;
    public float  radius;

    public bool is_valid()
    {
        return radius >= 0.0;
    }
};

public struct BoundingBox
{
    public float3 min;
    public float3 max;

    public float3 center()
    {
        return (min + max) * 0.5;
    }

    public float3 extents()
    {
        return (max - min) * 0.5;
    }

    public BoundingSphere to_sphere()
    {
        BoundingSphere s;
        s.center = center();
        s.radius = length(extents());
        return s;
    }
};

public struct NormalCone
{
    public float3 apex;       // cone origin / apex in world space
    public float3 axis;       // average cluster normal direction (normalized)
    public float  cutoff_sin; // sine of half-angle subtended by cluster normals + self-shadow angle
};

// =============================================================================
// 2. Frustum Plane Extraction (Gribb & Hartmann)
// =============================================================================

/**
 * @brief Extracts 6 normalized inward-pointing frustum clipping planes from a View-Projection matrix.
 * Format of planes[i]: xyz = normal pointing inward, w = plane distance offset (dot(p, xyz) + w >= 0 inside).
 * Order: Left (0), Right (1), Bottom (2), Top (3), Near (4), Far (5).
 */
public void extract_frustum_planes(float4x4 m, out float4 planes[6])
{
    // Left: row3 + row0
    planes[0] = float4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]);
    // Right: row3 - row0
    planes[1] = float4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]);
    // Bottom: row3 + row1
    planes[2] = float4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]);
    // Top: row3 - row1
    planes[3] = float4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]);
    // Near: row2 (assuming standard Vulkan/D3D depth [0, 1])
    planes[4] = float4(m[0][2], m[1][2], m[2][2], m[3][2]);
    // Far: row3 - row2
    planes[5] = float4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);

    // Normalize plane equations
    [unroll]
    for (int i = 0; i < 6; ++i) {
        float len = length(planes[i].xyz);
        planes[i] *= (1.0 / max(len, EPSILON));
    }
}

// =============================================================================
// 3. Frustum Culling
// =============================================================================

/**
 * @brief Tests whether a bounding sphere is culled by the 6 frustum planes.
 * @return True if culled (outside frustum), false if visible/intersecting.
 */
public bool cull_sphere_frustum(float4 planes[6], float3 center, float radius)
{
    [unroll]
    for (int i = 0; i < 6; ++i) {
        if (dot(center, planes[i].xyz) + planes[i].w < -radius) {
            return true; // outside this plane
        }
    }
    return false;
}

/**
 * @brief Tests whether an Axis-Aligned Bounding Box (AABB) is culled by the 6 frustum planes.
 * Uses center-extents projection for optimal branchless evaluation.
 * @return True if culled (outside frustum), false if visible/intersecting.
 */
public bool cull_aabb_frustum(float4 planes[6], float3 aabb_min, float3 aabb_max)
{
    float3 center  = (aabb_min + aabb_max) * 0.5;
    float3 extents = (aabb_max - aabb_min) * 0.5;

    [unroll]
    for (int i = 0; i < 6; ++i) {
        float3 n = planes[i].xyz;
        float r = extents.x * abs(n.x) + extents.y * abs(n.y) + extents.z * abs(n.z);
        float d = dot(center, n) + planes[i].w;

        if (d < -r) {
            return true; // entirely outside this plane
        }
    }
    return false;
}

public bool cull_aabb_frustum(float4x4 view_proj, float3 aabb_min, float3 aabb_max)
{
    float4 planes[6];
    extract_frustum_planes(view_proj, planes);
    return cull_aabb_frustum(planes, aabb_min, aabb_max);
}

// =============================================================================
// 4. Normal / Cone Culling (Meshlet & Cluster Backface Culling)
// =============================================================================

/**
 * @brief Backface culls an entire meshlet/cluster of triangles using its bounding normal cone.
 * @param camera_pos World space camera position
 * @param cone_apex Cone apex in world space
 * @param cone_axis Dominant normal axis of the cluster (normalized)
 * @param cone_cutoff_sin Sine of the cone cutoff angle
 * @return True if the entire cluster is facing away from the camera and can be culled.
 */
public bool cull_cluster_cone(
    float3 camera_pos,
    float3 cone_apex,
    float3 cone_axis,
    float  cone_cutoff_sin)
{
    float3 to_camera = camera_pos - cone_apex;
    float dist_sq = dot(to_camera, to_camera);

    // If camera is very close or inside the cluster apex, don't cull
    if (dist_sq < 1e-4) {
        return false;
    }

    float3 view_dir = to_camera * rsqrt(dist_sq);
    return dot(view_dir, cone_axis) <= cone_cutoff_sin;
}

// =============================================================================
// 5. Light & Volume Culling (Clustered / Tiled Shading)
// =============================================================================

/**
 * @brief Tests whether a bounding sphere overlaps an AABB volume (e.g. point light vs cluster grid tile).
 */
public bool cull_sphere_aabb(float3 sphere_center, float sphere_radius, float3 aabb_min, float3 aabb_max)
{
    float3 closest = clamp(sphere_center, aabb_min, aabb_max);
    float3 diff    = closest - sphere_center;
    return dot(diff, diff) > (sphere_radius * sphere_radius); // returns true if culled (no overlap)
}

/**
 * @brief Tests whether a spotlight cone is culled from a bounding sphere (tile cluster).
 */
public bool cull_cone_sphere(
    float3 cone_apex,
    float3 cone_axis,
    float  cone_range,
    float  cone_angle_cos,
    float3 sphere_center,
    float  sphere_radius)
{
    float3 v = sphere_center - cone_apex;
    float len_sq = dot(v, v);
    float v_proj = dot(v, cone_axis);

    // 1. Sphere behind the cone apex
    if (v_proj + sphere_radius < 0.0) {
        return true;
    }

    // 2. Sphere beyond the cone range
    if (v_proj - sphere_radius > cone_range) {
        return true;
    }

    // 3. Sphere outside the cone lateral slant
    float cone_angle_sin = sqrt(max(0.0, 1.0 - cone_angle_cos * cone_angle_cos));
    float dist_to_axis = sqrt(max(0.0, len_sq - v_proj * v_proj));

    // Distance from sphere center to closest surface point on infinite cone
    float dist_to_cone_surface = dist_to_axis * cone_angle_cos - v_proj * cone_angle_sin;
    if (dist_to_cone_surface > sphere_radius) {
        return true;
    }

    return false;
}

// =============================================================================
// 6. Screen-Space Projection & Hi-Z Occlusion Culling
// =============================================================================

/**
 * @brief Projects an AABB into normalized screen-space UV bounding rectangle [u_min, v_min, u_max, v_max]
 * and computes minimum and maximum clipping-space depth values.
 */
public bool project_aabb_to_screen_rect(
    float3   aabb_min,
    float3   aabb_max,
    float4x4 view_proj,
    out float4 screen_rect_uv,
    out float  min_depth,
    out float  max_depth)
{
    float3 corners[8] = {
        float3(aabb_min.x, aabb_min.y, aabb_min.z),
        float3(aabb_max.x, aabb_min.y, aabb_min.z),
        float3(aabb_min.x, aabb_max.y, aabb_min.z),
        float3(aabb_max.x, aabb_max.y, aabb_min.z),
        float3(aabb_min.x, aabb_min.y, aabb_max.z),
        float3(aabb_max.x, aabb_min.y, aabb_max.z),
        float3(aabb_min.x, aabb_max.y, aabb_max.z),
        float3(aabb_max.x, aabb_max.y, aabb_max.z),
    };

    float2 min_uv = float2(1e5, 1e5);
    float2 max_uv = float2(-1e5, -1e5);
    min_depth = 1e5;
    max_depth = -1e5;

    bool any_in_front = false;

    [unroll]
    for (int i = 0; i < 8; ++i) {
        float4 clip = mul(view_proj, float4(corners[i], 1.0));
        if (clip.w > 0.0) {
            any_in_front = true;
            float3 ndc = clip.xyz / clip.w;
            float2 uv  = ndc.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

            min_uv = min(min_uv, uv);
            max_uv = max(max_uv, uv);
            min_depth = min(min_depth, ndc.z);
            max_depth = max(max_depth, ndc.z);
        }
    }

    if (!any_in_front) {
        screen_rect_uv = float4(0.0, 0.0, 0.0, 0.0);
        return false;
    }

    screen_rect_uv = float4(clamp(min_uv, 0.0, 1.0), clamp(max_uv, 0.0, 1.0));
    return true;
}

/**
 * @brief Evaluates Hierarchical Z (Hi-Z) occlusion culling.
 * @param hiz_pyramid Texture2D mipmapped depth pyramid
 * @param smp Point-clamp sampler
 * @param screen_rect_uv Projected bounding box [min_u, min_v, max_u, max_v]
 * @param object_depth Closest depth of the object (min_depth for standard, max_depth for reverse-Z)
 * @param hiz_size Dimensions of the base (mip 0) Hi-Z texture
 * @param reverse_z Set to true if using reverse floating-point depth (1 near, 0 far), false for standard (0 near, 1 far)
 * @return True if occluded / culled, false if visible.
 */
public bool cull_hiz_occlusion(
    Texture2D<float> hiz_pyramid,
    SamplerState     smp,
    float4           screen_rect_uv,
    float            object_depth,
    float2           hiz_size,
    bool             reverse_z = true)
{
    float2 rect_size = (screen_rect_uv.zw - screen_rect_uv.xy) * hiz_size;
    float max_dim = max(rect_size.x, rect_size.y);
    float mip = max(0.0, ceil(log2(max(max_dim, 1.0))));

    // Fetch the 4 texels encompassing the screen footprint at selected mip
    float2 uv0 = screen_rect_uv.xy;
    float2 uv1 = screen_rect_uv.zw;
    float2 uv2 = float2(screen_rect_uv.x, screen_rect_uv.w);
    float2 uv3 = float2(screen_rect_uv.z, screen_rect_uv.y);

    float d0 = hiz_pyramid.SampleLevel(smp, uv0, mip);
    float d1 = hiz_pyramid.SampleLevel(smp, uv1, mip);
    float d2 = hiz_pyramid.SampleLevel(smp, uv2, mip);
    float d3 = hiz_pyramid.SampleLevel(smp, uv3, mip);

    if (reverse_z) {
        // In reverse-Z, larger depth is closer. The Hi-Z pyramid stores min depth (furthest occluder).
        float furthest_occluder = min(min(d0, d1), min(d2, d3));
        return object_depth <= furthest_occluder; // object is completely behind occluder
    } else {
        // In standard Z, smaller depth is closer. The Hi-Z pyramid stores max depth.
        float furthest_occluder = max(max(d0, d1), max(d2, d3));
        return object_depth >= furthest_occluder;
    }
}

// =============================================================================
// 7. Subpixel & Distance Culling
// =============================================================================

/**
 * @brief Culls small geometry whose projected screen-space diameter is below min_pixels.
 */
public bool cull_subpixel_size(
    float3 center,
    float  radius,
    float3 camera_pos,
    float  fov_y_radians,
    float  screen_height,
    float  min_pixels = 2.0)
{
    float dist = length(center - camera_pos);
    if (dist <= radius) {
        return false;
    }

    float projected_diameter = (2.0 * radius * screen_height) / (2.0 * dist * tan(0.5 * fov_y_radians));
    return projected_diameter < min_pixels;
}

/**
 * @brief Culls objects beyond maximum view distance.
 */
public bool cull_distance(float3 center, float radius, float3 camera_pos, float max_distance)
{
    return (length(center - camera_pos) - radius) > max_distance;
}
