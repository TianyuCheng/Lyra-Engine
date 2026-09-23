#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UIPROPERTY_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UIPROPERTY_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/UISystem/Widgets/UI.h>

namespace lyra::ui
{
    struct VecConfig
    {
        float speed = 0.1f;
        float min   = 0.0f;
        float max   = 0.0f;
        float reset = 0.0f;
    };

    struct ScalarConfig
    {
        float speed = 0.1f;
        float min   = 0.0f;
        float max   = 0.0f;
        float reset = 0.0f;
    };

    using VecLimits    = VecConfig;
    using NumberLimits = ScalarConfig;

    // collapsible component section with standardized header and icon
    void section(CString title, CString icon, ActionRef content);

    // standardized two-column property sheet (auto-aligned labels on left, widgets on right)
    void properties(ActionRef content);

    // vector inputs
    void vec3(CString label, Vector3& value, const VecConfig& config = {});
    void vec3(CString label, Vector3& value, ActionRef on_change, const VecConfig& config = {});
    void vec3(CString label, Vector3& value, ChangeRef<Vector3> on_change, const VecConfig& config = {});

    void vec2(CString label, Vector2& value, const VecConfig& config = {});
    void vec2(CString label, Vector2& value, ActionRef on_change, const VecConfig& config = {});
    void vec2(CString label, Vector2& value, ChangeRef<Vector2> on_change, const VecConfig& config = {});

    // numeric inputs
    void number(CString label, float& value, const ScalarConfig& config = {});
    void number(CString label, float& value, ActionRef on_change, const ScalarConfig& config = {});

    void integer(CString label, int& value, int speed = 1, int min = 0, int max = 0);
    void integer(CString label, int& value, ActionRef on_change, int speed = 1, int min = 0, int max = 0);

    // color inputs
    void color(CString label, Vector3& rgb);
    void color(CString label, Vector3& rgb, ActionRef on_change);

    void color(CString label, Vector4& rgba);
    void color(CString label, Vector4& rgba, ActionRef on_change);

    // text inputs
    void text(CString label, char* buffer, size_t buffer_size);
    void text(CString label, char* buffer, size_t buffer_size, ActionRef on_commit);

    // toggle
    void toggle(CString label, bool& value);
    void toggle(CString label, bool& value, ChangeRef<bool> on_toggle);

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UIPROPERTY_H
