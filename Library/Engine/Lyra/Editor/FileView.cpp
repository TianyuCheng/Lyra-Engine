#include <algorithm>
#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Assets/AMSServer.h>

// local imports
#include "Icons.h"
#include "Layout.h"
#include "FileView.h"

#define LYRA_FILES_WINDOW_NAME (LYRA_ICON_FOLDER " Files")

using namespace lyra;

FileView::FileView(const Path& root)
    : root(root), curr(root)
{
    // sanity check
    assert(std::filesystem::exists(root));
    assert(std::filesystem::is_directory(root));
}

void FileView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &FileView::update>(*this);

    // initial data
    update_directory(root, true);
}

void FileView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_FILES_WINDOW_NAME, layout.bottom);
    });

    ImGui::Begin(LYRA_FILES_WINDOW_NAME);
    {
        handle_file_drop(blackboard);

        show_breadcrumb();
        ImGui::Separator();
        ImGui::BeginChild("##FileBrowser");
        {
            show_dir_files(blackboard);
            show_context_menu(blackboard);
            show_new_file_dialog();
            show_new_folder_dialog();
            show_rename_dialog();
            show_delete_dialog(blackboard);
            show_import_indicator();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void FileView::handle_file_drop(Blackboard& blackboard)
{
    auto window = blackboard.get<Window*>();
    auto ams    = blackboard.get<AssetServer*>();

    // check if mouse is over the file view window
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup))
        return;

    if (!window->get_input_state().has_dropped_files())
        return;

    for (const auto& path_str : window->get_input_state().get_dropped_files()) {
        Path src(path_str);
        Path dst = curr / src.filename();
        try {
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
            auto rel_path = std::filesystem::relative(dst, root);
            auto future   = ams->import_asset(rel_path);
            if (future.valid()) {
                active_imports.push_back(std::move(future));
                spdlog::info("Importing dropped asset: {} -> {}", path_str, rel_path.string());
            } else {
                spdlog::error("Failed to start import for asset: {}", rel_path.string());
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to copy/import dropped file {}: {}", path_str, e.what());
        }
    }

    // refresh directory view
    update_directory(curr, true);
}

void FileView::show_import_indicator()
{
    // cleanup finished imports
    for (auto it = active_imports.begin(); it != active_imports.end();) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            if (it->get() != 0) {
                finished_success++;
            } else {
                finished_failure++;
            }
            it                 = active_imports.erase(it);
            notification_timer = 5.0f; // show for 5 seconds
        } else {
            ++it;
        }
    }

    if (active_imports.empty() && notification_timer <= 0.0f) return;

    // update timer
    if (active_imports.empty() && notification_timer > 0.0f) {
        notification_timer -= ImGui::GetIO().DeltaTime;
    }

    // show indicator in the corner of the window
    ImVec2 region = ImGui::GetWindowContentRegionMax();
    ImGui::SetCursorPos(ImVec2(region.x - 250, region.y - 50));
    ImGui::BeginChild("##ImportIndicator", ImVec2(250, 50), true, ImGuiWindowFlags_NoScrollbar);
    {
        if (!active_imports.empty()) {
            ImGui::Text(LYRA_ICON_IMPORT " Importing %zu assets...", active_imports.size());
        } else {
            ImGui::TextColored(finished_failure > 0 ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.4f, 1, 0.4f, 1),
                "Import finished: %u ok, %u failed", finished_success, finished_failure);
            if (ImGui::IsWindowHovered()) notification_timer = 0.0f; // dismiss on hover
        }
    }
    ImGui::EndChild();

    // reset counters when notification is gone
    if (active_imports.empty() && notification_timer <= 0.0f) {
        finished_success = 0;
        finished_failure = 0;
    }
}

