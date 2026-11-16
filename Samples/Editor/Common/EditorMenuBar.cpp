#include "EditorMenuBar.h"
#include <Lyra/Lyra.hpp>

using namespace lyra;

void lyra::render_editor_menubar(AppContext& context)
{
    ui::menubar([&]() {
        ui::menu("Scene", [&]() {
            ui::menu_item("New Scene", "Ctrl+N", [&]() {
                ui::command::execute("scene.new");
            });

            ui::menu_item("Open Scene...", "Ctrl+O", [&]() {
                ui::command::execute("scene.open");
            });

            ui::menu_item("Save Scene", "Ctrl+S", [&]() {
                ui::command::execute("scene.save");
            });

            ui::menu_item("Save Scene As...", "Ctrl+Shift+S", [&]() {
                ui::command::execute("scene.save_as");
            });

            ui::separator();

            ui::menu_item("Load Additive...", [&]() {
                ui::command::execute("scene.load_additive");
            });
        });

        if (auto ams = context.services.try_get<AssetServer>()) {
            ui::menu("Assets", [&]() {
                ui::menu_item("Reimport All (Force)", [&]() {
                    ui::command::execute("assets.reimport_all");
                });
                ui::menu_item("Purge Unused Assets", [&]() {
                    ui::command::execute("assets.purge");
                });
                ui::menu_item("Flush Registry to Disk", [&]() {
                    ui::command::execute("assets.flush");
                });
                ui::separator();
                ui::menu_check_item("Watch Source Directory", ams->is_watching(), [&](const bool& watching) {
                    ui::command::execute("assets.toggle_watching");
                });
            });
        }

        ui::menu("Window", [&]() {
            ui::menu_item("Command Palette", "Ctrl+P", [&]() {
                ui::command::toggle_palette();
            });
            ui::separator();
            ui::menu_item("Reset Layout", [&]() {
                ui::command::execute("window.reset_layout");
            });
        });
    });
}
