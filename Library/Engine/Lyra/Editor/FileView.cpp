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
    assert(std::filesystem::exists(root));
    assert(std::filesystem::is_directory(root));
}

void FileView::bind(Application& app)
{
    app.bind<AppEvent::UPDATE, &FileView::update>(*this);
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

// --- UI Helpers ---

void FileView::show_breadcrumb()
{
    if (ImGui::Button(LYRA_ICON_HOME " Home"))
        update_directory(root);

    ImGui::SameLine();
    ImGui::TextUnformatted(LYRA_ICON_CARET);
    ImGui::SameLine();

    if (root == curr) {
        ImGui::NewLine();
        return;
    }

    for (const auto& breadcrumb : breadcrumbs) {
        if (ImGui::Button(breadcrumb.name.c_str())) {
            update_directory(breadcrumb.path);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(LYRA_ICON_CARET);
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

void FileView::show_dir_files(Blackboard& blackboard)
{
    auto ctx = grid.begin();

    ImVec2 marquee_end_pos = ImGui::GetMousePos();
    ImRect marquee_rect;

    // background click logic
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            is_marquee_selecting = true;
            marquee_start_pos    = ImGui::GetMousePos();
            initial_selection    = (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) ? selection.items : Vector<String>();
            if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeySuper) {
                selection.clear();
            }
        }
    }

    if (is_marquee_selecting) {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            is_marquee_selecting = false;
            initial_selection.clear();
        } else {
            marquee_rect = ImRect(marquee_start_pos, marquee_end_pos);
            if (marquee_rect.Min.x > marquee_rect.Max.x) std::swap(marquee_rect.Min.x, marquee_rect.Max.x);
            if (marquee_rect.Min.y > marquee_rect.Max.y) std::swap(marquee_rect.Min.y, marquee_rect.Max.y);

            // draw marquee visual
            ImGui::GetWindowDrawList()->AddRectFilled(marquee_rect.Min, marquee_rect.Max, ImGui::GetColorU32(ImGuiCol_Header, 0.3f));
            ImGui::GetWindowDrawList()->AddRect(marquee_rect.Min, marquee_rect.Max, ImGui::GetColorU32(ImGuiCol_Header, 1.0f));

            // start fresh from initial state for this frame's calculation
            selection.items = initial_selection;
        }
    }

    auto handle_marquee = [&](StringView name) {
        if (is_marquee_selecting) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImRect item_rect(pos, ImVec2(pos.x + grid.grid_size, pos.y + grid.grid_size));
            if (marquee_rect.Overlaps(item_rect)) {
                if (!selection.is_selected(name)) {
                    selection.items.emplace_back(name);
                }
            }
        }
    };

    for (const auto& folder : folders) {
        handle_marquee(folder);
        show_item(blackboard, grid, ctx, folder, true);
    }

    for (const auto& file : files) {
        handle_marquee(file);
        show_item(blackboard, grid, ctx, file, false);
    }

    ImGui::NewLine();
}

void FileView::show_item(Blackboard& blackboard, IconGrid& grid, IconGrid::Context& ctx, StringView name, bool is_folder)
{
    ImGui::PushID(name.data(), name.data() + name.size());

    bool is_sel = selection.is_selected(name);
    int  inter  = grid.draw_item(ctx, is_folder ? LYRA_ICON_FOLDER : LYRA_ICON_FILE, name.data(), is_sel);

    if (inter & IconGrid::Clicked) {
        if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper)
            selection.toggle(name);
        else
            selection.select_only(name);
    }

    if ((inter & IconGrid::DoubleClicked) && is_folder) {
        update_directory(curr / name);
    }

    if (inter & IconGrid::RightClicked) {
        if (!is_sel) selection.select_only(name);
        ImGui::OpenPopup(is_folder ? "FolderItemContextMenu" : "FileItemContextMenu");
    }

    // shared context menu logic
    auto render_context_menu = [&](const char* id) {
        if (ImGui::BeginPopup(id)) {
            if (selection.size() == 1) {
                if (ImGui::MenuItem(LYRA_ICON_RENAME " Rename")) {
                    show_rename_modal = true;
                    strncpy(rename_buffer, selection.items[0].c_str(), sizeof(rename_buffer) - 1);
                }
            }
            if (!is_folder) {
                if (ImGui::MenuItem(LYRA_ICON_IMPORT " Re-import")) {
                    action_reimport_selected(blackboard.get<AssetServer*>());
                }
            }
            if (ImGui::MenuItem(LYRA_ICON_DELETE " Delete")) {
                show_delete_modal = true;
            }
            ImGui::EndPopup();
        }
    };

    render_context_menu("FolderItemContextMenu");
    render_context_menu("FileItemContextMenu");

    ImGui::PopID();
}