void FileView::show_breadcrumb()
{
    // root / home
    if (ImGui::Button(LYRA_ICON_HOME " Home"))
        update_directory(root);

    ImGui::SameLine();
    ImGui::TextUnformatted("/");
    ImGui::SameLine();

    // currently at root
    if (root == curr) {
        ImGui::NewLine();
        return;
    }

    // show directory path segments
    for (const auto& breadcrumb : breadcrumbs) {
        if (ImGui::Button(breadcrumb.name.c_str())) {
            update_directory(breadcrumb.path);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

void FileView::show_dir_files(Blackboard& blackboard)
{
    // drawing grid
    int   grid_id   = 0;
    float start_x   = ImGui::GetCursorPosX();
    float row_width = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    auto ams = blackboard.get<AssetServer*>();

    // clear selection if we click on empty area
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        selected_items.clear();
        last_selected = "";
    }

    auto is_selected = [&](const String& item) {
        return std::find(selected_items.begin(), selected_items.end(), item) != selected_items.end();
    };

    auto toggle_selection = [&](const String& item) {
        auto it = std::find(selected_items.begin(), selected_items.end(), item);
        if (it != selected_items.end()) {
            selected_items.erase(it);
        } else {
            selected_items.push_back(item);
        }
        last_selected = item;
    };

    auto select_only = [&](const String& item) {
        selected_items.clear();
        selected_items.push_back(item);
        last_selected = item;
    };

    // helper for range selection
    auto select_range = [&](const String& item) {
        if (last_selected.empty()) {
            select_only(item);
            return;
        }

        Vector<String> all_items;
        all_items.insert(all_items.end(), folders.begin(), folders.end());
        all_items.insert(all_items.end(), files.begin(), files.end());

        int start_idx = -1;
        int end_idx   = -1;

        for (int i = 0; i < (int)all_items.size(); ++i) {
            if (all_items[i] == last_selected) start_idx = i;
            if (all_items[i] == item) end_idx = i;
        }

        if (start_idx != -1 && end_idx != -1) {
            int from = std::min(start_idx, end_idx);
            int to   = std::max(start_idx, end_idx);
            for (int i = from; i <= to; ++i) {
                if (!is_selected(all_items[i])) {
                    selected_items.push_back(all_items[i]);
                }
            }
        }
    };

    // folders
    for (const auto& folder : folders) {
        ImGui::PushID(grid_id++);
        {
            // folder icon
            draw_icon_grid(LYRA_ICON_FOLDER, folder.c_str(), icon_scale, is_selected(folder));

            // handle clicks
            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    update_directory(curr / folder);
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) {
                        toggle_selection(folder);
                    } else if (ImGui::GetIO().KeyShift) {
                        select_range(folder);
                    } else {
                        select_only(folder);
                    }
                }
            }

            if (ImGui::BeginPopupContextItem("FolderItemContextMenu")) {
                context_selected_folder = folder;
                context_selected_file   = "";
                if (!is_selected(folder)) {
                    select_only(folder);
                }

                if (selected_items.size() == 1) {
                    if (ImGui::MenuItem(LYRA_ICON_RENAME " Rename")) {
                        show_rename_modal = true;
                        strncpy(rename_buffer, folder.c_str(), sizeof(rename_buffer));
                    }
                }

                if (ImGui::MenuItem(LYRA_ICON_DELETE " Delete")) {
                    show_delete_modal = true;
                }
                ImGui::EndPopup();
            }

            next_icon_grid(row_width, start_x);
        }
        ImGui::PopID();
    }

    // files
    for (const auto& file : files) {
        ImGui::PushID(grid_id++);
        {
            // file icon
            draw_icon_grid(LYRA_ICON_FILE, file.c_str(), icon_scale, is_selected(file));

            // handle clicks
            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    // double click file: no action for now, but placeholder
                    spdlog::debug("Double clicked file: {}", file);
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) {
                        toggle_selection(file);
                    } else if (ImGui::GetIO().KeyShift) {
                        select_range(file);
                    } else {
                        select_only(file);
                    }
                }
            }

            if (ImGui::BeginPopupContextItem("FileItemContextMenu")) {
                context_selected_file   = file;
                context_selected_folder = "";
                if (!is_selected(file)) {
                    select_only(file);
                }

                if (selected_items.size() == 1) {
                    if (ImGui::MenuItem(LYRA_ICON_RENAME " Rename")) {
                        show_rename_modal = true;
                        strncpy(rename_buffer, file.c_str(), sizeof(rename_buffer));
                    }
                }

                if (ImGui::MenuItem(LYRA_ICON_IMPORT " Re-import")) {
                    for (const auto& selected : selected_items) {
                        // check if it's a file (exists in files vector)
                        if (std::find(files.begin(), files.end(), selected) != files.end()) {
                            auto rel_path = std::filesystem::relative(curr / selected, root);
                            auto future   = ams->import_asset(rel_path);
                            if (future.valid()) {
                                active_imports.push_back(std::move(future));
                            }
                        }
                    }
                }

                if (ImGui::MenuItem(LYRA_ICON_DELETE " Delete")) {
                    show_delete_modal = true;
                }
                ImGui::EndPopup();
            }

            next_icon_grid(row_width, start_x);
        }
        ImGui::PopID();
    }

    ImGui::NewLine();
}

