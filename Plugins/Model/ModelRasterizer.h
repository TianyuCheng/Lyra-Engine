#pragma once

#include <Lyra/Common/Config.h>
#include <Lyra/Common/Math.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Assets/AMSAPI.h>

namespace lyra::model
{
    /**
     * @brief Geometry container for offline thumbnail rasterization.
     */
    struct RasterizerMesh
    {
        Vector<Vector3> positions;
        Vector<Vector3> normals;
        Vector<uint>    indices;
        Matrix4x4       transform = Matrix4x4(1.0f);
    };

    /**
     * @brief Rasterizes one or more meshes offline on CPU and outputs a 128x128 thumbnail PNG.
     * @param metadata JSON metadata for the asset (will have "thumbnail" set on success).
     * @param meshes One or more submeshes / nodes to rasterize.
     * @param caches_root Root cache directory.
     * @return True if thumbnail was successfully generated and saved.
     */
    bool generate_model_thumbnail(
        JSON&                         metadata,
        const Vector<RasterizerMesh>& meshes,
        OSPath                        caches_root);

    /**
     * @brief Convenience overload for a single mesh (e.g. OBJ / STL).
     */
    bool generate_model_thumbnail(
        JSON&                  metadata,
        const Vector<Vector3>& positions,
        const Vector<Vector3>& normals,
        const Vector<uint>&    indices,
        OSPath                 caches_root);
} // namespace lyra::model
