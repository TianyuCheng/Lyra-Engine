#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_IMGUI_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_IMGUI_H

#include <imgui.h>
#include <imgui_internal.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Macros.h>

namespace lyra::ui::imgui
{
    // convert lyra vector types to imgui types
    FORCE_INLINE auto to_imvec2(const Vector2& v) -> ImVec2
    {
        return ImVec2(v.x, v.y);
    }

    FORCE_INLINE auto to_vector2(const ImVec2& v) -> Vector2
    {
        return Vector2(v.x, v.y);
    }

    FORCE_INLINE auto to_imvec4(const Vector4& v) -> ImVec4
    {
        return ImVec4(v.x, v.y, v.z, v.w);
    }

    FORCE_INLINE auto to_vector4(const ImVec4& v) -> Vector4
    {
        return Vector4(v.x, v.y, v.z, v.w);
    }

} // namespace lyra::ui::imgui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_IMGUI_H
