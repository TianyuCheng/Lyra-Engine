#pragma once

#ifndef LYRA_LIBRARY_ENGINE_HELPER_CANVAS_H
#define LYRA_LIBRARY_ENGINE_HELPER_CANVAS_H

#include <Lyra/Common/GUI.h>
#include <Lyra/Plugin/GUI/GUITypes.h>
#include <Lyra/Plugin/RHI/RHITypes.h>
#include <Lyra/Engine/Applet/Application.h>

namespace lyra
{
    struct ImGuiFrame
    {
        GUITexture     tex_id  = GUITexture{};
        GPUTexture     texture = GPUTexture{};
        GPUTextureView texview = GPUTextureView{};
        GPUExtent2D    extent  = GPUExtent2D{};
    };

    struct ImGuiCanvas
    {
    public:
        explicit ImGuiCanvas();
        virtual ~ImGuiCanvas();

        void init(uint frames_in_flight);

        void update(Blackboard& blackboard);

        bool& is_visible() { return frame_visible; }

        bool is_visible() const { return frame_visible; }

        auto get_frame() const -> const ImGuiFrame&
        {
            return frames.at(frame_index);
        }

        auto get_backbuffer() const -> Backbuffer
        {
            auto& frame = get_frame();

            Backbuffer backbuffer = {};
            backbuffer.format     = GPUTextureFormat::RGBA8UNORM;
            backbuffer.extent     = frame.extent;
            backbuffer.texture    = frame.texture;
            backbuffer.texview    = frame.texview;
            return backbuffer;
        }

        void display() const
        {
            auto& frame = get_frame();
            ImGui::Image(frame.tex_id.texid, frame_extent);
        }

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

        Vector<ImGuiFrame> frames;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_SYSTEM_CANVAS_H
