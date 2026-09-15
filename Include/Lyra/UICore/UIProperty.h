#pragma once

#ifndef LYRA_LYRA_UICORE_UIPROPERTY_H
#define LYRA_LYRA_UICORE_UIPROPERTY_H

#include <Lyra/Common/Math.h>
#include <Lyra/Common/String.h>
#include <Lyra/UICore/UI.h>

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

    // Collapsible component section with standardized header and icon
    void section(CString title, CString icon, ActionRef content);

    // Standardized two-column property sheet (auto-aligned labels on left, widgets on right)
    void properties(ActionRef content);

    // Vector inputs
    void vec3(CString label, Vector3& value, const VecConfig& config = {});
    void vec3(CString label, Vector3& value, ActionRef on_change, const VecConfig& config = {});
    void vec3(CString label, Vector3& value, ChangeRef<Vector3> on_change, const VecConfig& config = {});

    void vec2(CString label, Vector2& value, const VecConfig& config = {});
    void vec2(CString label, Vector2& value, ActionRef on_change, const VecConfig& config = {});
    void vec2(CString label, Vector2& value, ChangeRef<Vector2> on_change, const VecConfig& config = {});

    // Numeric inputs
    void number(CString label, float& value, const ScalarConfig& config = {});
    void number(CString label, float& value, ActionRef on_change, const ScalarConfig& config = {});

    void integer(CString label, int& value, int speed = 1, int min = 0, int max = 0);
    void integer(CString label, int& value, ActionRef on_change, int speed = 1, int min = 0, int max = 0);

    // Color inputs
    void color(CString label, Vector3& rgb);
    void color(CString label, Vector3& rgb, ActionRef on_change);

    void color(CString label, Vector4& rgba);
    void color(CString label, Vector4& rgba, ActionRef on_change);

    // Text inputs
    void text(CString label, char* buffer, size_t buffer_size);
    void text(CString label, char* buffer, size_t buffer_size, ActionRef on_commit);

    // Toggle
    void toggle(CString label, bool& value);
    void toggle(CString label, bool& value, ChangeRef<bool> on_toggle);

} // namespace lyra::ui

#endif // LYRA_LYRA_UICORE_UIPROPERTY_H
