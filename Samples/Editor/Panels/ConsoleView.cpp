#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>
#include <Lyra/UICore/UI.h>
#include <Lyra/UICore/UILayout.h>
#include <Lyra/UICore/UIControls.h>
#include <Lyra/UICore/UIDock.h>

// local imports
#include <Lyra/UICore/UIIcons.h>
#include "Common/EditorLayout.h"
#include "ConsoleView.h"

#define LYRA_CONSOLE_WINDOW_NAME (LYRA_ICON_CONSOLE " Console")

using namespace lyra;

ConsoleView::ConsoleView(size_t capacity)
{
    get_console_sink()->get_console().resize(capacity);

    filter[0] = '\0';
}

void ConsoleView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &ConsoleView::update>(*this);
}

void ConsoleView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        ui::workspace::dock(LYRA_CONSOLE_WINDOW_NAME, ui::Area::Bottom);
    });

    ui::panel(LYRA_CONSOLE_WINDOW_NAME, [&]() {
        show_bar();
        show_logs();
    });
}

void ConsoleView::show_bar()
{
    ui::toolbar([&]() {
        // clear button
        ui::button(LYRA_ICON_DELETE " Clear", [&]() {
            get_console_sink()->get_console().clear();
        });

        // auto-scroll toggle
        ui::toggle_button(LYRA_ICON_REFRESH " Auto-scroll", auto_scroll, [&](bool val) {
            auto_scroll = val;
        });

        ui::separator();

        // toggle buttons for each log level
        auto level_button = [&](CString label, LogLevel level, ui::StatusRole role) {
            bool active = (level_filter & (1 << (int)level)) != 0;
            ui::toggle_button(label, active, role, [&](bool) {
                level_filter ^= (1 << (int)level);
            }, Vector2{26.0f, 0.0f});
        };

        level_button("T", LogLevel::trace, ui::StatusRole::Muted);
        level_button("D", LogLevel::debug, ui::StatusRole::Info);
        level_button("I", LogLevel::info, ui::StatusRole::Success);
        level_button("W", LogLevel::warn, ui::StatusRole::Warning);
        level_button("E", LogLevel::err, ui::StatusRole::Error);
        level_button("C", LogLevel::critical, ui::StatusRole::Critical);

        ui::separator();

        // right-aligned fixed-size search filter
        ui::spacer();
        ui::search_bar(filter, sizeof(filter), 250.0f);
    });
}

void ConsoleView::show_logs() const
{
    String filter_text(filter);

    auto get_status_role = [](LogLevel level) -> ui::StatusRole {
        switch (level) {
            case LogLevel::trace:    return ui::StatusRole::Muted;
            case LogLevel::debug:    return ui::StatusRole::Info;
            case LogLevel::info:     return ui::StatusRole::Success;
            case LogLevel::warn:     return ui::StatusRole::Warning;
            case LogLevel::err:      return ui::StatusRole::Error;
            case LogLevel::critical: return ui::StatusRole::Critical;
            default:                 return ui::StatusRole::Muted;
        }
    };

    ui::scroll_area("Logs", [&]() {
        auto& sink = get_console_sink()->get_console();
        sink.for_each([&](const ConsoleLog& log) {
            if (level_filter & (1 << (int)log.verbosity)) {
                if (filter_text.empty() || log.payload.find(filter_text) != String::npos) {
                    if (!log.payload.empty()) {
                        ui::label(log.payload.c_str(), get_status_role(log.verbosity));
                    }
                }
            }
        });
        if (sink.modified()) {
            sink.reset();
            if (auto_scroll) {
                ui::scroll_to_bottom();
            }
        }
    });
}
