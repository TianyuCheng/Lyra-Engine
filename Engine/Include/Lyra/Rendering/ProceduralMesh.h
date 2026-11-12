#pragma once

#ifndef LYRA_ENGINE_RENDERING_PROCEDURAL_MESH_H
#define LYRA_ENGINE_RENDERING_PROCEDURAL_MESH_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Assets/Format/MeshAsset.h>
#include <Lyra/Rendering/GeometryCache.h>

namespace lyra
{
    namespace detail
    {
        struct ProceduralVertex
        {
            Vector3 pos;
            Vector3 normal;
            Vector2 uv;
        };

        MeshAsset build_procedural_mesh_asset(
            const Vector<ProceduralVertex>& vertices,
            const Vector<uint32_t>&         indices,
            const Vector3&                  min_bounds,
            const Vector3&                  max_bounds);
    }

    // deterministic identifiers for standard procedural mesh primitives
    inline constexpr AssetID PROCEDURAL_CUBE_GUID      = 0xC0BE000000000001ULL;
    inline constexpr AssetID PROCEDURAL_SPHERE_GUID    = 0x548E000000000002ULL;
    inline constexpr AssetID PROCEDURAL_PLANE_GUID     = 0x91AE000000000003ULL;
    inline constexpr AssetID PROCEDURAL_CYLINDER_GUID  = 0xCE1D000000000004ULL;
    inline constexpr AssetID PROCEDURAL_ICOSPHERE_GUID = 0x1C05000000000005ULL;

    /**
     * @brief pre-allocates and uploads all standard procedural meshes into the gpu geometry cache.
     */
    void init_procedural_meshes(GeometryCache& cache);

    /**
     * @brief retrieves a standard procedural mesh asset by its deterministic GUID.
     * @return pointer to procedural mesh asset, or nullptr if not a recognized procedural GUID.
     */
    const MeshAsset* find_procedural_mesh(AssetID guid);

    /**
     * @brief generates a cube mesh centered at origin with position, normal, and texcoord attributes.
     */
    MeshAsset create_cube_mesh(float size = 1.0f);

    /**
     * @brief generates a uv sphere mesh centered at origin.
     */
    MeshAsset create_sphere_mesh(float radius = 0.5f, uint rings = 16, uint sectors = 24);

    /**
     * @brief generates an icosphere mesh centered at origin via recursive icosahedron subdivision.
     */
    MeshAsset create_icosphere_mesh(float radius = 0.5f, uint subdivisions = 2);

    /**
     * @brief generates a horizontal flat plane mesh lying on the xz plane with normal +y.
     */
    MeshAsset create_plane_mesh(float width = 10.0f, float depth = 10.0f, uint subdivisions_x = 1, uint subdivisions_z = 1);

    /**
     * @brief generates a vertical cylinder mesh centered at origin.
     */
    MeshAsset create_cylinder_mesh(float radius = 0.5f, float height = 1.0f, uint segments = 24);

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_PROCEDURAL_MESH_H
