#pragma once

#ifndef LYRA_EDITOR_COMMON_VIEWPORT_CANVAS_H
#define LYRA_EDITOR_COMMON_VIEWPORT_CANVAS_H

#include <Lyra/Common/Math.h>
#include <Lyra/UICore/GUITypes.h>
#include <Lyra/Render/RHITypes.h>

// local imports
#include <Lyra/Engine/Application.h>

namespace lyra
{
    struct ViewportCanvasFrame
    {
        GUITexture     tex_id  = GUITexture{};
        GPUTexture     texture = GPUTexture{};
        GPUTextureView texview = GPUTextureView{};
        GPUExtent2D    extent  = GPUExtent2D{};
    };

    struct ViewportCanvas
    {
    public:
        explicit ViewportCanvas();
        virtual ~ViewportCanvas();

        void init(uint frames_in_flight);

        void update(Blackboard& blackboard);

        bool& is_visible() { return frame_visible; }

        bool is_visible() const { return frame_visible; }

        auto get_frame() const -> const ViewportCanvasFrame&;

        auto get_backbuffer() const -> Backbuffer;

        void display() const;

    private:
        void detect_window();
        void create_frames(Blackboard& blackboard);
        void delete_frames(Blackboard& blackboard);

    private:
        uint    frame_count   = 0;
        uint    frame_index   = 0;
        Vector2 frame_extent  = {0.0f, 0.0f};
        bool    frame_changed = false;
        bool    frame_visible = false;

        Vector<ViewportCanvasFrame> frames;
    };

    using CanvasFrame = ViewportCanvasFrame;
    using Canvas      = ViewportCanvas;

} // namespace lyra

#endif // LYRA_EDITOR_COMMON_VIEWPORT_CANVAS_H
