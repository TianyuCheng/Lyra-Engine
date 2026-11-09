module SDF;

// references: Inigo Quilez (iquilezles.org/articles/distfunctions & distfunctions2d)
static const float SDF_EPSILON = 1e-4;

// -----------------------------------------------------------------------------
// 1. 3D Primitive Distance Functions
// -----------------------------------------------------------------------------

/**
 * @brief Exact Euclidean distance to a sphere.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_sphere(float3 p, float radius)
{
    return length(p) - radius;
}

/**
 * @brief Exact Euclidean distance to an axis-aligned box with half-extents b.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_box(float3 p, float3 half_extents)
{
    float3 q = abs(p) - half_extents;
    return length(max(q, float3(0.0))) + min(max(q.x, max(q.y, q.z)), 0.0);
}

/**
 * @brief Exact Euclidean distance to a rounded box with half-extents b and corner radius r.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_round_box(float3 p, float3 half_extents, float corner_radius)
{
    float3 q = abs(p) - half_extents + corner_radius;
    return length(max(q, float3(0.0))) + min(max(q.x, max(q.y, q.z)), 0.0) - corner_radius;
}

/**
 * @brief Exact Euclidean distance to a hollow wireframe box frame.
 * @param half_extents Box half-extents
 * @param edge_thickness Frame thickness e
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_box_frame(float3 p, float3 half_extents, float edge_thickness)
{
    p = abs(p) - half_extents;
    float3 q = abs(p + edge_thickness) - edge_thickness;
    return min(min(
        length(max(float3(p.x, q.y, q.z), float3(0.0))) + min(max(p.x, max(q.y, q.z)), 0.0),
        length(max(float3(q.x, p.y, q.z), float3(0.0))) + min(max(q.x, max(p.y, q.z)), 0.0)),
        length(max(float3(q.x, q.y, p.z), float3(0.0))) + min(max(q.x, max(q.y, p.z)), 0.0));
}

/**
 * @brief Exact Euclidean distance to a torus centered at origin in the XZ plane.
 * @param t float2(major_radius, tube_radius)
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_torus(float3 p, float2 t)
{
    float2 q = float2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

/**
 * @brief Exact Euclidean distance to a capped torus segment.
 * @param sc sin and cos of opening angle
 * @param ra Major radius
 * @param rb Tube radius
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_capped_torus(float3 p, float2 sc, float ra, float rb)
{
    p.x = abs(p.x);
    float k = (sc.y * p.x > sc.x * p.y) ? dot(p.xy, sc) : length(p.xy);
    return sqrt(dot(p, p) + ra * ra - 2.0 * ra * k) - rb;
}

/**
 * @brief Exact Euclidean distance to a vertical finite capped cylinder.
 * @param h Half height
 * @param r Radius
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_cylinder(float3 p, float h, float r)
{
    float2 d = abs(float2(length(p.xz), p.y)) - float2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, float2(0.0)));
}

/**
 * @brief Exact Euclidean distance to an infinite cylinder aligned with Y axis.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_cylinder_infinite(float3 p, float radius)
{
    return length(p.xz) - radius;
}

/**
 * @brief Exact Euclidean distance to an arbitrarily oriented capsule between points a and b.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_capsule(float3 p, float3 a, float3 b, float radius)
{
    float3 pa = p - a;
    float3 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - radius;
}

/**
 * @brief Exact Euclidean distance to a vertical capsule centered along the Y axis.
 * @param h Cylinder half height
 * @param r Capsule cap radius
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_vertical_capsule(float3 p, float h, float r)
{
    p.y -= clamp(p.y, 0.0, h);
    return length(p) - r;
}

/**
 * @brief Exact Euclidean distance to an infinite cone.
 * @param c sin/cos of angle
 * @param h Height
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_cone(float3 p, float2 c, float h)
{
    float2 q = h * float2(c.x / c.y, -1.0);
    float2 w = float2(length(p.xz), p.y);
    float2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
    float2 b = w - q * float2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
    float k = sign(q.y);
    float d = min(dot(a, a), dot(b, b));
    float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
    return sqrt(d) * sign(s);
}

/**
 * @brief Exact Euclidean distance to a capped cone / truncated frustum.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_capped_cone(float3 p, float h, float r1, float r2)
{
    float2 q = float2(length(p.xz), p.y);
    float2 k1 = float2(r2, h);
    float2 k2 = float2(r2 - r1, 2.0 * h);
    float2 ca = float2(q.x - min(q.x, (q.y < 0.0) ? r1 : r2), abs(q.y) - h);
    float2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot(k2, k2), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));
}

/**
 * @brief Exact Euclidean distance to an infinite plane defined by normal and offset.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_plane(float3 p, float3 normal, float offset)
{
    return dot(p, normal) + offset;
}

/**
 * @brief Bounded distance approximation to an ellipsoid.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_ellipsoid(float3 p, float3 radii)
{
    float k0 = length(p / radii);
    float k1 = length(p / (radii * radii));
    return (k1 > 0.0) ? (k0 * (k0 - 1.0) / k1) : 0.0;
}

/**
 * @brief Exact Euclidean distance to an octahedron.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_octahedron(float3 p, float s)
{
    p = abs(p);
    float m = p.x + p.y + p.z - s;
    float3 q;
    if (3.0 * p.x < m)      q = p.xyz;
    else if (3.0 * p.y < m) q = p.yzx;
    else if (3.0 * p.z < m) q = p.zxy;
    else return m * 0.57735027; // 1.0 / sqrt(3.0)

    float k = clamp(0.5 * (q.z - q.y + s), 0.0, s);
    return length(float3(q.x, q.y - s + k, q.z - k));
}

/**
 * @brief Exact Euclidean distance to a 4-sided pyramid with height h.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_pyramid(float3 p, float h)
{
    float m2 = h * h + 0.25;
    p.xz = abs(p.xz);
    p.xz = (p.z > p.x) ? p.zx : p.xz;
    p.xz -= 0.5;

    float3 q = float3(p.z, h * p.y - 0.5 * p.x, h * p.x + 0.5 * p.y);
    float s = max(-q.x, 0.0);
    float t = clamp((q.y - 0.5 * p.z) / (m2 + 0.25), 0.0, 1.0);

    float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
    float b = m2 * (q.x + 0.5 * t) * (q.x + 0.5 * t) + (q.y - m2 * t) * (q.y - m2 * t);

    float d2 = min(q.y, -q.x * m2 - q.y * 0.5) > 0.0 ? 0.0 : min(a, b);
    return sqrt((d2 + q.z * q.z) / m2) * sign(max(q.z, -p.y));
}

/**
 * @brief Exact Euclidean distance to a triangular prism.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_triangular_prism(float3 p, float2 h)
{
    float3 q = abs(p);
    return max(q.z - h.y, max(q.x * 0.866025 + p.y * 0.5, -p.y) - h.x * 0.5);
}

/**
 * @brief Exact Euclidean distance to a hexagonal prism.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float sdf_hexagonal_prism(float3 p, float2 h)
{
    const float3 k = float3(-0.8660254, 0.5, 0.57735026);
    p = abs(p);
    p.xy -= 2.0 * min(dot(k.xy, p.xy), 0.0) * k.xy;
    float2 d = float2(
        length(p.xy - float2(clamp(p.x, -k.z * h.x, k.z * h.x), h.x)) * sign(p.y - h.x),
        p.z - h.y);
    return min(max(d.x, d.y), 0.0) + length(max(d, float2(0.0)));
}

// =============================================================================
// 2. Boolean CSG Operations & Blends
// =============================================================================

/**
 * @brief Standard Constructive Solid Geometry (CSG) Union: A u B.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float op_union(float d1, float d2)
{
    return min(d1, d2);
}

/**
 * @brief Standard CSG Subtraction: B \ A (carves A out of B).
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float op_subtraction(float d1, float d2)
{
    return max(-d1, d2);
}

/**
 * @brief Standard CSG Intersection: A n B.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float op_intersection(float d1, float d2)
{
    return max(d1, d2);
}

/**
 * @brief Inigo Quilez polynomial smooth minimum (C1 continuous blend).
 * Reference: https://iquilezles.org/articles/smin/
 * @param k Smoothing radius / blending factor
 */
