#include <Lyra/UISystem/Widgets/UIGizmo.h>
#include "ImGui.h"
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

using namespace lyra;

void ui::gizmo::begin_frame()
{
    ImGuizmo::BeginFrame();
}

void ui::gizmo::set_enabled(bool enabled)
{
    ImGuizmo::Enable(enabled);
}

bool ui::gizmo::is_over()
{
    return ImGuizmo::IsOver();
}

bool ui::gizmo::is_using()
{
    return ImGuizmo::IsUsing();
}

void ui::gizmo::set_orthographic(bool is_orthographic)
{
    ImGuizmo::SetOrthographic(is_orthographic);
}

void ui::gizmo::set_rect(const Vector2& screen_pos, const Vector2& extent)
{
    ImGuizmo::SetRect(screen_pos.x, screen_pos.y, extent.x, extent.y);
}

void ui::gizmo::set_drawlist(void* drawlist)
{
    ImGuizmo::SetDrawlist(static_cast<ImDrawList*>(drawlist));
}

bool ui::gizmo::manipulate(
    const Matrix4x4& view,
    const Matrix4x4& projection,
    Operation        operation,
    Mode             mode,
    Matrix4x4&       matrix,
    Matrix4x4*       delta_matrix,
    const float*     snap)
{
    auto im_op   = static_cast<ImGuizmo::OPERATION>(operation);
    auto im_mode = static_cast<ImGuizmo::MODE>(mode);

    return ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        im_op,
        im_mode,
        glm::value_ptr(matrix),
        delta_matrix ? glm::value_ptr(*delta_matrix) : nullptr,
        snap);
}

void ui::gizmo::decompose(
    const Matrix4x4& matrix,
    Vector3&         out_translation,
    Vector3&         out_rotation_euler,
    Vector3&         out_scale)
{
    ImGuizmo::DecomposeMatrixToComponents(
        glm::value_ptr(matrix),
        glm::value_ptr(out_translation),
        glm::value_ptr(out_rotation_euler),
        glm::value_ptr(out_scale));
}

void ui::gizmo::recompose(
    const Vector3& translation,
    const Vector3& rotation_euler,
    const Vector3& scale,
    Matrix4x4&     out_matrix)
{
    ImGuizmo::RecomposeMatrixFromComponents(
        glm::value_ptr(translation),
        glm::value_ptr(rotation_euler),
        glm::value_ptr(scale),
        glm::value_ptr(out_matrix));
}
