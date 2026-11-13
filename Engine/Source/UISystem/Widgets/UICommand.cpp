#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <Lyra/UISystem/Widgets/UICommand.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>
#include <Lyra/Utilities/Collections.h>

using namespace lyra;
using namespace lyra::ui;

namespace
{
    struct PaletteState
    {
        bool is_open         = false;
        bool focus_search    = false;
        char search_buf[128] = {0};
        int  selected_index  = 0;
    };

    Vector<CommandDescriptor> g_commands;
    PaletteState              g_palette;
    int                       g_palette_action_frame = -1;

    struct CategoryStyle
    {
        ImU32 icon_bg;
        ImU32 icon_color;
        ImU32 badge_bg;
        ImU32 badge_border;
        ImU32 badge_text;
    };

    auto get_category_style(StringView cat) -> CategoryStyle
    {
        if (cat == "Scene") {
            return {
                ImColor(16, 185, 129, 32),   // icon bg
                ImColor(52, 211, 153, 255),  // icon text
                ImColor(16, 185, 129, 24),   // badge bg
                ImColor(16, 185, 129, 75),   // badge border
                ImColor(52, 211, 153, 240)   // badge text
            };
        } else if (cat == "Assets") {
            return {
                ImColor(245, 158, 11, 32),
                ImColor(251, 191, 36, 255),
                ImColor(245, 158, 11, 24),
                ImColor(245, 158, 11, 75),
                ImColor(251, 191, 36, 240)
            };
        } else if (cat == "Window") {
            return {
                ImColor(168, 85, 247, 32),
                ImColor(192, 132, 252, 255),
                ImColor(168, 85, 247, 24),
                ImColor(168, 85, 247, 75),
                ImColor(192, 132, 252, 240)
            };
        } else if (cat == "Entity") {
            return {
                ImColor(56, 189, 248, 32),
                ImColor(56, 189, 248, 255),
                ImColor(56, 189, 248, 24),
                ImColor(56, 189, 248, 75),
                ImColor(56, 189, 248, 240)
            };
        } else if (cat == "Console") {
            return {
                ImColor(244, 63, 94, 32),
                ImColor(251, 113, 133, 255),
                ImColor(244, 63, 94, 24),
                ImColor(244, 63, 94, 75),
                ImColor(251, 113, 133, 240)
            };
        }
        return {
            ImColor(148, 163, 184, 28),
            ImColor(148, 163, 184, 255),
            ImColor(148, 163, 184, 20),
            ImColor(148, 163, 184, 65),
            ImColor(148, 163, 184, 220)
        };
    }

    auto split_shortcut(StringView shortcut) -> Vector<String>
    {
        Vector<String> keys;
        usize start = 0;
        while (start < shortcut.size()) {
            usize plus = shortcut.find('+', start);
            if (plus == StringView::npos) {
                keys.push_back(String(shortcut.substr(start)));
                break;
            }
            keys.push_back(String(shortcut.substr(start, plus - start)));
            start = plus + 1;
        }
        return keys;
    }

    auto to_lower(StringView str) -> String
    {
        String res(str);
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return res;
    }

    bool fuzzy_subsequence(StringView pattern, StringView text)
    {
        if (pattern.empty()) return true;
        usize pi = 0;
        for (usize ti = 0; ti < text.size() && pi < pattern.size(); ++ti) {
            if (std::tolower(static_cast<unsigned char>(text[ti])) ==
                std::tolower(static_cast<unsigned char>(pattern[pi]))) {
                ++pi;
            }
        }
        return pi == pattern.size();
    }

    auto compute_score(const CommandDescriptor& cmd, StringView query) -> int
    {
        if (query.empty()) return 1;

        String q_lower   = to_lower(query);
        String cat_lower = to_lower(cmd.category);
        String tit_lower = to_lower(cmd.title);
        String full      = cat_lower + " " + tit_lower;

        // split query into whitespace-separated tokens
        Vector<String> tokens;
        usize start = 0;
        while (start < q_lower.size()) {
            while (start < q_lower.size() && std::isspace(static_cast<unsigned char>(q_lower[start]))) ++start;
            if (start >= q_lower.size()) break;
            usize end = start;
            while (end < q_lower.size() && !std::isspace(static_cast<unsigned char>(q_lower[end]))) ++end;
            tokens.push_back(q_lower.substr(start, end - start));
            start = end;
        }

        if (tokens.empty()) return 1;

        // all tokens must match either as substring or fuzzy subsequence
        int score = 0;
        for (const auto& token : tokens) {
            if (full.find(token) != String::npos) {
                score += 100;
                if (tit_lower.starts_with(token)) score += 300;
                else if (tit_lower.find(token) != String::npos) score += 150;
            } else if (fuzzy_subsequence(token, full)) {
                score += 40;
            } else {
                return 0; // rejected: token not matched
            }
        }

        // exact and prefix bonuses
        if (tit_lower == q_lower) score += 2000;
        else if (tit_lower.starts_with(q_lower)) score += 1000;
        else if (full.starts_with(q_lower)) score += 500;

        return score;
    }