public float op_smooth_union(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / max(k, SDF_EPSILON), 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

/**
 * @brief Polynomial smooth subtraction.
 * Reference: https://iquilezles.org/articles/smin/
 */
public float op_smooth_subtraction(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 + d1) / max(k, SDF_EPSILON), 0.0, 1.0);
    return lerp(d2, -d1, h) + k * h * (1.0 - h);
}

/**
 * @brief Polynomial smooth intersection.
 * Reference: https://iquilezles.org/articles/smin/
 */
public float op_smooth_intersection(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 - d1) / max(k, SDF_EPSILON), 0.0, 1.0);
    return lerp(d2, d1, h) + k * h * (1.0 - h);
}

/**
 * @brief Chamfer union: creates a 45-degree flat bevel between two surfaces.
 */
public float op_chamfer_union(float d1, float d2, float r)
{
    return min(min(d1, d2), (d1 - r + d2) * 0.70710678); // 1.0 / sqrt(2.0)
}

/**
 * @brief Round union: creates a filleted circular bevel between two surfaces.
 */
public float op_round_union(float d1, float d2, float r)
{
    float2 u = max(float2(r - d1, r - d2), float2(0.0));
    return max(r, min(d1, d2)) - length(u);
}

// =============================================================================
// 3. Domain Operations & Deformations
// =============================================================================

