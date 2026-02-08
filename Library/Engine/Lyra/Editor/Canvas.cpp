#include "Canvas.h"

using namespace lyra;

Canvas::Canvas()
{
    // do nothing
}

Canvas::~Canvas()
{
    // do nothing
}

void Canvas::init(uint frames_in_flight)
{
    frame_count = frames_in_flight;
    frame_index = 0;
}

void Canvas::update(Blackboard& blackboard)
{
    // update current frame
    frame_index = (frame_index + 1) % frame_count;

    // check window properties
    detect_window();

    // update framebuffer
    if (frame_changed) {
        delete_frames(blackboard);
        create_frames(blackboard);
    }
}

const CanvasFrame& Canvas::get_frame() const
{
    return frames.at(frame_index);
}

Backbuffer Canvas::get_backbuffer() const
{
    auto& frame = get_frame();

    Backbuffer backbuffer = {};
    backbuffer.format     = GPUTextureFormat::RGBA8UNORM;
    backbuffer.extent     = frame.extent;
    backbuffer.texture    = frame.texture;
    backbuffer.texview    = frame.texview;
    return backbuffer;
}

void Canvas::display() const
{
    auto& frame = get_frame();
    ImGui::Image(frame.tex_id.texid, frame_extent);
}

void Canvas::detect_window()
{
    // detect window appearing
    frame_visible = ImGui::IsWindowAppearing();
    frame_changed = false;

    // detect new frames
    if (frames.size() < frame_count)
        frame_changed = true;

    // detect window resized
    ImVec2 curr_extent = ImGui::GetContentRegionAvail();
    if (curr_extent.x != frame_extent.x || curr_extent.y != frame_extent.y)
        frame_changed = true;

    frame_extent = curr_extent;
}

void Canvas::create_frames(Blackboard& blackboard)
{
    auto extent   = GPUExtent2D{};
    extent.width  = std::max(64u, static_cast<uint>(frame_extent.x));
    extent.height = std::max(64u, static_cast<uint>(frame_extent.y));

    auto gui = blackboard.get<GUIRenderer*>();
    auto dev = blackboard.get<GPUDevice>();
    for (uint i = 0; i < frame_count; i++) {
        CanvasFrame frame = {};

        // create texture storage and texture view
        frame.texture = lyra::execute([&]() {
            GPUTextureDescriptor desc{};
            desc.dimension       = GPUTextureDimension::x2D;
            desc.format          = GPUTextureFormat::RGBA8UNORM;
            desc.size.width      = extent.width;
            desc.size.height     = extent.height;
            desc.size.depth      = 1;
            desc.array_layers    = 1;
            desc.mip_level_count = 1;
            desc.sample_count    = 1;
            desc.usage           = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            return dev.create_texture(desc);
        });
        frame.texview = frame.texture.create_view();
        frame.extent  = extent;

        // imgui's create_texture will take over ownership of texture and texview
        frame.tex_id = gui->create_texture(frame.texture, frame.texview);
        frames.push_back(frame);
    }
}

void Canvas::delete_frames(Blackboard& blackboard)
{
    auto gui = blackboard.get<GUIRenderer*>();
    for (auto& frame : frames)
        gui->delete_texture(frame.tex_id);

    frames.clear();
}
