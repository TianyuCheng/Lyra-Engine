#include "SoftRaytracing.h"
#include <mutex>

using namespace lyra;
using namespace lyra::softgpu;

static RTCDevice s_embree_device = nullptr;
static Mutex     s_embree_mutex;

auto lyra::softgpu::get_or_create_embree_device() -> RTCDevice
{
    LockGuard lock(s_embree_mutex);
    if (!s_embree_device) {
        s_embree_device = rtcNewDevice("isa=avx2");
    }
    return s_embree_device;
}

SoftBlas::SoftBlas(const GPUBlasDescriptor& desc)
    : label(desc.label ? desc.label : "")
{
}

SoftBlas::~SoftBlas()
{
    if (scene) {
        rtcReleaseScene(scene);
        scene = nullptr;
    }
}

SoftBlas::SoftBlas(SoftBlas&& other) noexcept
    : scene(other.scene), label(other.label), fallback_indices(std::move(other.fallback_indices)), built(other.built)
{
    other.scene = nullptr;
    other.built = false;
}

SoftBlas& SoftBlas::operator=(SoftBlas&& other) noexcept
{
    if (this != &other) {
        if (scene) {
            rtcReleaseScene(scene);
        }
        scene            = other.scene;
        label            = other.label;
        fallback_indices = std::move(other.fallback_indices);
        built            = other.built;
        other.scene      = nullptr;
        other.built      = false;
    }
    return *this;
}

void SoftBlas::build(const GPUBlasBuildEntry& entry, const Slotmap<SoftBuffer>& buffer_pool)
{
    RTCDevice device = get_or_create_embree_device();
    if (scene) {
        rtcReleaseScene(scene);
        scene = nullptr;
    }

    scene = rtcNewScene(device);
    rtcSetSceneFlags(scene, RTC_SCENE_FLAG_ROBUST);
    fallback_indices.clear();

    for (const auto& tri_geom : entry.geometries.triangles) {
        if (!tri_geom.vertex_buffer.valid()) continue;

        const auto&    vbuf    = fetch_resource(const_cast<Slotmap<SoftBuffer>&>(buffer_pool), tri_geom.vertex_buffer);
        const uint8_t* vdata   = vbuf.data.data();
        usize          vstride = tri_geom.vertex_stride;
        const uint8_t* vptr    = vdata + tri_geom.first_vertex * vstride;

        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

        if (tri_geom.index_buffer.valid() && tri_geom.size.index_count > 0) {
            const auto&    ibuf           = fetch_resource(const_cast<Slotmap<SoftBuffer>&>(buffer_pool), tri_geom.index_buffer);
            const uint8_t* idata          = ibuf.data.data();
            bool           is_16          = (tri_geom.size.index_format == GPUIndexFormat::UINT16);
            uint32_t       triangle_count = tri_geom.size.index_count / 3;
            size_t         idx_stride     = is_16 ? sizeof(uint16_t) * 3 : sizeof(uint32_t) * 3;
            const uint8_t* iptr           = idata + tri_geom.first_index * (is_16 ? 2 : 4);

            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                RTC_FORMAT_FLOAT3, vptr, 0, vstride, tri_geom.size.vertex_count);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                is_16 ? RTC_FORMAT_USHORT3 : RTC_FORMAT_UINT3, iptr, 0, idx_stride, triangle_count);
        } else {
            uint32_t triangle_count = tri_geom.size.vertex_count / 3;
            fallback_indices.resize(tri_geom.size.vertex_count);
            for (uint32_t i = 0; i < tri_geom.size.vertex_count; ++i) {
                fallback_indices[i] = i;
            }

            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                RTC_FORMAT_FLOAT3, vptr, 0, vstride, tri_geom.size.vertex_count);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                RTC_FORMAT_UINT3, fallback_indices.data(), 0, sizeof(uint32_t) * 3, triangle_count);
        }

        rtcCommitGeometry(geom);
        rtcAttachGeometry(scene, geom);
        rtcReleaseGeometry(geom);
    }

    rtcCommitScene(scene);
    built = true;
}

bool SoftBlas::intersect(const Vector3& origin, const Vector3& dir, float t_min, float t_max,
    float& hit_t, float& hit_u, float& hit_v, uint32_t& hit_geom, uint32_t& hit_prim) const
{
    if (!scene || !built) return false;

    RTCIntersectArguments args;
    rtcInitIntersectArguments(&args);
    args.flags = RTC_RAY_QUERY_FLAG_INCOHERENT;

    alignas(16) RTCRayHit rayhit{};
    rayhit.ray.org_x  = origin.x;
    rayhit.ray.org_y  = origin.y;
    rayhit.ray.org_z  = origin.z;
    rayhit.ray.dir_x  = dir.x;
    rayhit.ray.dir_y  = dir.y;
    rayhit.ray.dir_z  = dir.z;
    rayhit.ray.tnear  = t_min;
    rayhit.ray.tfar   = t_max;
    rayhit.ray.mask   = 0xFF;
    rayhit.ray.time   = 0.0f;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;
    for (unsigned int l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l) {
        rayhit.hit.instID[l] = RTC_INVALID_GEOMETRY_ID;
    }

    rtcIntersect1(scene, &rayhit, &args);

    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        hit_t    = rayhit.ray.tfar;
        hit_u    = rayhit.hit.u;
        hit_v    = rayhit.hit.v;
        hit_geom = rayhit.hit.geomID;
        hit_prim = rayhit.hit.primID;
        return true;
    }
    return false;
}

SoftTlas::SoftTlas(const GPUTlasDescriptor& desc)
    : max_instances(desc.max_instances), label(desc.label ? desc.label : "")
{
}