/**
 * @brief Elongates an SDF domain along axis-aligned dimensions by half-extents h.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float3 op_elongate(float3 p, float3 h)
{
    return p - clamp(p, -h, h);
}

/**
 * @brief Rounds any sharp SDF primitive by expanding its boundary by radius r.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float op_round(float d, float radius)
{
    return d - radius;
}

/**
 * @brief Converts any solid SDF into a hollow shell of specified wall thickness (onion skin).
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float op_onion(float d, float thickness)
{
    return abs(d) - thickness;
}

/**
 * @brief Infinite lattice repetition: repeats domain periodically with spacing period.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float3 op_repeat(float3 p, float3 period)
{
    return (p + 0.5 * period) % period - 0.5 * period;
}

/**
 * @brief Limited lattice repetition: repeats domain up to integer count limits.
 * Reference: https://iquilezles.org/articles/distfunctions/
 */
public float3 op_repeat_limited(float3 p, float3 period, float3 limit)
{
    return p - period * clamp(round(p / period), -limit, limit);
}

/**
 * @brief Spatial twist deformation around the Y axis.
 * Reference: https://iquilezles.org/articles/distfunctions/
 * @param k Angular rate of twist per meter (radians/m)
 */
public float3 op_twist(float3 p, float k)
{
    float c = cos(k * p.y);
    float s = sin(k * p.y);
    float2x2 m = float2x2(c, -s, s, c);
    return float3(mul(m, p.xz), p.y).xzy;
}

/**
 * @brief Spatial bend deformation along the X axis.
 * Reference: https://iquilezles.org/articles/distfunctions/
 * @param k Curvature rate of bend
 */
public float3 op_bend(float3 p, float k)
{
    float c = cos(k * p.x);
    float s = sin(k * p.x);
    float2x2 m = float2x2(c, -s, s, c);
    return float3(mul(m, p.xy), p.z);
}

/**
 * @brief Domain symmetry reflection along the X axis.
 */
public float3 op_symmetry_x(float3 p)
{
    return float3(abs(p.x), p.yz);
}

/**
 * @brief Domain symmetry reflection along both X and Z axes.
 */