void FileView::show_context_menu(Blackboard& blackboard)
{
    // detect context menu (right click) on window background
    if (ImGui::BeginPopupContextWindow("File Manager Context Menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem(LYRA_ICON_REFRESH " Refresh")) {
            update_directory(curr, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(LYRA_ICON_IMPORT " Import")) {
            spdlog::info("Importing assets... (dialog not implemented)");
        }
        ImGui::Separator();
        if (ImGui::BeginMenu(LYRA_ICON_NEW_FILE " Create")) {
            if (ImGui::MenuItem(LYRA_ICON_NEW_FOLDER " Create Folder")) {
                show_new_folder_modal = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    // must be called outside of context menu
    if (show_new_folder_modal) {
        ImGui::OpenPopup("New Folder");
    }

    // must be called outside of context menu
    if (show_delete_modal) {
        ImGui::OpenPopup("Delete");
    }

    // must be called outside of context menu
    if (show_rename_modal) {
        ImGui::OpenPopup("Rename");
    }
}

void FileView::show_new_file_dialog()
{
    if (ImGui::BeginPopupModal("New File", &show_new_file_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("This is a modal dialog (file)!");
        ImGui::Separator();

        if (ImGui::Button("Close")) {
            show_new_file_modal = false;
            ImGui::CloseCurrentPopup(); // Close the current popup
        }

        ImGui::EndPopup();
    }
}

void FileView::show_new_folder_dialog()
{
    if (ImGui::BeginPopupModal("New Folder", &show_new_folder_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        // focus input on first frame
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
            memset(new_folder_name, 0, sizeof(new_folder_name));
        }

        ImGui::Text("Enter folder name:");
        if (ImGui::InputText("##FolderName", new_folder_name, sizeof(new_folder_name), ImGuiInputTextFlags_EnterReturnsTrue)) {
            // handle enter key same as create button
            goto do_create_folder;
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
        do_create_folder:
            if (strlen(new_folder_name) > 0) {
                Path new_path = curr / new_folder_name;
                try {
                    if (std::filesystem::exists(new_path)) {
                        spdlog::error("Folder already exists: {}", new_path.string());
                    } else if (std::filesystem::create_directory(new_path)) {
                        spdlog::info("Created folder: {}", new_path.string());
                        update_directory(curr, true);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Failed to create folder {}: {}", new_path.string(), e.what());
                }
            }
            show_new_folder_modal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_new_folder_modal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FileView::show_rename_dialog()
{
    if (ImGui::BeginPopupModal("Rename", &show_rename_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        // focus input on first frame
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::Text("Enter new name:");
        if (ImGui::InputText("##RenameBuffer", rename_buffer, sizeof(rename_buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            // handle enter key same as Rename button
            goto do_rename;
        }

        ImGui::Separator();

        if (ImGui::Button("Rename", ImVec2(120, 0))) {
        do_rename:
            if (strlen(rename_buffer) > 0) {
                String old_name = context_selected_file.empty() ? context_selected_folder : context_selected_file;
                Path   old_path = curr / old_name;
                Path   new_path = curr / rename_buffer;

                try {
                    if (old_name != rename_buffer) {
                        if (std::filesystem::exists(new_path)) {
                            spdlog::error("Rename failed: Destination already exists: {}", new_path.string());
                        } else {
                            std::filesystem::rename(old_path, new_path);
                            spdlog::info("Renamed: {} -> {}", old_path.string(), new_path.string());
                            update_directory(curr, true);
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Failed to rename {}: {}", old_path.string(), e.what());
                }
            }
            show_rename_modal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_rename_modal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FileView::show_delete_dialog(Blackboard& blackboard)
{
    if (ImGui::BeginPopupModal("Delete", &show_delete_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (selected_items.size() == 1) {
            String target = selected_items[0];
            ImGui::Text("Are you sure you want to delete:");

            float text_width = ImGui::CalcTextSize(target.c_str()).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - text_width) * 0.5f);
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "'%s'", target.c_str());
        } else {
            ImGui::Text("Are you sure you want to delete %zu selected items?", selected_items.size());
        }

        ImGui::Separator();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            for (const auto& target : selected_items) {
                Path target_path = curr / target;
                try {
                    if (std::filesystem::exists(target_path)) {
                        std::filesystem::remove_all(target_path);
                        spdlog::info("Deleted: {}", target_path.string());
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Failed to delete {}: {}", target_path.string(), e.what());
                }
            }
            update_directory(curr, true);
            show_delete_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_delete_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FileView::next_icon_grid(float row_width, float start_x)
{
    // calculate next position
    float last_x = ImGui::GetItemRectMax().x;
    if (last_x + padding + icon_size < row_width) {
        ImGui::SameLine(0.0f, padding);
    } else {
        ImGui::NewLine();
        ImGui::SetCursorPosX(start_x); // align new row
    }
}

void FileView::draw_icon_grid(CString icon, CString text, float icon_scale, bool selected) const
{
    const ImVec2 pos = ImGui::GetCursorPos();
    ImGui::BeginGroup();
    {
        // background button for interaction - hide by default, but show if selected
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Header]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered]);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        }

        ImGui::Button("##bg", ImVec2(icon_size, icon_size));

        if (selected) {
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PopStyleColor();
        }

        // draw icon on top, centered
        ImGui::SetWindowFontScale(icon_scale);
        const ImVec2 icon_size_actual = ImGui::CalcTextSize(icon);
        ImGui::SetCursorPosX(pos.x + (icon_size - icon_size_actual.x) * 0.5f);
        ImGui::SetCursorPosY(pos.y + (icon_size - icon_size_actual.y) * 0.5f);
        ImGui::TextUnformatted(icon);
        ImGui::SetWindowFontScale(1.0f);

        // reset cursor to below the button
        ImGui::SetCursorPosY(pos.y + icon_size + ImGui::GetStyle().ItemSpacing.y);
        const float width = ImGui::CalcTextSize(text, nullptr, false, icon_size).x;
        const float start = icon_size > width ? (icon_size - width) / 2 : 0;

        // filename text under icon
        ImGui::SetCursorPosX(pos.x + start);
        ImGui::PushTextWrapPos(pos.x + icon_size);
        ImGui::TextWrapped("%s", text);
        ImGui::PopTextWrapPos();
    }
    ImGui::EndGroup();
}

void FileView::update_directory(const Path& path, bool force)
{
    // stop if no changes
    if (!force && curr == path) return;

    // update current path
    curr = path;

    // invalid directory cache
    files.clear();
    folders.clear();
    breadcrumbs.clear();
    selected_items.clear();
    last_selected = "";

    // re-evaluate breadcrumbs
    auto relative   = std::filesystem::relative(curr, root);
    Path bread_path = root;
    for (const auto& part : relative) {
        if (part == ".") continue; // skip current directory indicator
        bread_path /= part;
        breadcrumbs.push_back({to_string(part), bread_path});
    }

    // re-evaluate immediate files and folders
    for (const auto& entry : std::filesystem::directory_iterator(curr)) {
        const auto& abs_path = entry.path();
        const auto  rel_path = to_string(std::filesystem::relative(abs_path, curr));

        // fallback: regular files
        if (entry.is_directory()) {
            folders.push_back(rel_path);
        } else {
            // hide *.import meta files
            if (abs_path.extension() == ".import")
                continue;

            files.push_back(rel_path);
        }
    }
}