SoftTlas::~SoftTlas()
{
    if (scene) {
        rtcReleaseScene(scene);
        scene = nullptr;
    }
}

SoftTlas::SoftTlas(SoftTlas&& other) noexcept
    : scene(other.scene), instances(std::move(other.instances)), max_instances(other.max_instances), label(other.label), built(other.built)
{
    other.scene = nullptr;
    other.built = false;
}

SoftTlas& SoftTlas::operator=(SoftTlas&& other) noexcept
{
    if (this != &other) {
        if (scene) {
            rtcReleaseScene(scene);
        }
        scene         = other.scene;
        instances     = std::move(other.instances);
        max_instances = other.max_instances;
        label         = other.label;
        built         = other.built;
        other.scene   = nullptr;
        other.built   = false;
    }
    return *this;
}

void SoftTlas::build(const GPUTlasBuildEntry& entry, const Slotmap<SoftBlas>& blas_pool)
{
    RTCDevice device = get_or_create_embree_device();
    if (scene) {
        rtcReleaseScene(scene);
        scene = nullptr;
    }

    scene = rtcNewScene(device);
    rtcSetSceneFlags(scene, RTC_SCENE_FLAG_ROBUST);
    instances.clear();

    for (const auto& desc_inst : entry.instances) {
        if (!desc_inst.blas.valid()) continue;

        const auto* blas = blas_pool.find(desc_inst.blas.to_slotmap_handle<SoftBlas>());
        if (!blas || !blas->scene) continue;

        RTCGeometry inst_geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(inst_geom, blas->scene);

        float xfm[16] = {
            desc_inst.transform[0][0], desc_inst.transform[1][0], desc_inst.transform[2][0], 0.0f,
            desc_inst.transform[0][1], desc_inst.transform[1][1], desc_inst.transform[2][1], 0.0f,
            desc_inst.transform[0][2], desc_inst.transform[1][2], desc_inst.transform[2][2], 0.0f,
            desc_inst.transform[0][3], desc_inst.transform[1][3], desc_inst.transform[2][3], 1.0f};
        rtcSetGeometryTransform(inst_geom, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, xfm);
        rtcSetGeometryMask(inst_geom, static_cast<unsigned int>(desc_inst.mask));

        rtcCommitGeometry(inst_geom);
        rtcAttachGeometry(scene, inst_geom);
        rtcReleaseGeometry(inst_geom);

        SoftTlasInstance ti{};
        ti.custom_index = desc_inst.custom_data;
        ti.mask         = desc_inst.mask;
        ti.blas         = desc_inst.blas;
        instances.push_back(ti);
    }

    rtcCommitScene(scene);
    built = true;
}

bool SoftTlas::intersect(const Vector3& origin, const Vector3& dir, float t_min, float t_max, uint8_t ray_mask,
    const Slotmap<SoftBlas>& /*blas_pool*/,
    float& hit_t, float& hit_u, float& hit_v, uint32_t& hit_instance, uint32_t& hit_geom, uint32_t& hit_prim) const
{
    if (!scene || !built) return false;

    RTCIntersectArguments args;
    rtcInitIntersectArguments(&args);
    args.flags = RTC_RAY_QUERY_FLAG_INCOHERENT;

    alignas(16) RTCRayHit rayhit{};
    rayhit.ray.org_x  = origin.x;
    rayhit.ray.org_y  = origin.y;
    rayhit.ray.org_z  = origin.z;
    rayhit.ray.dir_x  = dir.x;
    rayhit.ray.dir_y  = dir.y;
    rayhit.ray.dir_z  = dir.z;
    rayhit.ray.tnear  = t_min;
    rayhit.ray.tfar   = t_max;
    rayhit.ray.mask   = ray_mask;
    rayhit.ray.time   = 0.0f;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;
    for (unsigned int l = 0; l < RTC_MAX_INSTANCE_LEVEL_COUNT; ++l) {
        rayhit.hit.instID[l] = RTC_INVALID_GEOMETRY_ID;
    }

    rtcIntersect1(scene, &rayhit, &args);

    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        hit_t             = rayhit.ray.tfar;
        hit_u             = rayhit.hit.u;
        hit_v             = rayhit.hit.v;
        uint32_t inst_idx = (rayhit.hit.instID[0] != RTC_INVALID_GEOMETRY_ID) ? rayhit.hit.instID[0] : 0;
        hit_instance      = (inst_idx < instances.size()) ? instances[inst_idx].custom_index : inst_idx;
        hit_geom          = rayhit.hit.geomID;
        hit_prim          = rayhit.hit.primID;
        return true;
    }
    return false;
}

void SoftRayQuery::trace_ray_inline(const SoftTlas& tlas, uint32_t /*flags*/, uint8_t ray_mask,
    const Vector3& orig, const Vector3& dir, float min_t, float max_t,
    const Slotmap<SoftBlas>& blas_pool)
{
    origin    = orig;
    direction = dir;
    t_min     = min_t;
    t_max     = max_t;
    mask      = ray_mask;

    float    t, u, v;
    uint32_t inst_id, geom_id, prim_id;
    if (tlas.intersect(orig, dir, min_t, max_t, ray_mask, blas_pool, t, u, v, inst_id, geom_id, prim_id)) {
        status       = SoftRayQueryStatus::TRIANGLE_HIT;
        hit_t        = t;
        barycentrics = Vector2(u, v);
        instance_id  = inst_id;
        geometry_id  = geom_id;
        primitive_id = prim_id;
    } else {
        status       = SoftRayQueryStatus::NOTHING;
        hit_t        = max_t;
        barycentrics = Vector2(0.0f);
    }
}