void FileView::show_context_menu(Blackboard& blackboard)
{
    if (ImGui::BeginPopupContextWindow("File Manager Context Menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem(LYRA_ICON_REFRESH " Refresh")) {
            update_directory(curr, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(LYRA_ICON_IMPORT " Import")) {
            spdlog::info("Import not implemented");
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

    if (show_new_folder_modal) ImGui::OpenPopup("New Folder");
    if (show_delete_modal) ImGui::OpenPopup("Delete");
    if (show_rename_modal) ImGui::OpenPopup("Rename");
}

// --- Actions ---

void FileView::action_delete_selected()
{
    for (const auto& target : selection.items) {
        Path p = curr / target;
        try {
            if (std::filesystem::exists(p)) std::filesystem::remove_all(p);
        } catch (const std::exception& e) {
            spdlog::error("Failed to delete {}: {}", p.string(), e.what());
        }
    }
    update_directory(curr, true);
}

void FileView::action_rename(StringView old_name, StringView new_name)
{
    if (old_name == new_name) return;
    Path op = curr / old_name;
    Path np = curr / new_name;
    try {
        if (std::filesystem::exists(np))
            spdlog::error("Rename failed: Destination exists");
        else {
            std::filesystem::rename(op, np);
            update_directory(curr, true);
        }
    } catch (const std::exception& e) {
        spdlog::error("Rename error: {}", e.what());
    }
}

void FileView::action_create_folder(StringView name)
{
    Path p = curr / name;
    try {
        if (std::filesystem::create_directory(p)) update_directory(curr, true);
    } catch (const std::exception& e) {
        spdlog::error("Folder create error: {}", e.what());
    }
}

void FileView::action_reimport_selected(AssetServer* ams)
{
    for (const auto& sel : selection.items) {
        if (std::find(files.begin(), files.end(), sel) != files.end()) {
            auto fut = ams->import_asset(std::filesystem::relative(curr / sel, root));
            if (fut.valid()) active_imports.push_back(std::move(fut));
        }
    }
}

// --- Modals ---

void FileView::show_new_folder_dialog()
{
    if (ImGui::BeginPopupModal("New Folder", &show_new_folder_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
            memset(new_folder_name, 0, sizeof(new_folder_name));
        }
        ImGui::Text("Enter folder name:");
        if (ImGui::InputText("##FolderName", new_folder_name, sizeof(new_folder_name), ImGuiInputTextFlags_EnterReturnsTrue)) {
            action_create_folder(new_folder_name);
            show_new_folder_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            action_create_folder(new_folder_name);
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
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::Text("Enter new name:");
        if (ImGui::InputText("##RenameBuffer", rename_buffer, sizeof(rename_buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            action_rename(selection.items[0], rename_buffer);
            show_rename_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            action_rename(selection.items[0], rename_buffer);
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

void FileView::show_delete_dialog(Blackboard&)
{
    if (ImGui::BeginPopupModal("Delete", &show_delete_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (selection.size() == 1) {
            ImGui::Text("Are you sure you want to delete:");
            float w = ImGui::CalcTextSize(selection.items[0].c_str()).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - w) * 0.5f);
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "'%s'", selection.items[0].c_str());
        } else {
            ImGui::Text("Are you sure you want to delete %zu selected items?", selection.size());
        }
        ImGui::Separator();
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            action_delete_selected();
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

void FileView::update_directory(const Path& path, bool force)
{
    if (!force && curr == path) return;
    curr = path;
    files.clear();
    folders.clear();
    all_items.clear();
    breadcrumbs.clear();
    selection.clear();

    auto relative = std::filesystem::relative(curr, root);
    Path b_path   = root;
    for (const auto& part : relative) {
        if (part == ".") continue;
        b_path /= part;
        breadcrumbs.push_back({to_string(part), b_path});
    }

    for (const auto& entry : std::filesystem::directory_iterator(curr)) {
        if (entry.path().extension() == ".import") continue;
        String rel = to_string(std::filesystem::relative(entry.path(), curr));
        if (entry.is_directory())
            folders.push_back(rel);
        else
            files.push_back(rel);
    }

    // sort and build all_items
    std::sort(folders.begin(), folders.end());
    std::sort(files.begin(), files.end());

    all_items.reserve(folders.size() + files.size());
    for (const auto& f : folders)
        all_items.push_back(f);
    for (const auto& f : files)
        all_items.push_back(f);
}

void FileView::handle_file_drop(Blackboard& blackboard)
{
    auto window = blackboard.get<Window*>();
    auto ams    = blackboard.get<AssetServer*>();

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
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to copy/import dropped file {}: {}", path_str, e.what());
        }
    }
    update_directory(curr, true);
}

void FileView::show_import_indicator()
{
    for (auto it = active_imports.begin(); it != active_imports.end();) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            if (it->get() != 0)
                finished_success++;
            else
                finished_failure++;
            it                 = active_imports.erase(it);
            notification_timer = 5.0f;
        } else
            ++it;
    }

    if (active_imports.empty() && notification_timer <= 0.0f) return;

    if (active_imports.empty() && notification_timer > 0.0f) {
        notification_timer -= ImGui::GetIO().DeltaTime;
    }

    ImVec2 region = ImGui::GetWindowContentRegionMax();
    ImGui::SetCursorPos(ImVec2(region.x - 250, region.y - 50));
    ImGui::BeginChild("##ImportIndicator", ImVec2(250, 50), true, ImGuiWindowFlags_NoScrollbar);
    {
        if (!active_imports.empty()) {
            ImGui::Text(LYRA_ICON_IMPORT " Importing %zu assets...", active_imports.size());
        } else {
            ImGui::TextColored(finished_failure > 0 ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.4f, 1, 0.4f, 1),
                "Import finished: %u ok, %u failed", finished_success, finished_failure);
            if (ImGui::IsWindowHovered()) notification_timer = 0.0f;
        }
    }
    ImGui::EndChild();

    if (active_imports.empty() && notification_timer <= 0.0f) {
        finished_success = 0;
        finished_failure = 0;
    }
}

void FileView::show_new_file_dialog()
{
    if (ImGui::BeginPopupModal("New File", &show_new_file_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("This is a modal dialog (file)!");
        ImGui::Separator();
        if (ImGui::Button("Close")) {
            show_new_file_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
