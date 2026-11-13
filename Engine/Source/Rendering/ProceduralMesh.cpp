#include <Lyra/Rendering/ProceduralMesh.h>

using namespace lyra;

static const float PI = 3.14159265358979323846f;

using namespace lyra::detail;

MeshAsset lyra::detail::build_procedural_mesh_asset(
    const Vector<ProceduralVertex>& vertices,
    const Vector<uint32_t>&         indices,
    const Vector3&                  min_bounds,
    const Vector3&                  max_bounds)
{
    MeshAsset mesh;
    mesh.min_bounds = min_bounds;
    mesh.max_bounds = max_bounds;

    MeshLOD lod;
    lod.index_format = GPUIndexFormat::UINT32;
    lod.index_data.resize(indices.size() * sizeof(uint32_t));
    std::memcpy(lod.index_data.data(), indices.data(), lod.index_data.size());

    // 1. position attribute
    MeshAttribute pos_attr;
    pos_attr.semantics     = MeshSemantics::POSITION;
    pos_attr.format        = GPUVertexFormat::FLOAT32x3;
    pos_attr.element_count = static_cast<uint32_t>(vertices.size());
    pos_attr.data.resize(vertices.size() * sizeof(Vector3));
    auto* pos_dst = reinterpret_cast<Vector3*>(pos_attr.data.data());
    for (usize i = 0; i < vertices.size(); ++i) {
        pos_dst[i] = vertices[i].pos;
    }
    lod.attributes.push_back(std::move(pos_attr));

    // 2. normal attribute
    MeshAttribute nrm_attr;
    nrm_attr.semantics     = MeshSemantics::NORMAL;
    nrm_attr.format        = GPUVertexFormat::FLOAT32x3;
    nrm_attr.element_count = static_cast<uint32_t>(vertices.size());
    nrm_attr.data.resize(vertices.size() * sizeof(Vector3));
    auto* nrm_dst = reinterpret_cast<Vector3*>(nrm_attr.data.data());
    for (usize i = 0; i < vertices.size(); ++i) {
        nrm_dst[i] = vertices[i].normal;
    }
    lod.attributes.push_back(std::move(nrm_attr));

    // 3. texcoord attribute
    MeshAttribute uv_attr;
    uv_attr.semantics     = MeshSemantics::TEXCOORD0;
    uv_attr.format        = GPUVertexFormat::FLOAT32x2;
    uv_attr.element_count = static_cast<uint32_t>(vertices.size());
    uv_attr.data.resize(vertices.size() * sizeof(Vector2));
    auto* uv_dst = reinterpret_cast<Vector2*>(uv_attr.data.data());
    for (usize i = 0; i < vertices.size(); ++i) {
        uv_dst[i] = vertices[i].uv;
    }
    lod.attributes.push_back(std::move(uv_attr));

    // single surface covering entire procedural primitive
    MeshSurface surface;
    surface.slice.first_index  = 0;
    surface.slice.index_count  = static_cast<uint32_t>(indices.size());
    surface.slice.first_vertex = 0;
    surface.slice.vertex_count = static_cast<uint32_t>(vertices.size());
    surface.min_bounds         = min_bounds;
    surface.max_bounds         = max_bounds;
    lod.surfaces.push_back(surface);

    mesh.lods.push_back(std::move(lod));
    return mesh;
}

const MeshAsset* lyra::find_procedural_mesh(AssetID guid)
{
    static const MeshAsset cube      = create_cube_mesh(1.0f);
    static const MeshAsset sphere    = create_sphere_mesh(0.5f);
    static const MeshAsset plane     = create_plane_mesh(10.0f, 10.0f);
    static const MeshAsset cylinder  = create_cylinder_mesh(0.5f, 1.0f);
    static const MeshAsset icosphere = create_icosphere_mesh(0.5f, 2);

    switch (guid) {
        case PROCEDURAL_CUBE_GUID:
            return &cube;
        case PROCEDURAL_SPHERE_GUID:
            return &sphere;
        case PROCEDURAL_PLANE_GUID:
            return &plane;
        case PROCEDURAL_CYLINDER_GUID:
            return &cylinder;
        case PROCEDURAL_ICOSPHERE_GUID:
            return &icosphere;
        default:
            return nullptr;
    }
}

