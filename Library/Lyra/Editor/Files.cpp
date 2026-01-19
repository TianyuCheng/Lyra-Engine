#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Editor/Icons.h>
#include <Lyra/Editor/Colors.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/Files.h>

#define LYRA_FILES_WINDOW_NAME (LYRA_ICON_FOLDER "Files")

using namespace lyra;

Files::Files(const Path& root)
    : root(root), curr(root)
{
    // sanity check
    assert(std::filesystem::exists(root));
    assert(std::filesystem::is_directory(root));
}

void Files::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &Files::update>(*this);

    // initial data
    update_directory(root, true);
}

void Files::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_FILES_WINDOW_NAME, layout.bottom);
    });

    imgui::disable_window_menu_button();
    ImGui::Begin(LYRA_FILES_WINDOW_NAME);
    {
        show_breadcrumb();
        ImGui::Separator();
        ImGui::BeginChild("##FileBrowser");
        {
            show_dir_files();
            show_context_menu();
            show_new_file_dialog();
            show_new_folder_dialog();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void Files::show_breadcrumb()
{
    // root / home
    if (ImGui::Button(LYRA_ICON_HOME "Home"))
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
    auto relative = std::filesystem::relative(curr, root);
    for (const auto& part : relative) {
        auto parts = to_string(part);
        if (ImGui::Button(parts.c_str())) {
            Path path = root;
            for (auto p : relative) {
                path /= p;
                if (p == part) {
                    update_directory(path);
                    break;
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

void Files::show_dir_files()
{
    // drawing grid
    int   grid_id   = 0;
    float start_x   = ImGui::GetCursorPosX();
    float row_width = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    // folders
    for (const auto& folder : folders) {

        ImGui::PushID(grid_id++);
        {
            // folder icon
            draw_icon_grid(LYRA_ICON_FOLDER, folder.c_str(), 3.0f);
            next_icon_grid(row_width, start_x);

            // handle clicks
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                update_directory(curr / folder);
        }
        ImGui::PopID();
    }

    // files
    for (const auto& file : files) {

        ImGui::PushID(grid_id++);
        {
            // file icon
            draw_icon_grid(LYRA_ICON_FILE, file.c_str(), 3.0f);
            next_icon_grid(row_width, start_x);

            // TODO: handle clicks
        }
        ImGui::PopID();
    }

    ImGui::NewLine();
}

void Files::show_context_menu()
{
    // capture the whole file browser region for right click
    auto region = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##FileBrowserContext", region);

    // detect context menu (right click)
    if (ImGui::BeginPopupContextItem("File Manager Context Menu")) {
        if (ImGui::MenuItem(LYRA_ICON_REFRESH " Refresh")) {
            update_directory(curr, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(LYRA_ICON_NEW_FILE " New File")) {
            show_new_file_modal = true;
        }
        if (ImGui::MenuItem(LYRA_ICON_NEW_FOLDER " New Folder")) {
            show_new_folder_modal = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(LYRA_ICON_IMPORT " Import")) {
            spdlog::info("Import Asset!");
        }
        ImGui::EndPopup();
    }

    // must be called outside of context menu
    if (show_new_file_modal) {
        ImGui::OpenPopup("New File");
    }

    // must be called outside of context menu
    if (show_new_folder_modal) {
        ImGui::OpenPopup("New Folder");
    }
}

void Files::show_new_file_dialog()
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

void Files::show_new_folder_dialog()
{
    if (ImGui::BeginPopupModal("New Folder", &show_new_folder_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("This is a modal dialog (folder)!");
        ImGui::Separator();

        if (ImGui::Button("Close")) {
            show_new_folder_modal = false;
            ImGui::CloseCurrentPopup(); // Close the current popup
        }

        ImGui::EndPopup();
    }
}

void Files::next_icon_grid(float row_width, float start_x)
{
    // calculate next position
    float last_x = ImGui::GetItemRectMax().x;
    float next_x = last_x + padding + icon_size;
    if (next_x < row_width) {
        ImGui::SameLine();
    } else {
        ImGui::NewLine();
        ImGui::SetCursorPosX(start_x); // align new row
    }
}

void Files::draw_icon_grid(CString icon, CString text, float icon_scale) const
{
    ImGui::BeginGroup(); // group icon + text together
    {
        // icon
        ImGui::SetWindowFontScale(icon_scale);
        ImGui::Button(icon, ImVec2(icon_size, icon_size));
        ImGui::SetWindowFontScale(1.0f);

        const float width = ImGui::CalcTextSize(text).x;
        const float start = icon_size > width ? (icon_size - width) / 2 : 0;

        // filename text under icon
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + icon_size);
        ImGui::TextWrapped("%s", text);
        ImGui::PopTextWrapPos();
    }
    ImGui::EndGroup();
}

void Files::update_directory(const Path& path, bool force)
{
    // stop if no changes
    if (!force && curr == path) return;

    // update current path
    curr = path;

    // invalid directory cache
    files.clear();
    folders.clear();

    // re-evaluate immediate files and folders
    for (const auto& entry : std::filesystem::directory_iterator(curr)) {
        const auto& abs_path = entry.path();
        const auto  rel_path = to_string(std::filesystem::relative(abs_path, curr));

        // fallback: regular files
        if (entry.is_directory()) {
            folders.push_back(rel_path);
        } else {
            files.push_back(rel_path);
        }
    }
}
