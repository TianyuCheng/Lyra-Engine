#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>

// local imports
#include "Icons.h"
#include "Colors.h"
#include "Layout.h"
#include "LoggerView.h"

#define LYRA_CONSOLE_WINDOW_NAME (LYRA_ICON_CONSOLE " Console")

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
    // toggle buttons for each log level
    auto level_button = [&](const char* label, LogLevel level, ImVec4 color) {
        bool active = (level_filter & (1 << (int)level));
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, color.w));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, color.w));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ChildBg]);
        }

        if (ImGui::Button(label)) {
            level_filter ^= (1 << (int)level);
        }

        if (active) ImGui::PopStyleColor(3);
        else ImGui::PopStyleColor(2);
    };

    level_button("TRACE", LogLevel::trace, LYRA_COLOR_TRACE);
    ImGui::SameLine();
    level_button("DEBUG", LogLevel::debug, LYRA_COLOR_DEBUG);
    ImGui::SameLine();
    level_button("INFO", LogLevel::info, LYRA_COLOR_INFO);
    ImGui::SameLine();
    level_button("WARN", LogLevel::warn, LYRA_COLOR_WARN);
    ImGui::SameLine();
    level_button("ERROR", LogLevel::err, LYRA_COLOR_ERROR);
    ImGui::SameLine();
    level_button("CRIT", LogLevel::critical, LYRA_COLOR_CRITICAL);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // log filter
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##LogFilter", LYRA_ICON_FILTER " Filter...", filter, 1024);
    ImGui::PopItemWidth();
}

void LoggerView::show_logs() const
{
    String filter_text(filter);

    // change log color
    auto set_text_color = [&](LogLevel level) {
        ImGuiStyle& style  = ImGui::GetStyle();
        ImVec4*     colors = style.Colors;
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

    // record original text color
    ImVec4 original_color = ImGui::GetStyle().Colors[ImGuiCol_Text];

    // show filtered console logs
    ImGui::BeginChild("Logs", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        auto& sink = get_console_sink()->get_console();
        sink.for_each([&](const ConsoleLog& log) {
            if (level_filter & (1 << (int)log.verbosity)) {
                if (filter_text.empty() || log.payload.find(filter_text) != String::npos) {
                    if (!log.payload.empty()) {
                        set_text_color(log.verbosity);
                        ImGui::TextUnformatted(log.payload.c_str());
                    }
                }
            }
        });
        if (sink.modified()) {
            sink.reset();
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    // restore color
    ImGui::GetStyle().Colors[ImGuiCol_Text] = original_color;
}