public float3 op_symmetry_xz(float3 p)
{
    return float3(abs(p.x), p.y, abs(p.z));
}

// =============================================================================
// 4. Raymarching, Normal Gradient, and Lighting Evaluators
// =============================================================================

/**
 * @brief Slang interface for evaluating composite signed distance field scenes.
 */
public interface ISDF
{
    float evaluate(float3 p);
};

/**
 * @brief Standard sphere SDF implementing ISDF.
 */
public struct SphereSDF : ISDF
{
    public float radius;
    public float evaluate(float3 p)
    {
        return sdf_sphere(p, radius);
    }
};

/**
 * @brief Inigo Quilez tetrahedral surface normal gradient estimator.
 * Evaluates the gradient using only 4 field evaluations (instead of standard 6-tap central differences),
 * maximizing GPU SIMD execution efficiency and texture cache locality.
 * Reference: https://iquilezles.org/articles/normalsSDF/
 *
 * @param p Surface contact point
 * @param eps Finite difference step size (typically 1e-4 .. 1e-3)
 * @param scene Scene struct conforming to ISDF interface
 */
public float3 sdf_calc_normal<TScene : ISDF>(float3 p, float eps, TScene scene)
{
    const float2 k = float2(1.0, -1.0);
    return normalize(
        k.xyy * scene.evaluate(p + k.xyy * eps) +
        k.yyx * scene.evaluate(p + k.yyx * eps) +
        k.yxy * scene.evaluate(p + k.yxy * eps) +
        k.xxx * scene.evaluate(p + k.xxx * eps)
    );
}

/**
 * @brief Inigo Quilez 2017 improved soft shadow evaluator with penumbra correction.
 * Eliminates artificial dark band contact artifacts caused by standard linear ray step approximations.
 * Reference: https://iquilezles.org/articles/rmshadows/
 *
 * @param ro Ray origin (elevated along surface normal by small bias)
 * @param rd Ray direction towards light source
 * @param mint Minimum march distance
 * @param maxt Maximum march distance
 * @param k Penumbra softness sharpness factor (larger = sharper, e.g. 16.0..32.0)
 * @param scene Scene struct conforming to ISDF interface
 * @return Shadow visibility scalar in range [0.0, 1.0] (0 = fully occluded, 1 = unshadowed)
 */
public float sdf_calc_softshadow<TScene : ISDF>(
    float3 ro,
    float3 rd,
    float  mint,
    float  maxt,
    float  k,
    TScene scene)
{
    float res = 1.0;
    float ph = 1e20; // previous step distance
    float t = mint;

    for (int i = 0; i < 64 && t < maxt; ++i) {
        float h = scene.evaluate(ro + rd * t);
        if (h < 0.001) {
            return 0.0;
        }
        // Inigo Quilez 2017 correction: uses pythagorean triangle step correction
        float y = h * h / (2.0 * ph);
        float d = sqrt(max(0.0, h * h - y * y));
        res = min(res, k * d / max(0.0, t - y));
        ph = h;
        t += h;
    }
    return clamp(res, 0.0, 1.0);
}

/**
 * @brief Multi-step sphere-sampled ambient occlusion for signed distance fields.
 * Samples the field along the surface normal at increasing distance radii.
 * Reference: https://iquilezles.org/articles/distfunctions/
 *
 * @param p Surface position
 * @param n Surface unit normal
 * @param scene Scene struct conforming to ISDF interface
 * @param max_dist Maximum AO probe radius (typically 0.2..1.0m)
 * @param num_samples Number of sampling iterations (typically 5)
 * @return Ambient occlusion factor [0.0 = occluded, 1.0 = fully exposed]
 */
public float sdf_calc_ao<TScene : ISDF>(
    float3 p,
    float3 n,
    TScene scene,
    float  max_dist = 0.5,
    int    num_samples = 5)
{
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 1; i <= num_samples; ++i) {
        float h = (float(i) / float(num_samples)) * max_dist;
        float d = scene.evaluate(p + n * h);
        occ += (h - d) * sca;
        sca *= 0.75;
    }

    return clamp(1.0 - 1.5 * occ, 0.0, 1.0);
}

