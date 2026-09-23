#include "ViewportCanvas.h"
#include <Lyra/UISystem/Widgets/UI.h>

using namespace lyra;

ViewportCanvas::ViewportCanvas()
{
    // do nothing
}

ViewportCanvas::~ViewportCanvas()
{
    // do nothing
}

void ViewportCanvas::init(uint frames_in_flight)
{
    frame_count = frames_in_flight;
    frame_index = 0;
}

void ViewportCanvas::update(AppContext& context)
{
    // update current frame
    frame_index = (frame_index + 1) % frame_count;

    // check window properties
    detect_window();

    // update framebuffer
    if (frame_changed) {
        delete_frames(context);
        create_frames(context);
    }
}

const ViewportCanvasFrame& ViewportCanvas::get_frame() const
{
    return frames.at(frame_index);
}

Backbuffer ViewportCanvas::get_backbuffer() const
{
    auto& frame = get_frame();

    Backbuffer backbuffer = {};
    backbuffer.format     = GPUTextureFormat::RGBA8UNORM;
    backbuffer.extent     = frame.extent;
    backbuffer.texture    = frame.texture;
    backbuffer.texview    = frame.texview;
    return backbuffer;
}

void ViewportCanvas::display() const
{
    auto& frame = get_frame();
    ui::image(frame.tex_id.texid, frame_extent);
}

void ViewportCanvas::detect_window()
{
    // detect window appearing
    frame_visible = ui::is_panel_appearing();
    frame_changed = false;

    // detect new frames
    if (frames.size() < frame_count)
        frame_changed = true;

    // detect window resized
    Vector2 curr_extent = ui::get_available_space();
    if (curr_extent.x != frame_extent.x || curr_extent.y != frame_extent.y)
        frame_changed = true;

    frame_extent   = curr_extent;
    frame_extent.x = std::max(0.0f, frame_extent.x);
    frame_extent.y = std::max(0.0f, frame_extent.y);
}

void ViewportCanvas::create_frames(AppContext& context)
{
    auto extent   = GPUExtent2D{};
    extent.width  = std::max(64u, static_cast<uint>(frame_extent.x));
    extent.height = std::max(64u, static_cast<uint>(frame_extent.y));

    auto gui = context.toolboard.get<GUIRenderer*>();
    auto dev = context.toolboard.get<GPUDevice*>();
    for (uint i = 0; i < frame_count; i++) {
        ViewportCanvasFrame frame = {};

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
            return dev->create_texture(desc);
        });
        frame.texview = frame.texture.create_view();
        frame.extent  = extent;

        // gui's create_texture will take over ownership of texture and texview
        frame.tex_id = gui->create_texture(frame.texture, frame.texview);
        frames.push_back(frame);
    }
}

void ViewportCanvas::delete_frames(AppContext& context)
{
    auto gui = context.toolboard.get<GUIRenderer*>();
    for (auto& frame : frames)
        gui->delete_texture(frame.tex_id);

    frames.clear();
}