    struct ScoredItem
    {
        const CommandDescriptor* cmd   = nullptr;
        int                      score = 0;
    };

    struct InputCallbackData
    {
        int* selected_index = nullptr;
        int  matched_count  = 0;
    };
} // namespace

void lyra::ui::command::register_command(CommandDescriptor desc)
{
    // replace if ID already exists
    for (auto& cmd : g_commands) {
        if (cmd.id == desc.id) {
            cmd = std::move(desc);
            return;
        }
    }
    g_commands.push_back(std::move(desc));
}

void lyra::ui::command::unregister_command(StringView id)
{
    auto it = std::remove_if(g_commands.begin(), g_commands.end(), [&](const CommandDescriptor& cmd) {
        return cmd.id == id;
    });
    g_commands.erase(it, g_commands.end());
}

bool lyra::ui::command::execute(StringView id)
{
    for (const auto& cmd : g_commands) {
        if (cmd.id == id) {
            if (!cmd.is_enabled || cmd.is_enabled()) {
                if (cmd.action) {
                    cmd.action();
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

void lyra::ui::command::clear()
{
    g_commands.clear();
}

auto lyra::ui::command::get_command(StringView id) -> const CommandDescriptor*
{
    for (const auto& cmd : g_commands) {
        if (cmd.id == id) {
            return &cmd;
        }
    }
    return nullptr;
}

auto lyra::ui::command::get_all_commands() -> const Vector<CommandDescriptor>&
{
    return g_commands;
}

void lyra::ui::command::open_palette()
{
    g_palette_action_frame   = ImGui::GetFrameCount();
    g_palette.is_open        = true;
    g_palette.focus_search   = true;
    g_palette.search_buf[0]  = '\0';
    g_palette.selected_index = 0;
}

void lyra::ui::command::close_palette()
{
    if (ImGui::GetFrameCount() == g_palette_action_frame) {
        return;
    }
    g_palette_action_frame = ImGui::GetFrameCount();
    g_palette.is_open      = false;
}

void lyra::ui::command::toggle_palette()
{
    if (ImGui::GetFrameCount() == g_palette_action_frame) {
        return;
    }
    if (g_palette.is_open) {
        close_palette();
    } else {
        open_palette();
    }
}

bool lyra::ui::command::is_palette_open()
{
    return g_palette.is_open;
}

void lyra::ui::command::dispatch_shortcuts()
{
    // do not trigger shortcuts when typing into text boxes or when palette is open
    if (ui::is_text_input_active() || g_palette.is_open) {
        return;
    }

    bool ctrl  = ui::is_key_down(KeyButton::CTRL);
    bool shift = ui::is_key_down(KeyButton::SHIFT);
    bool alt   = ui::is_key_down(KeyButton::ALT);
    bool super = ui::is_key_down(KeyButton::SUPER);

    for (const auto& cmd : g_commands) {
        if (!cmd.chord.valid()) continue;
        if (cmd.chord.primary.type != DeviceButton::Type::KEY) continue;

        auto key = static_cast<KeyButton>(cmd.chord.primary.code);

        bool req_ctrl  = cmd.chord.modifiers.contains(ModifierKey::CTRL);
        bool req_shift = cmd.chord.modifiers.contains(ModifierKey::SHIFT);
        bool req_alt   = cmd.chord.modifiers.contains(ModifierKey::ALT);
        bool req_super = cmd.chord.modifiers.contains(ModifierKey::SUPER);

        if (ctrl == req_ctrl && shift == req_shift && alt == req_alt && super == req_super && ui::is_key_pressed(key)) {
            if (!cmd.is_enabled || cmd.is_enabled()) {
                if (cmd.action) {
                    cmd.action();
                }
                break;
            }
        }
    }
}

void lyra::ui::command::palette()
{
    if (!g_palette.is_open) {
        return;
    }

    // handle dismiss keys (Escape, or Ctrl+P while open)
    if (ImGui::GetFrameCount() != g_palette_action_frame) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            (ui::is_key_down(KeyButton::CTRL) && !ui::is_key_down(KeyButton::SHIFT) && ui::is_key_pressed(KeyButton::P))) {
            close_palette();
            return;
        }
    }

    ImGuiViewport* viewport      = ImGui::GetMainViewport();
    ImVec2         viewport_pos  = viewport->Pos;
    ImVec2         viewport_size = viewport->Size;
    float          palette_width = std::clamp(viewport_size.x * 0.60f, 520.0f, viewport_size.x - 40.0f);
    float          palette_y     = viewport_pos.y + std::max(80.0f, viewport_size.y * 0.10f);
    ImVec2         modal_pos(viewport_pos.x + (viewport_size.x - palette_width) * 0.5f, palette_y);

    // 1. full-viewport backdrop window to dim background & detect outside clicks
    ImGui::SetNextWindowPos(viewport_pos);
    ImGui::SetNextWindowSize(viewport_size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGuiWindowFlags backdrop_flags = ImGuiWindowFlags_NoTitleBar |
                                      ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoNav;

    ImGui::Begin("##CommandPaletteBackdrop", nullptr, backdrop_flags);
    if (ImGui::IsWindowHovered() && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))) {
        close_palette();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    if (!g_palette.is_open) {
        return;
    }

    // 2. Command Palette floating overlay window
    ImGui::SetNextWindowPos(modal_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(palette_width, 0.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.25f, 0.32f, 0.90f));

    ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##CommandPaletteOverlay", nullptr, overlay_flags)) {
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // top subtle glass highlight line
        draw->AddLine(
            ImVec2(modal_pos.x + 14.0f, modal_pos.y + 1.0f),
            ImVec2(modal_pos.x + palette_width - 14.0f, modal_pos.y + 1.0f),
            IM_COL32(255, 255, 255, 30),
            1.0f);

        // filter commands
        Vector<ScoredItem> scored;
        scored.reserve(g_commands.size());

        StringView q_view(g_palette.search_buf);
        for (const auto& cmd : g_commands) {
            int s = compute_score(cmd, q_view);
            if (s > 0) {
                scored.push_back({&cmd, s});
            }
        }

        std::sort(scored.begin(), scored.end(), [](const ScoredItem& a, const ScoredItem& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.cmd->category != b.cmd->category) return a.cmd->category < b.cmd->category;
            return a.cmd->title < b.cmd->title;
        });

        int matched_count = static_cast<int>(scored.size());
        if (g_palette.selected_index >= matched_count) {
            g_palette.selected_index = std::max(0, matched_count - 1);
        }

        // =====================================================================
        // 1. Sleek Search Header (Borderless with glowing icon & count badge)
        // =====================================================================
        ImVec2 header_pos = ImGui::GetCursorScreenPos();
        float  avail_w    = ImGui::GetContentRegionAvail().x;
        float  header_h   = 44.0f;
        float  font_h     = ImGui::GetFontSize();

        // glowing search icon on the left
        ImVec2 search_icon_pos(header_pos.x + 8.0f, header_pos.y + (header_h - font_h) * 0.5f);
        draw->AddText(search_icon_pos, IM_COL32(56, 189, 248, 255), LYRA_ICON_SEARCH);

        // match count badge on the right
        char count_buf[32];
        if (g_palette.search_buf[0] != '\0') {
            snprintf(count_buf, sizeof(count_buf), "%d results", matched_count);
        } else {
            snprintf(count_buf, sizeof(count_buf), "%zu commands", g_commands.size());
        }
        float count_w = ImGui::CalcTextSize(count_buf).x;
        float count_x = header_pos.x + avail_w - count_w - 8.0f;
        float count_y = header_pos.y + (header_h - font_h) * 0.5f;
        draw->AddText(ImVec2(count_x, count_y), IM_COL32(100, 116, 139, 200), count_buf);

        // borderless search input text field
        float input_x = 36.0f;
        float input_w = avail_w - input_x - (count_w + 20.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + input_x);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (header_h - (font_h + 16.0f)) * 0.5f);
        ImGui::SetNextItemWidth(input_w);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 8.0f));

        if (g_palette.focus_search) {
            ImGui::SetKeyboardFocusHere();
            g_palette.focus_search = false;
        }

        InputCallbackData cb_data{&g_palette.selected_index, matched_count};
        auto input_cb = [](ImGuiInputTextCallbackData* data) -> int {
            auto* cb = static_cast<InputCallbackData*>(data->UserData);
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (*cb->selected_index > 0) {
                    (*cb->selected_index)--;
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (*cb->selected_index + 1 < cb->matched_count) {
                    (*cb->selected_index)++;
                }
            }
            return 0;
        };

        bool enter_pressed = ImGui::InputTextWithHint(
            "##PaletteSearch",
            "Type a command or search...",
            g_palette.search_buf,
            sizeof(g_palette.search_buf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
            input_cb,
            &cb_data);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        // handle execute
        const CommandDescriptor* cmd_to_execute = nullptr;
        auto trigger_execute = [&](int idx) {
            if (idx >= 0 && idx < matched_count) {
                const auto* item = scored[idx].cmd;
                if (!item->is_enabled || item->is_enabled()) {
                    cmd_to_execute = item;
                }
            }
        };

        if (enter_pressed || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            trigger_execute(g_palette.selected_index);
        }

        // cleanly advance cursor past the entire header
        ImGui::SetCursorScreenPos(ImVec2(header_pos.x, header_pos.y + header_h));

        // divider line separating header from list
        draw->AddLine(
            ImVec2(header_pos.x - 16.0f, header_pos.y + header_h),
            ImVec2(header_pos.x + avail_w + 16.0f, header_pos.y + header_h),
            IM_COL32(35, 40, 52, 230),
            1.0f);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);

        // =====================================================================
        // 2. Results List with Custom Row Tiles, Keycaps & Glow
        // =====================================================================
        const float item_height = 42.0f;
        const int   max_visible = 8;
        const float list_height = (matched_count > 0)
                                      ? std::min(matched_count, max_visible) * (item_height + 4.0f) + 4.0f
                                      : 64.0f;

        static int s_prev_selected_index = -1;
        bool need_scroll_to_selected = (g_palette.selected_index != s_prev_selected_index);
        if (need_scroll_to_selected) {
            s_prev_selected_index = g_palette.selected_index;
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (ImGui::BeginChild("##PaletteList", ImVec2(0.0f, list_height), false, ImGuiWindowFlags_None)) {
            ImDrawList* list_draw = ImGui::GetWindowDrawList();

            if (matched_count == 0) {
                ImGui::Dummy(ImVec2(0.0f, 16.0f));
                float text_w = ImGui::CalcTextSize(LYRA_ICON_SEARCH "  No matching commands found").x;
                ImGui::SetCursorPosX((avail_w - text_w) * 0.5f);
                ImGui::TextColored(ImVec4(0.40f, 0.45f, 0.55f, 1.0f), "%s  No matching commands found", LYRA_ICON_SEARCH);
            } else {
                for (int i = 0; i < matched_count; ++i) {
                    const auto* cmd            = scored[i].cmd;
                    bool        is_selected    = (g_palette.selected_index == i);
                    bool        is_cmd_enabled = (!cmd->is_enabled || cmd->is_enabled());
                    auto        cat_style      = get_category_style(cmd->category);

                    ImGui::PushID(cmd->id.c_str());

                    ImVec2 cur_pos   = ImGui::GetCursorScreenPos();
                    float  row_avail = ImGui::GetContentRegionAvail().x;

                    // 1. highlight background
                    if (is_selected) {
                        // tinted highlight pill
                        ImU32 fill_col = is_cmd_enabled ? IM_COL32(30, 58, 110, 200) : IM_COL32(26, 30, 40, 160);
                        list_draw->AddRectFilled(cur_pos, ImVec2(cur_pos.x + row_avail, cur_pos.y + item_height), fill_col, 8.0f);

                        // outer border highlight
                        ImU32 border_col = is_cmd_enabled ? IM_COL32(59, 130, 246, 200) : IM_COL32(45, 52, 68, 140);
                        list_draw->AddRect(cur_pos, ImVec2(cur_pos.x + row_avail, cur_pos.y + item_height), border_col, 8.0f);

                        // electric blue left accent indicator bar
                        if (is_cmd_enabled) {
                            list_draw->AddRectFilled(
                                ImVec2(cur_pos.x + 2.0f, cur_pos.y + 8.0f),
                                ImVec2(cur_pos.x + 5.5f, cur_pos.y + item_height - 8.0f),
                                IM_COL32(56, 189, 248, 255),
                                2.0f);
                        }
                    }

                    // invisible selectable for click & hover detection
                    if (ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(row_avail, item_height))) {
                        trigger_execute(i);
                    }

                    if (ImGui::IsItemHovered()) {
                        g_palette.selected_index = i;
                        s_prev_selected_index    = i; // prevent auto-scrolling on hover
                    }

                    if (is_selected && need_scroll_to_selected) {
                        ImGui::SetScrollHereY(0.5f);
                        need_scroll_to_selected = false;
                    }

                    // 2. icon tile (category-colored rounded square)
                    const float tile_size = 28.0f;
                    const float tile_x    = cur_pos.x + 12.0f;
                    const float tile_y    = cur_pos.y + (item_height - tile_size) * 0.5f;

                    list_draw->AddRectFilled(ImVec2(tile_x, tile_y), ImVec2(tile_x + tile_size, tile_y + tile_size), cat_style.icon_bg, 6.0f);
                    list_draw->AddRect(ImVec2(tile_x, tile_y), ImVec2(tile_x + tile_size, tile_y + tile_size), cat_style.badge_border, 6.0f);

                    CString icon_str = cmd->icon ? cmd->icon : LYRA_ICON_SETTINGS;
                    ImVec2  icon_sz  = ImGui::CalcTextSize(icon_str);
                    list_draw->AddText(
                        ImVec2(tile_x + (tile_size - icon_sz.x) * 0.5f, tile_y + (tile_size - icon_sz.y) * 0.5f),
                        is_cmd_enabled ? cat_style.icon_color : IM_COL32(100, 110, 125, 180),
                        icon_str);

                    // 3. category pill badge (with exact mathematical horizontal and vertical centering)
                    ImVec2      cat_text_sz = ImGui::CalcTextSize(cmd->category.c_str());
                    const float badge_pad_x = 8.0f;
                    float       badge_w     = cat_text_sz.x + badge_pad_x * 2.0f;
                    const float badge_h     = 22.0f;
                    float       badge_x     = tile_x + tile_size + 10.0f;
                    float       badge_y     = cur_pos.y + (item_height - badge_h) * 0.5f;

                    list_draw->AddRectFilled(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), cat_style.badge_bg, 4.0f);
                    list_draw->AddRect(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), cat_style.badge_border, 4.0f);
                    list_draw->AddText(
                        ImVec2(badge_x + (badge_w - cat_text_sz.x) * 0.5f, badge_y + (badge_h - cat_text_sz.y) * 0.5f),
                        is_cmd_enabled ? cat_style.badge_text : IM_COL32(100, 110, 125, 180),
                        cmd->category.c_str());

                    // 4. command title (crisp white when highlighted, fully legible)
                    float title_x = badge_x + badge_w + 12.0f;
                    float title_y = cur_pos.y + (item_height - font_h) * 0.5f;
                    ImU32 title_col = is_cmd_enabled
                                          ? (is_selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(226, 232, 240, 245))
                                          : IM_COL32(100, 116, 139, 180);
                    list_draw->AddText(ImVec2(title_x, title_y), title_col, cmd->title.c_str());

                    // 5. right-aligned tactile keycaps or disabled badge
                    if (!is_cmd_enabled) {
                        CString dis_text = "Disabled";
                        ImVec2  dis_sz   = ImGui::CalcTextSize(dis_text);
                        float   dis_x    = cur_pos.x + row_avail - dis_sz.x - 14.0f;
                        float   dis_y    = cur_pos.y + (item_height - font_h) * 0.5f;
                        list_draw->AddText(ImVec2(dis_x, dis_y), IM_COL32(100, 116, 139, 180), dis_text);
                    } else if (!cmd->shortcut.empty()) {
                        auto  keys    = split_shortcut(cmd->shortcut);
                        float right_x = cur_pos.x + row_avail - 12.0f;

                        for (int k = static_cast<int>(keys.size()) - 1; k >= 0; --k) {
                            const auto& key_str = keys[k];
                            ImVec2 key_sz = ImGui::CalcTextSize(key_str.c_str());
                            float cap_w  = std::max(key_sz.x + 12.0f, 22.0f);
                            float cap_h  = 20.0f;
                            float cap_x  = right_x - cap_w;
                            float cap_y  = cur_pos.y + (item_height - cap_h) * 0.5f;
                            float txt_y  = cap_y + (cap_h - font_h) * 0.5f;

                            // keycap shadow
                            list_draw->AddRectFilled(ImVec2(cap_x, cap_y + 1.0f), ImVec2(cap_x + cap_w, cap_y + cap_h + 1.0f), IM_COL32(0, 0, 0, 120), 4.0f);
                            // keycap body
                            ImU32 cap_bg = is_selected ? IM_COL32(24, 32, 48, 245) : IM_COL32(18, 22, 30, 230);
                            list_draw->AddRectFilled(ImVec2(cap_x, cap_y), ImVec2(cap_x + cap_w, cap_y + cap_h), cap_bg, 4.0f);
                            // keycap border
                            ImU32 cap_border = is_selected ? IM_COL32(65, 85, 125, 220) : IM_COL32(40, 48, 64, 180);
                            list_draw->AddRect(ImVec2(cap_x, cap_y), ImVec2(cap_x + cap_w, cap_y + cap_h), cap_border, 4.0f);
                            // keycap top specular highlight line
                            list_draw->AddLine(ImVec2(cap_x + 2.0f, cap_y + 1.0f), ImVec2(cap_x + cap_w - 2.0f, cap_y + 1.0f), IM_COL32(255, 255, 255, 25), 1.0f);
                            // keycap legend text
                            ImU32 cap_text = is_selected ? IM_COL32(220, 235, 255, 255) : IM_COL32(148, 163, 184, 220);
                            list_draw->AddText(ImVec2(cap_x + (cap_w - key_sz.x) * 0.5f, txt_y), cap_text, key_str.c_str());

                            right_x = cap_x - 4.0f;
                        }
                    }

                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(4);

        // =====================================================================
        // 3. Status Bar Footer (Breadcrumbs & Key Legends)
        // =====================================================================
        const float footer_height = 36.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
        ImVec2 footer_pos = ImGui::GetCursorScreenPos();
        draw->AddLine(
            ImVec2(footer_pos.x - 16.0f, footer_pos.y),
            ImVec2(footer_pos.x + avail_w + 16.0f, footer_pos.y),
            IM_COL32(35, 40, 52, 230),
            1.0f);

        float footer_center_y = footer_pos.y + (footer_height - font_h) * 0.5f;

        // active item breadcrumb on the left
        if (g_palette.selected_index >= 0 && g_palette.selected_index < matched_count) {
            const auto* sel_cmd = scored[g_palette.selected_index].cmd;
            char breadcrumb_buf[128];
            snprintf(breadcrumb_buf, sizeof(breadcrumb_buf), "%s  \xe2\x80\xba  %s", sel_cmd->category.c_str(), sel_cmd->title.c_str());
            draw->AddText(ImVec2(footer_pos.x + 8.0f, footer_center_y), IM_COL32(148, 163, 184, 220), breadcrumb_buf);
        }

        // keyboard navigation hints on the right
        auto draw_hint_pill = [&](float& cur_r_x, CString key_txt, CString label_txt) {
            ImVec2 lbl_sz = ImGui::CalcTextSize(label_txt);
            ImVec2 k_sz   = ImGui::CalcTextSize(key_txt);
            float  cap_w  = k_sz.x + 8.0f;
            float  cap_h  = k_sz.y + 4.0f;
            float  total_w = cap_w + 6.0f + lbl_sz.x;
            float  start_x = cur_r_x - total_w;
            float  cap_y   = footer_pos.y + (footer_height - cap_h) * 0.5f;
            float  txt_y   = footer_pos.y + (footer_height - font_h) * 0.5f;

            // keycap
            draw->AddRectFilled(ImVec2(start_x, cap_y), ImVec2(start_x + cap_w, cap_y + cap_h), IM_COL32(24, 28, 38, 240), 3.0f);
            draw->AddRect(ImVec2(start_x, cap_y), ImVec2(start_x + cap_w, cap_y + cap_h), IM_COL32(50, 58, 76, 200), 3.0f);
            draw->AddText(ImVec2(start_x + 4.0f, txt_y), IM_COL32(180, 195, 215, 240), key_txt);

            // label
            draw->AddText(ImVec2(start_x + cap_w + 6.0f, txt_y), IM_COL32(100, 116, 139, 220), label_txt);

            cur_r_x = start_x - 14.0f;
        };

        float hint_right_x = footer_pos.x + avail_w - 6.0f;
        draw_hint_pill(hint_right_x, "Esc", "Close");
        draw_hint_pill(hint_right_x, "\xe2\x86\x91\xe2\x86\x93", "Navigate");
        draw_hint_pill(hint_right_x, "\xe2\x86\xb5", "Select");

        ImGui::Dummy(ImVec2(0.0f, footer_height));

        if (cmd_to_execute) {
            close_palette();
            if (cmd_to_execute->action) {
                cmd_to_execute->action();
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}
