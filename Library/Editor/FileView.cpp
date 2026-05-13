#include <algorithm>
#include <fstream>
#include <stb_image.h>
#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/Render/RHIInits.h>

// local imports
#include <Lyra/Editor/Icons.h>
#include <Lyra/Editor/Colors.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/FileView.h>

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
    bboard = &app.get_blackboard();
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

        float start_y = ImGui::GetCursorPosY();
        show_breadcrumb();

        const float search_bar_width = 250.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosY(start_y);
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - search_bar_width);
        ImGui::SetNextItemWidth(search_bar_width);
        ImGui::InputTextWithHint("##FileSearch", LYRA_ICON_FILTER " Search...", search_filter, sizeof(search_filter));

        ImGui::Separator();
        ImGui::BeginChild("##FileBrowser", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
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

        ImGui::Separator();
        ImGui::TextDisabled(" %zu items  |  %zu selected", files.size() + folders.size(), selection.size());
    }
    ImGui::End();
}

// --- UI Helpers ---

void FileView::show_breadcrumb()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    // Root / Home
    if (curr == root) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled(LYRA_ICON_HOME);
    } else {
        if (ImGui::Button(LYRA_ICON_HOME)) {
            update_directory(root);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go to Root");
    }

    for (size_t i = 0; i < breadcrumbs.size(); ++i) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled(LYRA_ICON_CARET);
        ImGui::SameLine();

        const auto& bc      = breadcrumbs[i];
        bool        is_last = (i == breadcrumbs.size() - 1);

        if (is_last) {
            ImGui::TextUnformatted(bc.name.c_str());
        } else {
            if (ImGui::Button(bc.name.c_str())) {
                update_directory(bc.path);
            }
        }
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
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

    String filter(search_filter);
    auto   matches_filter = [&](StringView name) {
        if (filter.empty()) return true;
        String n(name);
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        String f(filter);
        std::transform(f.begin(), f.end(), f.begin(), ::tolower);
        return n.find(f) != String::npos;
    };

    for (const auto& folder : folders) {
        if (!matches_filter(folder)) continue;
        handle_marquee(folder);
        show_item(blackboard, grid, ctx, folder, true);
    }

    for (const auto& file : files) {
        if (!matches_filter(file)) continue;
        handle_marquee(file);
        show_item(blackboard, grid, ctx, file, false);
    }

    ImGui::NewLine();
}

