#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UIGIZMO_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UIGIZMO_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/BitFlags.h>

namespace lyra::ui
{
    enum struct GizmoOperation : uint32_t
    {
        TranslateX   = 1 << 0,
        TranslateY   = 1 << 1,
        TranslateZ   = 1 << 2,
        RotateX      = 1 << 3,
        RotateY      = 1 << 4,
        RotateZ      = 1 << 5,
        RotateScreen = 1 << 6,
        ScaleX       = 1 << 7,
        ScaleY       = 1 << 8,
        ScaleZ       = 1 << 9,
        Bounds       = 1 << 10,
        ScaleUniform = 1 << 11,

        Translate = TranslateX | TranslateY | TranslateZ,
        Rotate    = RotateX | RotateY | RotateZ | RotateScreen,
        Scale     = ScaleX | ScaleY | ScaleZ,
        Universal = Translate | Rotate | Scale
    };
} // namespace lyra::ui

ENABLE_BIT_FLAGS(lyra::ui::GizmoOperation);

namespace lyra::ui::gizmo
{
    using Operation  = GizmoOperation;
    using Operations = BitFlags<GizmoOperation>;

    enum struct Mode : uint32_t
    {
        Local = 0,
        World = 1
    };

    struct SnapSettings
    {
        bool    snap_enabled     = false;
        Vector3 translation_snap = Vector3(1.0f);
        float   rotation_snap    = 15.0f; // degrees
        float   scale_snap       = 0.1f;
    };

    /**
     * @brief call at the start of each frame before any gizmo operations.
     */
    void begin_frame();

    /**
     * @brief enable or disable gizmo interaction.
     */
    void set_enabled(bool enabled);

    /**
     * @brief query whether the mouse is currently hovering over any gizmo handle.
     */
    bool is_over();

    /**
     * @brief query whether the gizmo is actively being manipulated/dragged.
     */
    bool is_using();

    /**
     * @brief set orthographic or perspective projection mode for the gizmo.
     */
    void set_orthographic(bool is_orthographic);

    /**
     * @brief define the viewport rectangle (screen origin and size) where the gizmo is rendered.
     */
    void set_rect(const Vector2& screen_pos, const Vector2& extent);

    /**
     * @brief set custom draw list (defaults to foreground draw list if null).
     */
    void set_drawlist(void* drawlist = nullptr);

    /**
     * @brief manipulate a 4x4 transformation matrix interactively using 3d gizmos.
     * @param view camera view matrix (world-to-camera).
     * @param projection camera projection matrix.
     * @param operation transform operation (Translate, Rotate, Scale, Universal).
     * @param mode coordinate space (Local or World).
     * @param matrix 4x4 matrix to be transformed in-place.
     * @param delta_matrix optional output matrix for the delta transformation.
     * @param snap optional snap array: float[3] for translate, float[1] for rotate/scale.
     * @return true if the matrix was modified by user interaction this frame.
     */
    bool manipulate(
        const Matrix4x4& view,
        const Matrix4x4& projection,
        Operation        operation,
        Mode             mode,
        Matrix4x4&       matrix,
        Matrix4x4*       delta_matrix = nullptr,
        const float*     snap         = nullptr);

    /**
     * @brief helper to decompose a 4x4 matrix into translation, rotation (euler in degrees), and scale.
     */
    void decompose(
        const Matrix4x4& matrix,
        Vector3&         out_translation,
        Vector3&         out_rotation_euler,
        Vector3&         out_scale);

    /**
     * @brief helper to recompose a 4x4 matrix from translation, rotation (euler in degrees), and scale.
     */
    void recompose(
        const Vector3& translation,
        const Vector3& rotation_euler,
        const Vector3& scale,
        Matrix4x4&     out_matrix);

} // namespace lyra::ui::gizmo

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UIGIZMO_H
