import lyra;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct FragmentOutput
{
    float4 color : SV_Target;
    float  depth : SV_Depth;
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
    // fullscreen quad in NDC [-1, 1] using 6 vertices (2 triangles)
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
    output.uv       = p;
    return output;
}

float compute_grid(float2 coord, float scale, float line_width)
{
    float2 uv = coord / scale;
    float2 d = max(fwidth(uv), float2(1e-6, 1e-6));
    float2 g = abs(frac(uv - 0.5) - 0.5) / d;
    float line_dist = min(g.x, g.y);
    return 1.0 - min(line_dist / line_width, 1.0);
}

[shader("fragment")]
FragmentOutput fsmain(VertexOutput input)
{
    // unproject near and far points from NDC (-1 to 1) to view then world space
    float4 p_near_clip = float4(input.uv, 0.0, 1.0);
    float4 p_far_clip  = float4(input.uv, 1.0, 1.0);

    float4 p_near_view = mul(p_near_clip, camera.inv_proj);
    p_near_view /= p_near_view.w;
    float3 p_near_world = mul(float4(p_near_view.xyz, 1.0), camera.inv_view).xyz;

    float4 p_far_view = mul(p_far_clip, camera.inv_proj);
    p_far_view /= p_far_view.w;
    float3 p_far_world = mul(float4(p_far_view.xyz, 1.0), camera.inv_view).xyz;

    float3 ray_dir = p_far_world - p_near_world;
    if (abs(ray_dir.y) < 1e-6)
        discard;

    // intersection with ground plane Y = 0
    float t = -p_near_world.y / ray_dir.y;
    if (t <= 0.0)
        discard;

    float3 world_pos = p_near_world + t * ray_dir;

    // compute clip-space depth for depth buffer
    float4 clip_pos = mul(mul(float4(world_pos, 1.0), camera.view), camera.proj);
    float depth = clip_pos.z / clip_pos.w;

    if (depth < 0.0 || depth > 1.0)
        discard;

    // screen-space derivatives for anti-aliasing
    float2 d_coord = max(fwidth(world_pos.xz), float2(1e-6, 1e-6));

    // distance-based radial LOD calculation (uniform around the camera, avoiding elliptical artifacts)
    float dist = length(world_pos - camera.pos);
    float cam_alt = max(abs(camera.pos.y), 1.5);

    // 1-meter fine grid: fully visible nearby, smoothly dissolves over a broad distance range
    float fine_start = cam_alt * 6.0;
    float fine_end   = cam_alt * 18.0;
    float fine_lod   = 1.0 - smoothstep(fine_start, fine_end, dist);

    // 10-meter coarse grid: remains visible far out into the distance
    float coarse_start = cam_alt * 25.0;
    float coarse_end   = cam_alt * 65.0;
    float coarse_lod   = 1.0 - smoothstep(coarse_start, coarse_end, dist);

    // 0.1-meter sub-grid: softly appears only when zoomed in close to the ground (camera Y < 2.0)
    float sub_weight = smoothstep(2.0, 0.6, abs(camera.pos.y));
    float sub_lod    = (1.0 - smoothstep(2.0, 8.0, dist)) * sub_weight;

    // compute grid lines for active levels
    float line_01m = (sub_lod > 0.001)    ? compute_grid(world_pos.xz, 0.1,  0.8)  : 0.0;
    float line_1m  = (fine_lod > 0.001)   ? compute_grid(world_pos.xz, 1.0,  1.0)  : 0.0;
    float line_10m = (coarse_lod > 0.001) ? compute_grid(world_pos.xz, 10.0, 1.25) : 0.0;

    // opacities for each level
    float alpha_01m = line_01m * 0.15 * sub_lod;
    float alpha_1m  = line_1m  * 0.30 * fine_lod;
    float alpha_10m = line_10m * 0.60 * coarse_lod;

    // maximum blending: major 10m lines stay solid while intermediate 1m lines dissolve seamlessly
    float alpha = max(alpha_01m, max(alpha_1m, alpha_10m));

    // color grading from fine lines to major subdivision lines
    float3 col = float3(0.48, 0.48, 0.52);
    if (line_10m > 0.01 && coarse_lod > 0.01) {
        col = lerp(col, float3(0.72, 0.72, 0.76), line_10m * coarse_lod);
    }

    // coordinate axes: X axis (Z == 0) in Red, Z axis (X == 0) in Blue
    float x_axis_dist = abs(world_pos.z) / d_coord.y;
    float is_x_axis = 1.0 - min(x_axis_dist / 1.5, 1.0);

    float z_axis_dist = abs(world_pos.x) / d_coord.x;
    float is_z_axis = 1.0 - min(z_axis_dist / 1.5, 1.0);

    if (is_x_axis > 0.001) {
        col = lerp(col, float3(0.88, 0.25, 0.25), is_x_axis);
        alpha = max(alpha, is_x_axis * 0.90 * coarse_lod);
    }
    if (is_z_axis > 0.001) {
        col = lerp(col, float3(0.25, 0.50, 0.90), is_z_axis);
        alpha = max(alpha, is_z_axis * 0.90 * coarse_lod);
    }

    // soft grazing angle fade near the horizon to avoid edge shimmer
    float3 view_dir = normalize(world_pos - camera.pos);
    float angle_fade = smoothstep(0.005, 0.05, abs(view_dir.y));
    alpha *= angle_fade;

    if (alpha <= 0.002)
        discard;

    FragmentOutput output;
    output.color = float4(col, alpha);
    output.depth = depth;
    return output;
}