void FileView::show_item(Blackboard& blackboard, IconGrid& grid, IconGrid::Context& ctx, StringView name, bool is_folder)
{
    ImGui::PushID(name.data(), name.data() + name.size());

    bool is_sel = selection.is_selected(name);

    ImTextureID tex_id = ImTextureID_Invalid;
    ImVec2      thumb_size = {0, 0};
    if (!is_folder) {
        auto it = thumbnails.find(String(name));
        if (it != thumbnails.end()) {
            if (it->second.valid) {
                tex_id = as_type<ImTextureID>(it->second.gui_texture.texid);
                thumb_size = ImVec2((float)it->second.texture.width, (float)it->second.texture.height);
            }
        } else {
            // Check for .import file
            Path import_path = curr / (String(name) + ".import");
            if (std::filesystem::exists(import_path)) {
                try {
                    std::ifstream f(import_path);
                    JSON          j = JSON::parse(f);
                    if (j.contains("thumbnail")) {
                        load_thumbnail(blackboard, name, j["thumbnail"]);
                        auto it2 = thumbnails.find(String(name));
                        if (it2 != thumbnails.end() && it2->second.valid) {
                            tex_id = as_type<ImTextureID>(it2->second.gui_texture.texid);
                            thumb_size = ImVec2((float)it2->second.texture.width, (float)it2->second.texture.height);
                        }
                    }
                } catch (...) {
                    // silent fail for now
                }
            }
            if (tex_id == ImTextureID_Invalid) {
                thumbnails[String(name)] = {{}, {}, false};
            }
        }
    }

    int inter = 0;
    if (tex_id != ImTextureID_Invalid) {
        inter = grid.draw_image_item(ctx, tex_id, thumb_size, name.data(), is_sel);
    } else {
        inter = grid.draw_item(ctx, is_folder ? LYRA_ICON_FOLDER : LYRA_ICON_FILE, name.data(), is_sel, is_folder ? LYRA_COLOR_FOLDER : ImVec4(0, 0, 0, 0));
    }

    if (inter & IconGrid::Clicked) {
        if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper)
            selection.toggle(name);
        else
            selection.select_only(name);
    }

    if ((inter & IconGrid::DoubleClicked) && is_folder) {
        update_directory(curr / name);
    }

    if (ImGui::BeginPopupContextItem("ItemContextMenu")) {
        if (!is_sel) selection.select_only(name);

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

    grid.next_column(ctx);
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

    if (bboard && bboard->has<GUIRenderer*>()) {
        auto gui = bboard->get<GUIRenderer*>();
        for (auto& [name, thumb] : thumbnails) {
            if (thumb.valid) {
                gui->delete_texture(thumb.gui_texture);
            }
        }
    }
    thumbnails.clear();

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

    constexpr uint hovered_flags = ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                   ImGuiHoveredFlags_ChildWindows |
                                   ImGuiHoveredFlags_AllowWhenBlockedByPopup;

    if (!ImGui::IsWindowHovered(hovered_flags))
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

void FileView::load_thumbnail(Blackboard& blackboard, StringView name, const String& thumb_rel_path)
{
    auto loader = blackboard.get<FileLoader*>();

    const String& loader_path = thumb_rel_path;

    if (!loader->exists(loader_path.c_str())) {
        spdlog::warn("Thumbnail file does not exist: {}", loader_path);
        return;
    }

    auto content = loader->read<uint8_t>(loader_path.c_str());
    if (content.empty()) {
        spdlog::error("Failed to read thumbnail file data: {}", loader_path);
        return;
    }

    int      w, h, c;
    stbi_uc* data = stbi_load_from_memory(content.data(), (int)content.size(), &w, &h, &c, STBI_rgb_alpha);
    if (!data) {
        spdlog::error("Failed to decode thumbnail image: {}", loader_path);
        return;
    }

    auto& device  = RHI::get_current_device();
    auto& adapter = RHI::get_current_adapter();
    auto  gui     = blackboard.get<GUIRenderer*>();

    GPUTextureDescriptor tex_desc{};
    tex_desc.size      = {(uint)w, (uint)h, 1};
    tex_desc.format    = GPUTextureFormat::RGBA8UNORM;
    tex_desc.usage     = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
    GPUTexture texture = device.create_texture(tex_desc);

    uint alignment = adapter.properties.texture_row_pitch_alignment;
    uint row_pitch = (w * 4 + alignment - 1) & ~(alignment - 1);

    GPUBufferDescriptor buf_desc{};
    buf_desc.size     = row_pitch * h;
    buf_desc.usage    = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
    GPUBuffer staging = device.create_buffer(buf_desc);

    staging.map(GPUMapMode::WRITE);
    auto mapped = staging.get_mapped_range();
    for (int i = 0; i < h; i++) {
        std::memcpy(mapped.data + row_pitch * i, data + w * 4 * i, w * 4);
    }
    staging.unmap();

    GPUCommandBuffer cmdbuffer = execute([&]() {
        auto desc = GPUCommandBufferDescriptor{};
        return device.create_command_buffer(desc);
    });

    GPUTexelCopyBufferInfo src_info{};
    src_info.buffer         = staging;
    src_info.bytes_per_row  = row_pitch;
    src_info.rows_per_image = h;

    GPUTexelCopyTextureInfo dst_info{};
    dst_info.texture = texture;
    dst_info.aspect  = GPUTextureAspect::COLOR;

    cmdbuffer.resource_barrier(state_transition(texture, undefined_state(), copy_dst_state()));
    cmdbuffer.copy_buffer_to_texture(src_info, dst_info, {(uint)w, (uint)h, 1});
    cmdbuffer.resource_barrier(state_transition(texture, copy_dst_state(), shader_resource_state(GPUBarrierSync::PIXEL_SHADING)));
    cmdbuffer.submit();

    device.wait();

    staging.destroy();
    stbi_image_free(data);

    GUITexture gui_tex       = gui->create_texture(texture, texture.create_view());
    thumbnails[String(name)] = {texture, gui_tex, true};
}