void lyra::init_procedural_meshes(GeometryCache& cache)
{
    if (const auto* cube = find_procedural_mesh(PROCEDURAL_CUBE_GUID)) {
        cache.acquire(MeshAssetHandle(PROCEDURAL_CUBE_GUID), *cube, GeometryForm::INDEXED);
    }
    if (const auto* sphere = find_procedural_mesh(PROCEDURAL_SPHERE_GUID)) {
        cache.acquire(MeshAssetHandle(PROCEDURAL_SPHERE_GUID), *sphere, GeometryForm::INDEXED);
    }
    if (const auto* plane = find_procedural_mesh(PROCEDURAL_PLANE_GUID)) {
        cache.acquire(MeshAssetHandle(PROCEDURAL_PLANE_GUID), *plane, GeometryForm::INDEXED);
    }
    if (const auto* cylinder = find_procedural_mesh(PROCEDURAL_CYLINDER_GUID)) {
        cache.acquire(MeshAssetHandle(PROCEDURAL_CYLINDER_GUID), *cylinder, GeometryForm::INDEXED);
    }
    if (const auto* icosphere = find_procedural_mesh(PROCEDURAL_ICOSPHERE_GUID)) {
        cache.acquire(MeshAssetHandle(PROCEDURAL_ICOSPHERE_GUID), *icosphere, GeometryForm::INDEXED);
    }
}

MeshAsset lyra::create_cube_mesh(float size)
{
    float half = size * 0.5f;

    // clang-format off
    Vector<ProceduralVertex> vertices = {
        // +Z face (front)
        {{ -half, -half,  half }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }},
        {{  half, -half,  half }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }},
        {{  half,  half,  half }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }},
        {{ -half,  half,  half }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }},

        // -Z face (back)
        {{  half, -half, -half }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }},
        {{ -half, -half, -half }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }},
        {{ -half,  half, -half }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }},
        {{  half,  half, -half }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }},

        // +X face (right)
        {{  half, -half,  half }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }},
        {{  half, -half, -half }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }},
        {{  half,  half, -half }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }},
        {{  half,  half,  half }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }},

        // -X face (left)
        {{ -half, -half, -half }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }},
        {{ -half, -half,  half }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }},
        {{ -half,  half,  half }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }},
        {{ -half,  half, -half }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }},

        // +Y face (top)
        {{ -half,  half,  half }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }},
        {{  half,  half,  half }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }},
        {{  half,  half, -half }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }},
        {{ -half,  half, -half }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }},

        // -Y face (bottom)
        {{ -half, -half, -half }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f }},
        {{  half, -half, -half }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f }},
        {{  half, -half,  half }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f }},
        {{ -half, -half,  half }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f }},
    };

    Vector<uint32_t> indices = {
        0,  1,  2,   2,  3,  0, // +Z
        4,  5,  6,   6,  7,  4, // -Z
        8,  9, 10,  10, 11,  8, // +X
       12, 13, 14,  14, 15, 12, // -X
       16, 17, 18,  18, 19, 16, // +Y
       20, 21, 22,  22, 23, 20  // -Y
    };
    // clang-format on

    return build_procedural_mesh_asset(vertices, indices, Vector3(-half), Vector3(half));
}

MeshAsset lyra::create_sphere_mesh(float radius, uint rings, uint sectors)
{
    Vector<ProceduralVertex> vertices;
    Vector<uint32_t>         indices;

    const float R = 1.0f / static_cast<float>(rings - 1);
    const float S = 1.0f / static_cast<float>(sectors - 1);

    for (uint r = 0; r < rings; ++r) {
        for (uint s = 0; s < sectors; ++s) {
            const float y = std::sin(-PI * 0.5f + PI * static_cast<float>(r) * R);
            const float x = std::cos(2.0f * PI * static_cast<float>(s) * S) * std::sin(PI * static_cast<float>(r) * R);
            const float z = std::sin(2.0f * PI * static_cast<float>(s) * S) * std::sin(PI * static_cast<float>(r) * R);

            ProceduralVertex v;
            v.normal = Vector3(x, y, z);
            v.pos    = v.normal * radius;
            v.uv     = Vector2(static_cast<float>(s) * S, static_cast<float>(r) * R);
            vertices.push_back(v);
        }
    }

    for (uint r = 0; r < rings - 1; ++r) {
        for (uint s = 0; s < sectors - 1; ++s) {
            uint32_t cur  = r * sectors + s;
            uint32_t next = (r + 1) * sectors + s;

            indices.push_back(cur);
            indices.push_back(next);
            indices.push_back(next + 1);

            indices.push_back(cur);
            indices.push_back(next + 1);
            indices.push_back(cur + 1);
        }
    }

    return build_procedural_mesh_asset(vertices, indices, Vector3(-radius), Vector3(radius));
}