// =============================================================================
// 5. 2D Distance Functions & 3D Extrusion/Revolution
// Reference: Inigo Quilez, https://iquilezles.org/articles/distfunctions2d/
// =============================================================================

public float sdf_circle_2d(float2 p, float r)
{
    return length(p) - r;
}

public float sdf_box_2d(float2 p, float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, float2(0.0))) + min(max(d.x, d.y), 0.0);
}

public float sdf_round_box_2d(float2 p, float2 b, float r)
{
    float2 d = abs(p) - b + r;
    return length(max(d, float2(0.0))) + min(max(d.x, d.y), 0.0) - r;
}

public float sdf_oriented_box_2d(float2 p, float2 a, float2 b, float th)
{
    float l = length(b - a);
    float2 d = (b - a) / max(l, 1e-5);
    float2 q = p - (a + b) * 0.5;
    q = float2(d.x * q.x + d.y * q.y, -d.y * q.x + d.x * q.y);
    q = abs(q) - float2(l * 0.5, th);
    return length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0);
}

public float sdf_segment_2d(float2 p, float2 a, float2 b)
{
    float2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-5), 0.0, 1.0);
    return length(pa - ba * h);
}

public float sdf_rhombus_2d(float2 p, float2 b)
{
    p = abs(p);
    float h = clamp((dot(b - 2.0 * p, b)) / max(dot(b, b), 1e-5), -1.0, 1.0);
    float d = length(p - 0.5 * b * float2(1.0 - h, 1.0 + h));
    return d * sign(p.x * b.y + p.y * b.x - b.x * b.y);
}

public float sdf_trapezoid_2d(float2 p, float r1, float r2, float he)
{
    float2 k1 = float2(r2, he);
    float2 k2 = float2(r2 - r1, 2.0 * he);
    p.x = abs(p.x);
    float2 ca = float2(p.x - min(p.x, (p.y < 0.0) ? r1 : r2), abs(p.y) - he);
    float2 cb = p - k1 + k2 * clamp(dot(k1 - p, k2) / max(dot(k2, k2), 1e-5), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));
}

public float sdf_equilateral_triangle_2d(float2 p, float r)
{
    const float k = 1.7320508;
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0) p = float2(p.x - k * p.y, -k * p.x - p.y) * 0.5;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

public float sdf_hexagon_2d(float2 p, float r)
{
    const float3 k = float3(-0.866025404, 0.5, 0.577350269);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= float2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}

public float sdf_octagon_2d(float2 p, float r)
{
    const float3 k = float3(-0.9238795325, 0.3826834323, 0.4142135623);
    p = abs(p);
    p -= 2.0 * min(dot(float2(k.x, k.y), p), 0.0) * float2(k.x, k.y);
    p -= 2.0 * min(dot(float2(-k.x, k.y), p), 0.0) * float2(-k.x, k.y);
    p -= float2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}

public float sdf_heart_2d(float2 p)
{
    p.x = abs(p.x);
    if (p.y + p.x > 1.0)
        return sqrt(dot(p - float2(0.25, 0.75), p - float2(0.25, 0.75))) - 0.35355339; // sqrt(2)/4
    return sqrt(min(dot(p - float2(0.00, 1.00), p - float2(0.00, 1.00)),
                    dot(p - 0.5 * max(p.x + p.y, 0.0), p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
}

public float op_annular_2d(float d, float r)
{
    return abs(d) - r;
}

public float op_extrude(float3 p, float d_2d, float h)
{
    float2 w = float2(d_2d, abs(p.z) - h);
    return min(max(w.x, w.y), 0.0) + length(max(w, float2(0.0)));
}

public float2 op_revolve_coords(float3 p, float offset)
{
    return float2(length(p.xz) - offset, p.y);
}
