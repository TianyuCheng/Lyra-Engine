#pragma once

#ifndef LYRA_MODULE_UISYSTEM_IMGUI_RENDERER_H
#define LYRA_MODULE_UISYSTEM_IMGUI_RENDERER_H

// imgui header(s)
#include <imgui.h>

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Conversion.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Windowing/WSIAPI.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/Compiler/SLCAPI.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/UISystem/GUIAPI.h>

using namespace lyra;

namespace lyra::imgui
{
    Logger get_logger();

    struct GUITexture
    {
        GPUBindGroup   bindgroup;
        GPUTexture     texture;
        GPUTextureView view;

        bool valid() const { return texture.valid() || view.valid(); }
    };

    struct GUIWindowContext
    {
        WindowHandle     window;
        ImGuiContext*    context = nullptr;
        WindowInputQuery events;
    };

    struct GUIPipelineData;
    struct GUIRendererData;
    struct GUIViewportData
    {
        GUIPipelineData* pipeline = nullptr;
        GUIRendererData* renderer = nullptr;
        GPUSurfaceHandle surface;
        WindowHandle     window;
        bool             owned;
    };

    struct GUIPlatformData
    {
        WindowHandle             primary_window;
        ImGuiContext*            context           = nullptr;
        float                    elapsed           = 0.0f;
        Vector<GUIWindowContext> window_contexts   = {};
        Vector<GUIViewportData*> garbage_viewports = {};
    };

    struct GUITextureDeleter
    {
        // deferred deletion, only reset the handles
        void operator()(GUITexture* texture)
        {
            texture->texture.handle.reset();
            texture->view.handle.reset();
        }
    };

    using GUITextureManager = Slotmap<GUITexture, GUITextureDeleter>;

    struct GUIGarbageBuffer
    {
        GPUBuffer object;
        uint      unused = 0;

        bool should_remove(uint threshold) { return unused++ >= threshold; }
    };

    struct GUIGarbageTexture
    {
        GUITextureManager::handle texid;
        GUITexture                object;
        uint                      unused = 0;

        bool should_remove(uint threshold) { return unused++ >= threshold; }
    };

    using GUIGarbageBuffers  = Vector<GUIGarbageBuffer>;
    using GUIGarbageTextures = Vector<GUIGarbageTexture>;

    struct GUIPipelineData
    {
        GPURenderPipeline                pipeline;
        GPUPipelineLayout                playout;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        GPUShaderModule                  vshader;
        GPUShaderModule                  fshader;
        uint                             frame_count = 3;
        uint                             frame_index = 0;
    };

    struct GUIRendererData
    {
        Vector<GPUBuffer>  vbuffers;
        Vector<GPUBuffer>  ibuffers;
        GPUBindGroupHeap   heap;
        GPUSampler         sampler;
        GUITextureManager  textures;
        GUIGarbageBuffers  garbage_buffers;
        GUIGarbageTextures garbage_textures;
    };

    struct GUIRenderer
    {
    public:
        explicit GUIRenderer(const GUIDescriptor& descriptor);

        void init(const GUIDescriptor& descriptor);
        void reset();
        void prepare(GPUCommandBuffer cmdbuffer);
        void render(GPUCommandBuffer cmdbuffer, GPUTextureViewHandle backbuffer);
        void update();
        void resize();
        void destroy();
        void new_frame();
        void end_frame();

        auto create_texture(GPUTextureHandle texture, GPUTextureViewHandle view) -> GUITextureHandle;
        void delete_texture(GUITextureHandle texid);

        void begin_render_pass(GPUCommandBuffer cmdbuffer, GPUTextureViewHandle backbuffer) const;
        void end_render_pass(GPUCommandBuffer cmdbuffer) const;

        // imgui context is initialized inside engine DLLs.
        // user application needs the same context in order to use ImGui.
        auto context() const -> ImGuiContext*;

    private:
        void init_imgui_setup(const GUIDescriptor& descriptor);
        void init_config_flags(const GUIDescriptor& descriptor);
        void init_backend_flags(const GUIDescriptor& descriptor);
        void init_platform_data(const GUIDescriptor& descriptor);
        void init_pipeline_data(const GUIDescriptor& descriptor);
        void init_renderer_data(const GUIDescriptor& descriptor);
        void init_viewport_data(const GUIDescriptor& descriptor);
        void init_dummy_texture();
        void init_imgui_font(float font_size);

        void setup_render_state(GPUCommandBuffer cmdbuffer, ImDrawData* draw_data, int width, int height);

        void update_key_state(ImGuiIO& io, const GUIWindowContext& ctx);
        void update_mouse_state(ImGuiIO& io, const GUIWindowContext& ctx);
        void update_viewport_state(ImGuiIO& io, const GUIWindowContext& ctx);
        void update_monitor_state();

        GUIDescriptor        descriptor    = {};
        ImGuiContext*        imgui_context = nullptr;
        Own<GUIPlatformData> platform_data = nullptr;
        Own<GUIPipelineData> pipeline_data = nullptr;
        Own<GUIRendererData> renderer_data = nullptr;
        Vector<MonitorInfo>  monitors;
    };

} // namespace lyra::imgui

#endif // LYRA_LIBRARY_RENDER_GUI_RENDERER_H