MeshAsset lyra::create_plane_mesh(float width, float depth, uint subdivisions_x, uint subdivisions_z)
{
    Vector<ProceduralVertex> vertices;
    Vector<uint32_t>         indices;

    float half_w = width * 0.5f;
    float half_d = depth * 0.5f;

    uint vx_count = subdivisions_x + 1;
    uint vz_count = subdivisions_z + 1;

    for (uint z = 0; z < vz_count; ++z) {
        float fz = static_cast<float>(z) / static_cast<float>(subdivisions_z);
        float pz = -half_d + fz * depth;

        for (uint x = 0; x < vx_count; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(subdivisions_x);
            float px = -half_w + fx * width;

            ProceduralVertex v;
            v.pos    = Vector3(px, 0.0f, pz);
            v.normal = Vector3(0.0f, 1.0f, 0.0f);
            v.uv     = Vector2(fx, fz);
            vertices.push_back(v);
        }
    }

    for (uint z = 0; z < subdivisions_z; ++z) {
        for (uint x = 0; x < subdivisions_x; ++x) {
            uint32_t row1 = z * vx_count;
            uint32_t row2 = (z + 1) * vx_count;

            indices.push_back(row1 + x);
            indices.push_back(row2 + x);
            indices.push_back(row2 + x + 1);

            indices.push_back(row1 + x);
            indices.push_back(row2 + x + 1);
            indices.push_back(row1 + x + 1);
        }
    }

    return build_procedural_mesh_asset(
        vertices,
        indices,
        Vector3(-half_w, 0.0f, -half_d),
        Vector3(half_w, 0.0f, half_d));
}

MeshAsset lyra::create_cylinder_mesh(float radius, float height, uint segments)
{
    Vector<ProceduralVertex> vertices;
    Vector<uint32_t>         indices;

    float half_h = height * 0.5f;

    // side vertices
    for (uint i = 0; i <= segments; ++i) {
        float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * PI;
        float cos_t = std::cos(theta);
        float sin_t = std::sin(theta);
        float u     = static_cast<float>(i) / static_cast<float>(segments);

        // bottom vertex
        vertices.push_back({{radius * cos_t, -half_h, radius * sin_t}, {cos_t, 0.0f, sin_t}, {u, 0.0f}});
        // top vertex
        vertices.push_back({{radius * cos_t, half_h, radius * sin_t}, {cos_t, 0.0f, sin_t}, {u, 1.0f}});
    }

    for (uint i = 0; i < segments; ++i) {
        uint32_t b0 = i * 2;
        uint32_t t0 = b0 + 1;
        uint32_t b1 = (i + 1) * 2;
        uint32_t t1 = b1 + 1;

        indices.push_back(b0);
        indices.push_back(t0);
        indices.push_back(t1);

        indices.push_back(b0);
        indices.push_back(t1);
        indices.push_back(b1);
    }

    // top and bottom caps
    uint32_t top_center_idx = static_cast<uint32_t>(vertices.size());
    vertices.push_back({{0.0f, half_h, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}});
    for (uint i = 0; i <= segments; ++i) {
        float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * PI;
        vertices.push_back({{radius * std::cos(theta), half_h, radius * std::sin(theta)}, {0.0f, 1.0f, 0.0f}, {0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)}});
    }
    for (uint i = 0; i < segments; ++i) {
        indices.push_back(top_center_idx);
        indices.push_back(top_center_idx + 1 + i);
        indices.push_back(top_center_idx + 2 + i);
    }

    uint32_t bot_center_idx = static_cast<uint32_t>(vertices.size());
    vertices.push_back({{0.0f, -half_h, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});
    for (uint i = 0; i <= segments; ++i) {
        float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * PI;
        vertices.push_back({{radius * std::cos(theta), -half_h, radius * std::sin(theta)}, {0.0f, -1.0f, 0.0f}, {0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta)}});
    }
    for (uint i = 0; i < segments; ++i) {
        indices.push_back(bot_center_idx);
        indices.push_back(bot_center_idx + 2 + i);
        indices.push_back(bot_center_idx + 1 + i);
    }

    return build_procedural_mesh_asset(
        vertices,
        indices,
        Vector3(-radius, -half_h, -radius),
        Vector3(radius, half_h, radius));
}

