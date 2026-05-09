#pragma once

#ifndef LYRA_LYRA_EDITOR_CANVAS_H
#define LYRA_LYRA_EDITOR_CANVAS_H

#include <Lyra/Common/GUI.h>
#include <Lyra/UICore/GUITypes.h>
#include <Lyra/Render/RHITypes.h>

// local imports
#include <Lyra/Player/Application.h>

namespace lyra
{
    struct CanvasFrame
    {
        GUITexture     tex_id  = GUITexture{};
        GPUTexture     texture = GPUTexture{};
        GPUTextureView texview = GPUTextureView{};
        GPUExtent2D    extent  = GPUExtent2D{};

        auto imtex() const -> ImTextureID
        {
            return as_type<ImTextureID>(tex_id.texid);
        }
    };

    struct Canvas
    {
    public:
        explicit Canvas();
        virtual ~Canvas();

        void init(uint frames_in_flight);

        void update(Blackboard& blackboard);

        bool& is_visible() { return frame_visible; }

        bool is_visible() const { return frame_visible; }

        auto get_frame() const -> const CanvasFrame&;

        auto get_backbuffer() const -> Backbuffer;

        void display() const;

    private:
        void detect_window();
        void create_frames(Blackboard& blackboard);
        void delete_frames(Blackboard& blackboard);

    private:
        uint   frame_count = 0;
        uint   frame_index = 0;
        ImVec2 frame_extent;
        bool   frame_changed = false;
        bool   frame_visible = false;

        Vector<CanvasFrame> frames;
    };

} // namespace lyra

#endif // LYRA_LYRA_EDITOR_CANVAS_H
