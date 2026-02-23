#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>

// local imports
#include "Icons.h"
#include "Colors.h"
#include "Layout.h"
#include "LoggerView.h"

#define LYRA_CONSOLE_WINDOW_NAME (LYRA_ICON_CONSOLE "Console")

using namespace lyra;

LoggerView::LoggerView(size_t capacity)
{
    get_console_sink()->get_console().resize(capacity);

    filter[0] = '\0';
}

void LoggerView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &LoggerView::update>(*this);
}

void LoggerView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_CONSOLE_WINDOW_NAME, layout.bottom);
    });

    ImGui::Begin(LYRA_CONSOLE_WINDOW_NAME);
    {
        show_bar();
        show_logs();
    }
    ImGui::End();
}

void LoggerView::show_bar()
{
    // log verbosity
    ImGui::PushItemWidth(200);
    CString log_levels[]  = {"trace", "debug", "info", "warn", "error", "critical", "off"};
    CString preview_value = log_levels[log_level];

    // just to apply some padding
    ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, LYRA_COLOR_DISABLED);
    ImGui::Button(LYRA_ICON_FILTER);
    ImGui::PopStyleColor();
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::BeginCombo("##LogLevel", preview_value)) {
        for (int i = 0; i < IM_ARRAYSIZE(log_levels); i++) {
            const bool is_log_level = (log_level == i);
            if (ImGui::Selectable(log_levels[i], is_log_level)) {
                log_level = i;
            }
            if (is_log_level) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // log filter
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##LogFilter", filter, 1024, 0);
    ImGui::PopItemWidth();
}

void LoggerView::show_logs() const
{
    String filter_text(filter);

    // record the original text color
    ImGuiStyle& style      = ImGui::GetStyle();
    ImVec4*     colors     = style.Colors;
    ImVec4      text_color = colors[ImGuiCol_Text];

    // change log color
    auto set_text_color = [&](LogLevel level) {
        // clang-format off
        switch (level) {
            case LogLevel::trace:    colors[ImGuiCol_Text] = LYRA_COLOR_TRACE;    break;
            case LogLevel::debug:    colors[ImGuiCol_Text] = LYRA_COLOR_DEBUG;    break;
            case LogLevel::info:     colors[ImGuiCol_Text] = LYRA_COLOR_INFO;     break;
            case LogLevel::warn:     colors[ImGuiCol_Text] = LYRA_COLOR_WARN;     break;
            case LogLevel::err:      colors[ImGuiCol_Text] = LYRA_COLOR_ERROR;    break;
            case LogLevel::critical: colors[ImGuiCol_Text] = LYRA_COLOR_CRITICAL; break;
            default:                 colors[ImGuiCol_Text] = LYRA_COLOR_DISABLED; break;
        }
        // clang-format on
    };

    // show filtered console logs
    ImGui::BeginChild("Logs", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        auto& sink = get_console_sink()->get_console();
        sink.for_each([&](const ConsoleLog& log) {
            if (log_level <= static_cast<int>(log.verbosity))
                if (filter_text.empty() || log.payload.find(filter_text) != String::npos) {
                    if (!log.payload.empty()) {
                        set_text_color(log.verbosity);
                        ImGui::Text("%s", log.payload.c_str());
                    }
                }
        });
        if (sink.modified()) {
            sink.reset();
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    // restore the original text color
    colors[ImGuiCol_Text] = text_color;
}
