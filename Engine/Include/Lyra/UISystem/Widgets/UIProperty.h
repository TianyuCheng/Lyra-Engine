#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UIPROPERTY_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UIPROPERTY_H

#include <array>
#include <type_traits>
#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/UISystem/Widgets/UI.h>

namespace lyra::ui
{
    struct VectorConfig
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

    struct ColorConfig
    {
        bool hdr    = false;
        bool alpha  = false;
        bool wheel  = false;
        bool inputs = true;
    };

    using VecConfig    = VectorConfig;
    using VecLimits    = VectorConfig;
    using NumberLimits = ScalarConfig;

    // collapsible component section with standardized header and icon
    void section(CString title, CString icon, ActionRef content);

    // standardized two-column property sheet (auto-aligned labels on left, widgets on right)
    void properties(ActionRef content);

    // vector inputs
    void vec3(CString label, Vector3& value, const VectorConfig& config = {});
    void vec3(CString label, Vector3& value, ActionRef on_change, const VectorConfig& config = {});
    void vec3(CString label, Vector3& value, ChangeRef<Vector3> on_change, const VectorConfig& config = {});

    void vec2(CString label, Vector2& value, const VectorConfig& config = {});
    void vec2(CString label, Vector2& value, ActionRef on_change, const VectorConfig& config = {});
    void vec2(CString label, Vector2& value, ChangeRef<Vector2> on_change, const VectorConfig& config = {});

    // numeric inputs
    void number(CString label, float& value, const ScalarConfig& config = {});
    void number(CString label, float& value, ActionRef on_change, const ScalarConfig& config = {});

    void integer(CString label, int& value, int speed = 1, int min = 0, int max = 0);
    void integer(CString label, int& value, ActionRef on_change, int speed = 1, int min = 0, int max = 0);

    // color inputs
    void color(CString label, Vector3& rgb, const ColorConfig& config = {});
    void color(CString label, Vector3& rgb, ActionRef on_change, const ColorConfig& config = {});

    void color(CString label, Vector4& rgba, const ColorConfig& config = {});
    void color(CString label, Vector4& rgba, ActionRef on_change, const ColorConfig& config = {});

    // color picker inputs (inline full color wheel / picker widget)
    void color_picker(CString label, Vector3& rgb, const ColorConfig& config = {});
    void color_picker(CString label, Vector3& rgb, ActionRef on_change, const ColorConfig& config = {});

    void color_picker(CString label, Vector4& rgba, const ColorConfig& config = {});
    void color_picker(CString label, Vector4& rgba, ActionRef on_change, const ColorConfig& config = {});

    // text inputs
    void text(CString label, char* buffer, size_t buffer_size);
    void text(CString label, char* buffer, size_t buffer_size, ActionRef on_commit);

    // toggle
    void toggle(CString label, bool& value);
    void toggle(CString label, bool& value, ChangeRef<bool> on_toggle);

    // dropdown selection
    void dropdown(CString label, CString current_item, const CString* items, size_t count, ChangeRef<size_t> on_select);

    // templated enum dropdown using magic_enum
    template <typename E>
    requires std::is_enum_v<E>
    void enumeration(CString label, E& value)
    {
        constexpr auto entries      = magic_enum::enum_entries<E>();
        auto           current_name = String(magic_enum::enum_name(value));

        Array<String, entries.size()>  item_strings;
        Array<CString, entries.size()> items;
        for (size_t i = 0; i < entries.size(); ++i) {
            item_strings[i] = String(entries[i].second);
            items[i]        = item_strings[i].c_str();
        }

        dropdown(label, current_name.c_str(), items.data(), entries.size(), [&](size_t selected_index) {
            if (selected_index < entries.size()) {
                value = entries[selected_index].first;
            }
        });
    }

    template <typename E>
    requires std::is_enum_v<E>
    void enumeration(CString label, E& value, ChangeRef<E> on_change)
    {
        constexpr auto entries      = magic_enum::enum_entries<E>();
        auto           current_name = String(magic_enum::enum_name(value));

        Array<String, entries.size()>  item_strings;
        Array<CString, entries.size()> items;
        for (size_t i = 0; i < entries.size(); ++i) {
            item_strings[i] = String(entries[i].second);
            items[i]        = item_strings[i].c_str();
        }

        dropdown(label, current_name.c_str(), items.data(), entries.size(), [&](size_t selected_index) {
            if (selected_index < entries.size()) {
                value = entries[selected_index].first;
                on_change(value);
            }
        });
    }

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UIPROPERTY_H