MeshAsset lyra::create_icosphere_mesh(float radius, uint subdivisions)
{
    // 12 base vertices of an icosahedron
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

    // clang-format off
    Vector<Vector3> positions = {
        glm::normalize(Vector3(-1.0f,     t,  0.0f)),
        glm::normalize(Vector3( 1.0f,     t,  0.0f)),
        glm::normalize(Vector3(-1.0f,    -t,  0.0f)),
        glm::normalize(Vector3( 1.0f,    -t,  0.0f)),

        glm::normalize(Vector3( 0.0f, -1.0f,     t)),
        glm::normalize(Vector3( 0.0f,  1.0f,     t)),
        glm::normalize(Vector3( 0.0f, -1.0f,    -t)),
        glm::normalize(Vector3( 0.0f,  1.0f,    -t)),

        glm::normalize(Vector3(    t,  0.0f, -1.0f)),
        glm::normalize(Vector3(    t,  0.0f,  1.0f)),
        glm::normalize(Vector3(   -t,  0.0f, -1.0f)),
        glm::normalize(Vector3(   -t,  0.0f,  1.0f)),
    };

    // 20 initial triangular faces
    Vector<uint32_t> indices = {
        0, 11,  5,    0,  5,  1,    0,  1,  7,    0,  7, 10,    0, 10, 11,
        1,  5,  9,    5, 11,  4,   11, 10,  2,   10,  7,  6,    7,  1,  8,
        3,  9,  4,    3,  4,  2,    3,  2,  6,    3,  6,  8,    3,  8,  9,
        4,  9,  5,    2,  4, 11,    6,  2, 10,    8,  6,  7,    9,  8,  1,
    };
    // clang-format on

    // recursive midpoint subdivision
    HashMap<uint64_t, uint32_t> midpoint_cache;
    auto                        get_middle_point = [&](uint32_t p1, uint32_t p2) -> uint32_t {
        uint64_t smaller = std::min(p1, p2);
        uint64_t greater = std::max(p1, p2);
        uint64_t key     = (smaller << 32) | greater;

        auto it = midpoint_cache.find(key);
        if (it != midpoint_cache.end()) {
            return it->second;
        }

        Vector3  middle = glm::normalize((positions[p1] + positions[p2]) * 0.5f);
        uint32_t idx    = static_cast<uint32_t>(positions.size());
        positions.push_back(middle);
        midpoint_cache[key] = idx;
        return idx;
    };

    for (uint i = 0; i < subdivisions; ++i) {
        Vector<uint32_t> next_indices;
        next_indices.reserve(indices.size() * 4);
        for (usize j = 0; j < indices.size(); j += 3) {
            uint32_t a = indices[j];
            uint32_t b = indices[j + 1];
            uint32_t c = indices[j + 2];

            uint32_t ab = get_middle_point(a, b);
            uint32_t bc = get_middle_point(b, c);
            uint32_t ca = get_middle_point(c, a);

            next_indices.push_back(a);
            next_indices.push_back(ab);
            next_indices.push_back(ca);

            next_indices.push_back(b);
            next_indices.push_back(bc);
            next_indices.push_back(ab);

            next_indices.push_back(c);
            next_indices.push_back(ca);
            next_indices.push_back(bc);

            next_indices.push_back(ab);
            next_indices.push_back(bc);
            next_indices.push_back(ca);
        }
        indices = std::move(next_indices);
    }

    // construct final vertex data
    Vector<ProceduralVertex> vertices;
    vertices.reserve(positions.size());
    for (const auto& pos : positions) {
        ProceduralVertex v;
        v.normal = pos;
        v.pos    = pos * radius;

        // spherical uv mapping
        v.uv.x = 0.5f + std::atan2(pos.z, pos.x) / (2.0f * PI);
        v.uv.y = 0.5f - std::asin(std::clamp(pos.y, -1.0f, 1.0f)) / PI;

        vertices.push_back(v);
    }

    return build_procedural_mesh_asset(vertices, indices, Vector3(-radius), Vector3(radius));
}
