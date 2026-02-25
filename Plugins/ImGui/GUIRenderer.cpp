// library headers
#include <Lyra/Common/Plugin.h>
#include <Lyra/Common/Assert.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Render/RHIInits.h>
#include <Lyra/Render/RHITypes.h>

// local headers
#include "GUIRenderer.h"

// resources
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(imgui);

using namespace lyra;

static Logger logger = create_logger("ImGui", LogLevel::trace);

Logger get_logger()
{
    return logger;
}

namespace ImGui
{
    extern ImGuiIO& GetIO(ImGuiContext*);
}

static GUIRendererData* imgui_make_renderer(uint frame_count)
{
    auto renderer = new GUIRendererData();
    renderer->ibuffers.resize(frame_count);
    renderer->vbuffers.resize(frame_count);
    return renderer;
}

static GPUBuffer imgui_create_buffer(uint size, GPUBufferUsageFlags usages)
{
    auto& device = RHI::get_current_device();
    return execute([&]() {
        GPUBufferDescriptor desc{};
        desc.size               = size;
        desc.usage              = usages;
        desc.mapped_at_creation = false;
        return device.create_buffer(desc);
    });
}

static GPUBuffer imgui_prepare_buffer(GPUBuffer buffer, GUIRendererData* renderer_data, uint size, GPUBufferUsageFlags usages)
{
    // check if buffer is valid
    if (!buffer.handle.valid())
        return imgui_create_buffer(size, usages);

    // check if buffer is big enough
    if (buffer.size < size) {
        renderer_data->garbage_buffers.push_back(GUIGarbageBuffer{buffer, 0});
        return imgui_create_buffer(size, usages);
    }

    // keep using the original buffer
    return buffer;
}

static void imgui_create_vertex_buffers(GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, ImDrawData* draw_data)
{
    // no vertex/index data
    if (draw_data->TotalVtxCount == 0) return;

    auto& vbuffer = renderer_data->vbuffers.at(pipeline_data->frame_index);
    auto& ibuffer = renderer_data->ibuffers.at(pipeline_data->frame_index);

    // create/resize index buffer on the fly
    ibuffer = imgui_prepare_buffer(
        ibuffer,
        renderer_data,
        draw_data->TotalIdxCount * sizeof(ImDrawIdx),
        GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE);

    // create/resize vertex buffer on the fly
    vbuffer = imgui_prepare_buffer(
        vbuffer,
        renderer_data,
        draw_data->TotalVtxCount * sizeof(ImDrawVert),
        GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE);

    ibuffer.map(GPUMapMode::WRITE);
    vbuffer.map(GPUMapMode::WRITE);

    // copy index/vertex from all cmdlists
    auto idx_dst = ibuffer.get_mapped_range<ImDrawIdx>().data;
    auto vtx_dst = vbuffer.get_mapped_range<ImDrawVert>().data;
    for (const ImDrawList* draw_list : draw_data->CmdLists) {
        std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += draw_list->VtxBuffer.Size;
        idx_dst += draw_list->IdxBuffer.Size;
    }

    ibuffer.unmap();
    vbuffer.unmap();
}

static GPUBindGroup imgui_create_texture_descriptor(GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, GUITextureManager::handle texid)
{
    auto& device = RHI::get_current_device();

    auto& texinfo = renderer_data->textures.at(texid);
    if (texinfo.bindgroup.valid())
        return texinfo.bindgroup;

    Array<GPUBindGroupEntry, 2> entries;

    entries.at(0).binding = 0;
    entries.at(0).index   = 0;
    entries.at(0).type    = GPUResourceType::TEXTURE;
    entries.at(0).texture = texinfo.view;

    entries.at(1).binding = 1;
    entries.at(1).index   = 0;
    entries.at(1).type    = GPUResourceType::SAMPLER;
    entries.at(1).sampler = renderer_data->sampler;

    texinfo.bindgroup = execute([&]() {
        GPUBindGroupDescriptor desc{};
        desc.heap    = renderer_data->heap;
        desc.layout  = pipeline_data->blayouts.at(0);
        desc.entries = entries;
        return device.create_bind_group(desc);
    });
    return texinfo.bindgroup;
}

static void imgui_create_texture(GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, ImTextureData* tex)
{
    auto& device = RHI::get_current_device();

    auto texture = execute([&]() {
        GPUTextureDescriptor desc{};
        desc.dimension       = GPUTextureDimension::x2D;
        desc.format          = tex->Format == ImTextureFormat_RGBA32
                                   ? GPUTextureFormat::RGBA8UNORM
                                   : GPUTextureFormat::R8UNORM;
        desc.size.width      = tex->Width;
        desc.size.height     = tex->Height;
        desc.size.depth      = 1;
        desc.array_layers    = 1;
        desc.mip_level_count = 1;
        desc.sample_count    = 1;
        desc.usage           = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
        return device.create_texture(desc);
    });

    GUITexture texinfo{};
    texinfo.texture = texture;
    texinfo.view    = texture.create_view();

    auto texid = renderer_data->textures.add(texinfo);
    tex->SetTexID(as_type<ImTextureID>(texid));
}

static void imgui_update_texture(GPUCommandBuffer cmdbuffer, GUIRendererData* renderer_data, ImTextureData* tex)
{
    uint alignment = RHI::get_current_adapter().properties.texture_row_pitch_alignment;
    uint row_pitch = (tex->GetPitch() + alignment - 1) & ~(alignment - 1);
    uint buf_size  = row_pitch * tex->Height;

    // copy from texture data to staging buffer
    auto staging = imgui_create_buffer(buf_size, GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE);
    staging.map(GPUMapMode::WRITE);
    auto mapped = staging.get_mapped_range();
    for (int i = 0; i < tex->Height; i++) {
        uint8_t* dst = mapped.data + row_pitch * i;
        uint8_t* src = (uint8_t*)tex->GetPixels() + tex->GetPitch() * i;
        std::memcpy(dst, src, tex->GetPitch());
    }
    staging.unmap();

    auto  texid   = as_type<GUITextureManager::handle>(tex->GetTexID());
    auto& texture = renderer_data->textures.at(texid);

    GPUTexelCopyBufferInfo source{};
    source.buffer         = staging;
    source.bytes_per_row  = tex->GetPitch();
    source.offset         = 0;
    source.rows_per_image = tex->Height;

    GPUTexelCopyTextureInfo dest{};
    dest.texture   = texture.texture;
    dest.aspect    = GPUTextureAspect::COLOR;
    dest.mip_level = 0;
    dest.origin    = {0, 0, 0};

    GPUExtent3D copy_size{};
    copy_size.width  = tex->Width;
    copy_size.height = tex->Height;
    copy_size.depth  = 1;

    // copy from staging buffer to texture
    cmdbuffer.resource_barrier(state_transition(texture.texture, undefined_state(), copy_dst_state()));
    cmdbuffer.copy_buffer_to_texture(source, dest, copy_size);
    cmdbuffer.resource_barrier(state_transition(texture.texture, copy_dst_state(), shader_resource_state(GPUBarrierSync::PIXEL_SHADING)));

    // deferred deletion of the staging buffer, because this buffer is still used before the command buffer completes.
    renderer_data->garbage_buffers.push_back(GUIGarbageBuffer{staging, 0});

    // update the texture status back to OK (required by ImGui)
    tex->SetStatus(ImTextureStatus_OK);
}

static void imgui_delete_texture(GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, GUITextureManager::handle texid)
{
    // deferred deletion of texture, because the current texture/view might still be used in some frames in flight
    auto& texinfo = renderer_data->textures.at(texid);
    if (texinfo.valid()) {
        renderer_data->textures.remove(texid);
        renderer_data->garbage_textures.push_back(GUIGarbageTexture{texinfo, pipeline_data->frame_count});
    }
}

static void imgui_delete_texture(GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, ImTextureData* tex)
{
    auto texid = as_type<GUITextureManager::handle>(tex->GetTexID());
    imgui_delete_texture(pipeline_data, renderer_data, texid);

    // reset texture id
    tex->SetTexID(ImTextureID_Invalid);
    tex->SetStatus(ImTextureStatus_Destroyed);
}

static void imgui_process_texture(GPUCommandBuffer cmdbuffer, GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, ImTextureData* tex)
{
    // nothing to do
    if (tex->Status == ImTextureStatus_OK) return;

    // create texture if necessary
    if (tex->Status == ImTextureStatus_WantCreate)
        imgui_create_texture(pipeline_data, renderer_data, tex);

    // update texture if necessary
    if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
        imgui_update_texture(cmdbuffer, renderer_data, tex);

    // delete texture if necessary
    if (tex->Status == ImTextureStatus_WantDestroy)
        imgui_delete_texture(pipeline_data, renderer_data, tex);
}

static void imgui_prepare(GPUCommandBuffer cmdbuffer, GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, ImDrawData* draw_data)
{
    // catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                imgui_process_texture(cmdbuffer, pipeline_data, renderer_data, tex);
}

static void imgui_setup_render_state(GPUCommandBuffer cmdbuffer, GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, ImDrawData* draw_data, int width, int height)
{
    // bind pipeline
    cmdbuffer.set_pipeline(pipeline_data->pipeline);

    // bind viewport
    cmdbuffer.set_viewport(0, 0, (float)width, (float)height);

    // bind index/vertex buffers
    if (draw_data->TotalVtxCount > 0) {
        auto& vbuffer = renderer_data->vbuffers.at(pipeline_data->frame_index);
        auto& ibuffer = renderer_data->ibuffers.at(pipeline_data->frame_index);
        auto  iformat = sizeof(ImDrawIdx) == 2 ? GPUIndexFormat::UINT16 : GPUIndexFormat::UINT32;
        cmdbuffer.set_vertex_buffer(0, vbuffer);
        cmdbuffer.set_index_buffer(ibuffer, iformat);
    }

    // setup scale and translation:
    // our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos + data_data->DisplaySize (bottom right).
    // DisplayPos is (0,0) for single viewport apps.
    {
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

        glm::mat4 mvp = glm::ortho(L, R, B, T);
        cmdbuffer.set_push_constants(GPUShaderStage::VERTEX, 0, mvp);
    }
}

static void imgui_begin_render_pass(GPUCommandBuffer cmdbuffer, GPUTextureViewHandle backbuffer)
{
    // color attachments
    auto color_attachment        = GPURenderPassColorAttachment{};
    color_attachment.clear_value = GPUColor{0.0f, 0.0f, 0.0f, 0.0f};
    color_attachment.load_op     = GPULoadOp::CLEAR;
    color_attachment.store_op    = GPUStoreOp::STORE;
    color_attachment.view        = backbuffer;

    // render pass info
    auto render_pass                     = GPURenderPassDescriptor{};
    render_pass.color_attachments        = color_attachment;
    render_pass.depth_stencil_attachment = {};

    cmdbuffer.begin_render_pass(render_pass);
}

static void imgui_end_render_pass(GPUCommandBuffer cmdbuffer)
{
    cmdbuffer.end_render_pass();
}

static void imgui_render(GPUCommandBuffer cmdbuffer, GPUTextureViewHandle backbuffer, GUIPipelineData* pipeline_data, GUIRendererData* renderer_data, ImDrawData* draw_data)
{
    // avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width  = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0) return;

    cmdbuffer.push_debug_group("ImGui");

    // refresh index/vertex buffers
    imgui_create_vertex_buffers(pipeline_data, renderer_data, draw_data);

    // initial render state setup
    imgui_setup_render_state(cmdbuffer, pipeline_data, renderer_data, draw_data, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off   = draw_data->DisplayPos;       // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // render command lists
    // (because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (const ImDrawList* draw_list : draw_data->CmdLists) {
        for (const ImDrawCmd& draw_cmd : draw_list->CmdBuffer) {
            // project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_min((draw_cmd.ClipRect.x - clip_off.x) * clip_scale.x, (draw_cmd.ClipRect.y - clip_off.y) * clip_scale.y);
            ImVec2 clip_max((draw_cmd.ClipRect.z - clip_off.x) * clip_scale.x, (draw_cmd.ClipRect.w - clip_off.y) * clip_scale.y);

            // clamp to viewport as set_scissors() won't accept values that are off bounds
            if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
            if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
            if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            // apply scissor/clipping rectangle
            cmdbuffer.set_scissor_rect(
                static_cast<GPUIntegerCoordinate>(clip_min.x),
                static_cast<GPUIntegerCoordinate>(clip_min.y),
                static_cast<GPUIntegerCoordinate>(clip_max.x - clip_min.x),
                static_cast<GPUIntegerCoordinate>(clip_max.y - clip_min.y));

            // bind texture
            auto texid   = as_type<GUITextureManager::handle>(draw_cmd.GetTexID());
            auto texinfo = imgui_create_texture_descriptor(pipeline_data, renderer_data, texid);
            cmdbuffer.set_bind_group(0, texinfo);

            // draw
            cmdbuffer.draw_indexed(
                draw_cmd.ElemCount, 1,
                draw_cmd.IdxOffset + global_idx_offset,
                draw_cmd.VtxOffset + global_vtx_offset,
                0);
        }

        global_vtx_offset += draw_list->VtxBuffer.Size;
        global_idx_offset += draw_list->IdxBuffer.Size;
    }

    // reset scissor rect for future commands
    cmdbuffer.set_scissor_rect(0, 0, fb_width, fb_height);
    cmdbuffer.pop_debug_group();
}

static void imgui_create_window_context(GUIPlatformData* platform_data, WindowHandle window, ImGuiContext* context)
{
    auto& window_contexts = platform_data->window_contexts;
    window_contexts.emplace_back<GUIWindowContext>({window, context});
}

static void imgui_delete_window_context(GUIPlatformData* platform_data, WindowHandle window)
{
    auto& window_contexts = platform_data->window_contexts;
    for (auto it = window_contexts.begin(); it != window_contexts.end();) {
        if (it->window.window == window.window) {
            it = window_contexts.erase(it);
        } else {
            it++;
        }
    }
}

static WindowHandle& platform_get_window_handle(ImGuiViewport* viewport)
{
    return (reinterpret_cast<GUIViewportData*>(viewport->RendererUserData))->window;
}

static void platform_create_window(ImGuiViewport* viewport)
{
    WindowDescriptor desc{};
    desc.title  = "Viewport Window";
    desc.width  = static_cast<uint>(viewport->Size.x);
    desc.height = static_cast<uint>(viewport->Size.y);
    desc.flags  = 0;

    WindowHandle window;
    WSI::api()->create_window(desc, window);

    // used to propagate the shared renderer data
    auto main_viewport = ImGui::GetMainViewport();
    auto pipeline_data = reinterpret_cast<GUIViewportData*>(main_viewport->RendererUserData)->pipeline;
    auto platform_data = reinterpret_cast<GUIPlatformData*>(main_viewport->PlatformUserData);

    // surface / swapchain
    auto surface = execute([&]() {
        auto desc         = GPUSurfaceDescriptor{};
        desc.label        = "viewport_swapchain";
        desc.window       = window;
        desc.alpha_mode   = GPUCompositeAlphaMode::Opaque;
        desc.color_space  = GPUColorSpace::SRGB;
        desc.present_mode = GPUPresentMode::Fifo;
        desc.frames       = pipeline_data->frame_count;

        GPUSurfaceHandle surface;
        RHI::api()->create_surface(surface, desc);
        return GPUSurface(surface);
    });

    // viewport data
    GUIViewportData* vd = new GUIViewportData{};
    vd->pipeline        = pipeline_data;
    vd->renderer        = imgui_make_renderer(vd->pipeline->frame_count);
    vd->surface         = surface;
    vd->window          = window;
    vd->owned           = true;

    viewport->RendererUserData      = vd;
    viewport->PlatformUserData      = main_viewport->PlatformUserData;
    viewport->PlatformHandle        = vd->window.window;
    viewport->PlatformHandleRaw     = vd->window.window;
    viewport->PlatformWindowCreated = true;

    ImGuiContext* context = ImGui::GetCurrentContext();
    imgui_create_window_context(platform_data, window, context);
}

static void platform_delete_window(ImGuiViewport* viewport)
{
    auto pd = reinterpret_cast<GUIPlatformData*>(viewport->PlatformUserData);
    auto vd = reinterpret_cast<GUIViewportData*>(viewport->RendererUserData);
    if (vd == nullptr) return;
    pd->garbage_viewports.push_back(vd);

    imgui_delete_window_context(pd, vd->window);

    viewport->RendererUserData      = nullptr;
    viewport->PlatformUserData      = nullptr;
    viewport->PlatformHandle        = nullptr;
    viewport->PlatformHandleRaw     = nullptr;
    viewport->PlatformWindowCreated = false;
}

static void platform_show_window(ImGuiViewport* viewport)
{
    WSI::api()->show_window(platform_get_window_handle(viewport));
}

static void platform_render_window(ImGuiViewport* viewport, void*)
{
    GUIViewportData* vd = reinterpret_cast<GUIViewportData*>(viewport->RendererUserData);

    // command buffer
    auto cmdbuffer = execute([&]() {
        auto& device = RHI::get_current_device();
        auto  desc   = GPUCommandBufferDescriptor{};
        desc.queue   = GPUQueueType::DEFAULT;
        return device.create_command_buffer(desc);
    });

    // surface texture
    auto backbuffer = GPUSurface(vd->surface).get_current_texture();

    // synchronization
    cmdbuffer.wait(backbuffer.available, GPUBarrierSync::PIXEL_SHADING);
    cmdbuffer.signal(backbuffer.complete, GPUBarrierSync::RENDER_TARGET);

    // command recording
    cmdbuffer.resource_barrier(state_transition(backbuffer.texture, undefined_state(), color_attachment_state()));
    imgui_prepare(cmdbuffer, vd->pipeline, vd->renderer, viewport->DrawData);
    imgui_begin_render_pass(cmdbuffer, backbuffer);
    imgui_render(cmdbuffer, backbuffer, vd->pipeline, vd->renderer, viewport->DrawData);
    imgui_end_render_pass(cmdbuffer);
    cmdbuffer.resource_barrier(state_transition(backbuffer.texture, color_attachment_state(), present_src_state()));
    cmdbuffer.submit();

    // swapchain presentation
    backbuffer.present();
}

static void platform_swap_buffers(ImGuiViewport* viewport, void*)
{
    // NOTE: our implementation does not rely on window system for double buffering
}

static void platform_set_window_pos(ImGuiViewport* viewport, ImVec2 pos)
{
    WSI::api()->set_window_pos(
        platform_get_window_handle(viewport),
        static_cast<uint>(std::max(pos.x, 0.0f)),
        static_cast<uint>(std::max(pos.y, 0.0f)));
}

static ImVec2 platform_get_window_pos(ImGuiViewport* viewport)
{
    int xpos, ypos;
    WSI::api()->get_window_pos(platform_get_window_handle(viewport), xpos, ypos);
    ImVec2 pos(static_cast<float>(xpos), static_cast<float>(ypos));
    return pos;
}

static void platform_set_window_size(ImGuiViewport* viewport, ImVec2 size)
{
    uint w = static_cast<uint>(size.x);
    uint h = static_cast<uint>(size.y);
    WSI::api()->set_window_size(platform_get_window_handle(viewport), w, h);
}

static ImVec2 platform_get_window_size(ImGuiViewport* viewport)
{
    uint xsiz, ysiz;
    WSI::api()->get_window_size(platform_get_window_handle(viewport), xsiz, ysiz);
    return ImVec2(static_cast<float>(xsiz), static_cast<float>(ysiz));
}

static ImVec2 platform_get_framebuffer_scale(ImGuiViewport* viewport)
{
    float xscale, yscale;
    WSI::api()->get_framebuffer_scale(platform_get_window_handle(viewport), xscale, yscale);
    return ImVec2(xscale, yscale);
}

static void platform_set_window_title(ImGuiViewport* viewport, CString title)
{
    WSI::api()->set_window_title(platform_get_window_handle(viewport), title);
}

static void platform_set_window_focus(ImGuiViewport* viewport)
{
    WSI::api()->set_window_focus(platform_get_window_handle(viewport));
}

static bool platform_get_window_focus(ImGuiViewport* viewport)
{
    return WSI::api()->get_window_focus(platform_get_window_handle(viewport));
}

static bool platform_get_window_minimized(ImGuiViewport* viewport)
{
    return WSI::api()->get_window_minimized(platform_get_window_handle(viewport));
}

static void platform_set_window_alpha(ImGuiViewport* viewport, float alpha)
{
    return WSI::api()->set_window_alpha(platform_get_window_handle(viewport), alpha);
}

static ImGuiKey to_imgui_key_button(KeyButton button)
{
    // clang-format off
    switch (button) {
        // misc keys
        case KeyButton::TAB:           return ImGuiKey_Tab;
        case KeyButton::ESC:           return ImGuiKey_Escape;
        case KeyButton::SPACE:         return ImGuiKey_Space;
        case KeyButton::BACKSPACE:     return ImGuiKey_Backspace;
        case KeyButton::DEL:           return ImGuiKey_Delete;
        case KeyButton::ENTER:         return ImGuiKey_Enter;
        case KeyButton::PAGE_UP:       return ImGuiKey_PageUp;
        case KeyButton::PAGE_DOWN:     return ImGuiKey_PageDown;
        case KeyButton::HOME:          return ImGuiKey_Home;
        case KeyButton::END:           return ImGuiKey_End;
        case KeyButton::PAUSE:         return ImGuiKey_Pause;
        case KeyButton::NUM_LOCK:      return ImGuiKey_NumLock;
        case KeyButton::CAPS_LOCK:     return ImGuiKey_CapsLock;
        case KeyButton::SCROLL_LOCK:   return ImGuiKey_ScrollLock;

        // modifier keys
        case KeyButton::ALT:           return ImGuiKey_ModAlt;
        case KeyButton::CTRL:          return ImGuiKey_ModCtrl;
        case KeyButton::SHIFT:         return ImGuiKey_ModShift;
        case KeyButton::SUPER:         return ImGuiKey_ModSuper;

        // ASCII keys
        case KeyButton::A:             return ImGuiKey_A;
        case KeyButton::B:             return ImGuiKey_B;
        case KeyButton::C:             return ImGuiKey_C;
        case KeyButton::D:             return ImGuiKey_D;
        case KeyButton::E:             return ImGuiKey_E;
        case KeyButton::F:             return ImGuiKey_F;
        case KeyButton::G:             return ImGuiKey_G;
        case KeyButton::H:             return ImGuiKey_H;
        case KeyButton::I:             return ImGuiKey_I;
        case KeyButton::J:             return ImGuiKey_J;
        case KeyButton::K:             return ImGuiKey_K;
        case KeyButton::L:             return ImGuiKey_L;
        case KeyButton::M:             return ImGuiKey_M;
        case KeyButton::N:             return ImGuiKey_N;
        case KeyButton::O:             return ImGuiKey_O;
        case KeyButton::P:             return ImGuiKey_P;
        case KeyButton::Q:             return ImGuiKey_Q;
        case KeyButton::R:             return ImGuiKey_R;
        case KeyButton::S:             return ImGuiKey_S;
        case KeyButton::T:             return ImGuiKey_T;
        case KeyButton::U:             return ImGuiKey_U;
        case KeyButton::V:             return ImGuiKey_V;
        case KeyButton::W:             return ImGuiKey_W;
        case KeyButton::X:             return ImGuiKey_X;
        case KeyButton::Y:             return ImGuiKey_Y;
        case KeyButton::Z:             return ImGuiKey_Z;

        // arrow keys
        case KeyButton::UP:            return ImGuiKey_UpArrow;
        case KeyButton::DOWN:          return ImGuiKey_DownArrow;
        case KeyButton::LEFT:          return ImGuiKey_LeftArrow;
        case KeyButton::RIGHT:         return ImGuiKey_RightArrow;

        // numerical keys
        case KeyButton::D0:            return ImGuiKey_0;
        case KeyButton::D1:            return ImGuiKey_1;
        case KeyButton::D2:            return ImGuiKey_2;
        case KeyButton::D3:            return ImGuiKey_3;
        case KeyButton::D4:            return ImGuiKey_4;
        case KeyButton::D5:            return ImGuiKey_5;
        case KeyButton::D6:            return ImGuiKey_6;
        case KeyButton::D7:            return ImGuiKey_7;
        case KeyButton::D8:            return ImGuiKey_8;
        case KeyButton::D9:            return ImGuiKey_9;

        // function keys
        case KeyButton::F1:            return ImGuiKey_F1;
        case KeyButton::F2:            return ImGuiKey_F2;
        case KeyButton::F3:            return ImGuiKey_F3;
        case KeyButton::F4:            return ImGuiKey_F4;
        case KeyButton::F5:            return ImGuiKey_F5;
        case KeyButton::F6:            return ImGuiKey_F6;
        case KeyButton::F7:            return ImGuiKey_F7;
        case KeyButton::F8:            return ImGuiKey_F8;
        case KeyButton::F9:            return ImGuiKey_F9;
        case KeyButton::F10:           return ImGuiKey_F10;
        case KeyButton::F11:           return ImGuiKey_F11;
        case KeyButton::F12:           return ImGuiKey_F12;

        case KeyButton::APOSTROPHE:    return ImGuiKey_Apostrophe;
        case KeyButton::COMMA:         return ImGuiKey_Comma;
        case KeyButton::MINUS:         return ImGuiKey_Minus;
        case KeyButton::PERIOD:        return ImGuiKey_Period;
        case KeyButton::SLASH:         return ImGuiKey_Slash;
        case KeyButton::BACKSLASH:     return ImGuiKey_Backslash;
        case KeyButton::SEMICOLON:     return ImGuiKey_Semicolon;
        case KeyButton::EQUAL:         return ImGuiKey_Equal;
        case KeyButton::LEFT_BRACKET:  return ImGuiKey_LeftBracket;
        case KeyButton::RIGHT_BRACKET: return ImGuiKey_RightBracket;
        case KeyButton::GRAVE_ACCENT:  return ImGuiKey_GraveAccent;

        default:
            assert(!!!"Invalid keyboard button!");
             return ImGuiKey_None;
    }
    // clang-format on
}

static ImGuiMouseButton to_imgui_mouse_button(MouseButton button)
{
    // clang-format off
    switch (button) {
        case MouseButton::LEFT:   return ImGuiMouseButton_Left;
        case MouseButton::RIGHT:  return ImGuiMouseButton_Right;
        case MouseButton::MIDDLE: return ImGuiMouseButton_Middle;
        default:
            assert(!!!"Invalid mouse button!");
             return ImGuiMouseButton_Left;
    }
    // clang-format on
}

#pragma region GUIRenderer
GUIRenderer::GUIRenderer(const GUIDescriptor& descriptor) : descriptor(descriptor)
{
    init(descriptor);
}

void GUIRenderer::init(const GUIDescriptor& descriptor)
{
    init_imgui_setup(descriptor);
    init_config_flags(descriptor);
    init_backend_flags(descriptor);
    init_pipeline_data(descriptor);
    init_renderer_data(descriptor);
    init_platform_data(descriptor);
    init_viewport_data(descriptor);
    init_dummy_texture();
    init_imgui_font("Fonts/Font.ttf", descriptor.font_size);
}

void GUIRenderer::reset()
{
    assert(platform_data && "ImGui platform data has not been initialized!");
    assert(renderer_data && "ImGui renderer data has not been initialized!");
    assert(pipeline_data && "ImGui pipeline data has not been initialized!");

    uint frame_count = pipeline_data->frame_count;

    // reclaim unused buffers
    auto& garbage_buffers = renderer_data->garbage_buffers;
    for (auto it = garbage_buffers.begin(); it != garbage_buffers.end();) {
        auto& garbage = *it;
        if (garbage.should_remove(frame_count)) {
            garbage.object.destroy();
            it = garbage_buffers.erase(it);
        } else {
            it++;
        }
    }

    // reclaim unused textures
    auto& garbage_textures = renderer_data->garbage_textures;
    for (auto it = garbage_textures.begin(); it != garbage_textures.end();) {
        auto& garbage = *it;
        if (garbage.should_remove(frame_count)) {
            if (garbage.object.texture.valid()) garbage.object.texture.destroy();
            if (garbage.object.view.valid()) garbage.object.view.destroy();
            it = garbage_textures.erase(it);
        } else {
            it++;
        }
    }

    // update frame index
    pipeline_data->frame_index = (pipeline_data->frame_index + 1) % pipeline_data->frame_count;

    // update monitor state
    update_monitor_state();
}

void GUIRenderer::prepare(GPUCommandBuffer cmdbuffer)
{
    auto draw_data = ImGui::GetDrawData();
    imgui_prepare(cmdbuffer, pipeline_data.get(), renderer_data.get(), draw_data);
}

void GUIRenderer::render(GPUCommandBuffer cmdbuffer, GPUTextureViewHandle backbuffer)
{
    auto draw_data = ImGui::GetDrawData();
    imgui_render(cmdbuffer, backbuffer, pipeline_data.get(), renderer_data.get(), draw_data);
}

void GUIRenderer::update()
{
    // clean-up garbage viewports
    if (!platform_data->garbage_viewports.empty()) {
        RHI::api()->wait_idle();
        for (auto viewport : platform_data->garbage_viewports) {
            if (viewport->owned) {
                RHI::api()->delete_surface(viewport->surface);
                WSI::api()->delete_window(viewport->window);
            }
            delete viewport;
        }
        platform_data->garbage_viewports.clear();
    }

    // update window inputs
    for (auto& window : platform_data->window_contexts) {
        WSI::api()->query_input_events(window.window, window.events);

        ImGuiIO& io = ImGui::GetIO(window.context);
        update_viewport_state(io, window);
        update_mouse_state(io, window);
        update_key_state(io, window);
    }
}

void GUIRenderer::resize()
{
    uint width, height;
    WSI::api()->get_window_size(descriptor.window, width, height);

    float fb_xscale, fb_yscale;
    WSI::api()->get_framebuffer_scale(descriptor.window, fb_xscale, fb_yscale);

    float dpi_xscale, dpi_yscale;
    WSI::api()->get_content_scale(descriptor.window, dpi_xscale, dpi_yscale);

    auto& io                   = ImGui::GetIO();
    io.DisplaySize             = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.DisplayFramebufferScale = ImVec2(fb_xscale, fb_yscale);
    io.ConfigDpiScaleFonts     = true; // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true; // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    ImGuiStyle& style  = ImGui::GetStyle();
    style.FontScaleDpi = std::max(dpi_xscale, dpi_yscale);
}

void GUIRenderer::destroy()
{
    RHI::api()->wait_idle();

    // unset all backend user data (pointers will be reclaimed by unique_ptr)
    ImGuiIO& io                = ImGui::GetIO();
    io.BackendFlags            = 0;
    io.BackendPlatformUserData = nullptr;
    io.BackendRendererUserData = nullptr;
    imgui_delete_window_context(platform_data.get(), platform_data->primary_window);

    // unset primary viewport data
    auto viewport = ImGui::GetMainViewport();
    delete (GUIViewportData*)(viewport->RendererUserData);
    viewport->RendererUserData = nullptr;
    viewport->PlatformUserData = nullptr;

    // manually clean up rest of the viewports
    auto& window_contexts = platform_data->window_contexts;
    for (auto& window_context : window_contexts)
        WSI::api()->delete_window(window_context.window);

    // clean up Dear ImGui context
    ImGui::DestroyPlatformWindows();
    ImGui::DestroyContext();
}

void GUIRenderer::new_frame()
{
    ImGui::NewFrame();
}

void GUIRenderer::end_frame()
{
    // ImGui::Render() will automatically call ImGui::EndFrame
    ImGui::Render();
}

void GUIRenderer::begin_render_pass(GPUCommandBuffer cmdbuffer, GPUTextureViewHandle backbuffer) const
{
    imgui_begin_render_pass(cmdbuffer, backbuffer);
}

void GUIRenderer::end_render_pass(GPUCommandBuffer cmdbuffer) const
{
    imgui_end_render_pass(cmdbuffer);
}

GUITextureHandle GUIRenderer::create_texture(GPUTextureHandle texture, GPUTextureViewHandle view)
{
    GUITexture texinfo{};
    texinfo.view.handle    = view;
    texinfo.texture.handle = texture;

    auto handle = renderer_data->textures.add(texinfo);
    return GUITextureHandle::create(handle);
}

void GUIRenderer::delete_texture(GUITextureHandle texid)
{
    auto handle = as_type<GUITextureManager::handle>(texid);
    imgui_delete_texture(pipeline_data.get(), renderer_data.get(), handle);
}

ImGuiContext* GUIRenderer::context() const
{
    return imgui_context;
}

void GUIRenderer::init_imgui_setup(const GUIDescriptor& descriptor)
{
    // setup Dear ImGui context
    IMGUI_CHECKVERSION();
    imgui_context = ImGui::CreateContext();

    // setup display size, framebuffer scale, font scale, etc
    resize();
}

void GUIRenderer::init_config_flags(const GUIDescriptor& descriptor)
{
    ImGuiIO& io = ImGui::GetIO();

    // configure window docking
    if (descriptor.docking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigDockingWithShift = true;
    }

    // configure viewports for multi-window support
    if (descriptor.viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoTaskBarIcon = true;
        io.ConfigDpiScaleViewports      = true;
    }

    // font scale
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
}

void GUIRenderer::init_backend_flags(const GUIDescriptor& descriptor)
{
    ImGuiIO& io = ImGui::GetIO();

    // platform backend flags
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // we can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // we can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // we can call io.AddMouseViewportEvent() with correct data (optional)

    // renderer backend flags
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // we can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // we can honor ImGuiPlatformIO::Textures[] requests during render

    // viewport backend flags
    if (descriptor.viewports) {
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
        io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // we can create multi-viewports on the Platform side (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // we can call io.AddMouseViewportEvent() with correct data (optional)
    }
}

void GUIRenderer::init_platform_data(const GUIDescriptor& descriptor)
{
    ImGuiIO& io = ImGui::GetIO();
    assert(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    platform_data                 = std::make_unique<GUIPlatformData>();
    platform_data->primary_window = descriptor.window;
    platform_data->context        = ImGui::GetCurrentContext();
    platform_data->elapsed        = 0.0f;
    assert(platform_data->primary_window.window != nullptr && "Expect a valid handle for primary window!");

    // initialize platform backend data
    io.BackendPlatformUserData = platform_data.get();
    io.BackendPlatformName     = "lyra-window";

    ImGuiContext* context = ImGui::GetCurrentContext();
    imgui_create_window_context(platform_data.get(), descriptor.window, context);
}

void GUIRenderer::init_pipeline_data(const GUIDescriptor& descriptor)
{
    ImGuiIO& io = ImGui::GetIO();
    assert(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    auto& device   = RHI::get_current_device();
    auto  surface  = GPUSurface(descriptor.surface);
    auto  compiler = Compiler(descriptor.compiler);

    pipeline_data              = std::make_unique<GUIPipelineData>();
    pipeline_data->frame_count = surface.get_image_count();
    pipeline_data->frame_index = 0;

    // shader source
    auto fs     = cmrc::imgui::get_filesystem();
    auto file   = fs.open("Shaders/GUIShader.slang");
    auto source = String(file.begin(), file.end());

    // shader module
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "imgui";
        desc.path   = "imgui.slang";
        desc.source = source.c_str();
        return compiler.compile(desc);
    });

    // shader reflection
    auto refl = compiler.reflect({
        {*module, "vsmain"},
        {*module, "fsmain"},
    });

    // vertex shader
    pipeline_data->vshader = execute([&]() {
        auto code  = module->get_shader_blob("vsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "imgui_vertex_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    // fragment shader
    pipeline_data->fshader = execute([&]() {
        auto code  = module->get_shader_blob("fsmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "imgui_fragment_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    // bind group layouts
    for (auto& layout : refl->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(layout);
        pipeline_data->blayouts.push_back(blayout.handle);
    }

    // pipeline layout
    pipeline_data->playout = execute([&]() {
        auto desc                 = GPUPipelineLayoutDescriptor{};
        desc.label                = "imgui_pipeline_layout";
        desc.bind_group_layouts   = pipeline_data->blayouts;
        desc.push_constant_ranges = refl->get_push_constant_ranges();
        return device.create_pipeline_layout(desc);
    });

    // pipeline state
    pipeline_data->pipeline = execute([&]() {
        auto attributes = refl->get_vertex_attributes({
            {"pos", offsetof(ImDrawVert, pos)},
            {"uv", offsetof(ImDrawVert, uv)},
            {"color", offsetof(ImDrawVert, col)},
        });

        // slight modification to color vertex attribute (use unorm8x4 instead of f32x4)
        attributes.at(2).format = GPUVertexFormat::UNORM8x4;

        // vertex buffer layout
        GPUVertexBufferLayout buffer{};
        buffer.attributes   = attributes;
        buffer.array_stride = sizeof(ImDrawVert);
        buffer.step_mode    = GPUVertexStepMode::VERTEX;

        // color target
        GPUColorTargetState color_state{};
        color_state.format                 = surface.get_current_format();
        color_state.blend_enable           = true;
        color_state.blend.color.operation  = GPUBlendOperation::ADD;
        color_state.blend.color.src_factor = GPUBlendFactor::SRC_ALPHA;
        color_state.blend.color.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        color_state.blend.alpha.operation  = GPUBlendOperation::ADD;
        color_state.blend.alpha.src_factor = GPUBlendFactor::SRC_ALPHA;
        color_state.blend.alpha.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
        color_state.write_mask             = GPUColorWrite::ALL;

        // pipeline descriptor
        GPURenderPipelineDescriptor desc{};
        desc.label                = "imgui_pipeline";
        desc.layout               = pipeline_data->playout;
        desc.vertex.entry_point   = "vsmain";
        desc.vertex.buffers       = buffer;
        desc.vertex.module        = pipeline_data->vshader;
        desc.fragment.entry_point = "fsmain";
        desc.fragment.module      = pipeline_data->fshader;
        desc.fragment.targets     = color_state;
        desc.multisample.count    = 1;
        desc.primitive.cull_mode  = GPUCullMode::NONE;
        desc.primitive.front_face = GPUFrontFace::CW;
        desc.primitive.topology   = GPUPrimitiveTopology::TRIANGLE_LIST;

        return device.create_render_pipeline(desc);
    });

    // initialize renderer backend data
    io.BackendRendererUserData = pipeline_data.get();
    io.BackendRendererName     = "lyra-renderer";
}

void GUIRenderer::init_renderer_data(const GUIDescriptor& descriptor)
{
    auto& device = RHI::get_current_device();

    renderer_data.reset(imgui_make_renderer(pipeline_data->frame_count));

    // heap
    renderer_data->heap = execute([&]() {
        GPUBindGroupHeapDescriptor desc{};
        desc.page_size = 2048;
        return device.create_bind_group_heap(desc);
    });

    // sampler
    renderer_data->sampler = execute([&]() {
        GPUSamplerDescriptor desc{};
        desc.label          = "imgui_sampler";
        desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
        desc.lod_min_clamp  = 0.0f;
        desc.lod_max_clamp  = 1000.0f;
        desc.compare_enable = false;
        desc.min_filter     = GPUFilterMode::LINEAR;
        desc.mag_filter     = GPUFilterMode::LINEAR;
        desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
        desc.max_anisotropy = 1u;
        return device.create_sampler(desc);
    });
}

void GUIRenderer::init_viewport_data(const GUIDescriptor& descriptor)
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    // query window properties
    uint  width, height;
    float xscale, yscale;
    WSI::api()->get_window_size(descriptor.window, width, height);
    WSI::api()->get_content_scale(descriptor.window, xscale, yscale);

    // setup platform monitors
    update_monitor_state();

    // multi-viewport specific methods
    if (descriptor.viewports) {
        platform_io.Platform_CreateWindow              = platform_create_window;
        platform_io.Platform_DestroyWindow             = platform_delete_window;
        platform_io.Platform_ShowWindow                = platform_show_window;
        platform_io.Platform_SetWindowPos              = platform_set_window_pos;
        platform_io.Platform_GetWindowPos              = platform_get_window_pos;
        platform_io.Platform_SetWindowSize             = platform_set_window_size;
        platform_io.Platform_GetWindowSize             = platform_get_window_size;
        platform_io.Platform_GetWindowFramebufferScale = platform_get_framebuffer_scale;
        platform_io.Platform_SetWindowFocus            = platform_set_window_focus;
        platform_io.Platform_GetWindowFocus            = platform_get_window_focus;
        platform_io.Platform_GetWindowMinimized        = platform_get_window_minimized;
        platform_io.Platform_SetWindowTitle            = platform_set_window_title;
        platform_io.Platform_RenderWindow              = platform_render_window;
        platform_io.Platform_SwapBuffers               = platform_swap_buffers;
        platform_io.Platform_SetWindowAlpha            = platform_set_window_alpha;
    }

    // register main window handle (which is owned by the main application, not by us)
    GUIViewportData* vd = new GUIViewportData{};
    vd->pipeline        = pipeline_data.get();
    vd->renderer        = renderer_data.get();
    vd->surface         = descriptor.surface;
    vd->window          = platform_data->primary_window;
    vd->owned           = false;

    ImGuiViewport* viewport         = ImGui::GetMainViewport();
    viewport->RendererUserData      = vd;
    viewport->PlatformUserData      = platform_data.get();
    viewport->PlatformHandle        = vd->window.window;
    viewport->PlatformHandleRaw     = vd->window.window;
    viewport->PlatformWindowCreated = true;
}

void GUIRenderer::init_dummy_texture()
{
    // NOTE: ImTetxureID(0) is treated as invalid texture,
    // therefore we simply occupy slotmap at index 0, so that
    // we will never receive an invalid texID.

    GUITexture texinfo{};
    texinfo.texture.handle.reset();
    texinfo.view.handle.reset();
    renderer_data->textures.add(texinfo);
}

void GUIRenderer::init_imgui_font(CString filename, float font_size)
{
    ImGuiIO&    io    = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // initialize font size
    style.FontSizeBase = font_size;

    // adjust range for Nerd Font
    static const ImWchar icon_ranges[] = {0xe000, 0xf8ff, 0};

    // configure font properties
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;       // set to false if you manage memory yourself
    font_cfg.GlyphExcludeRanges   = icon_ranges; // exclude icon ranges (avoid font merge issues)

    ImFontConfig icon_cfg;
    icon_cfg.FontDataOwnedByAtlas = false;       // set to false if you manage memory yourself
    icon_cfg.MergeMode            = true;        // merge icons with regular font
    icon_cfg.PixelSnapH           = true;        // optional, can help with pixel alignment
    icon_cfg.GlyphRanges          = icon_ranges; // icons only
    icon_cfg.GlyphMaxAdvanceX     = font_size * +1.5f;
    icon_cfg.GlyphMinAdvanceX     = font_size * +1.5f;
    icon_cfg.GlyphOffset.x        = font_size * -0.25f;

    // font source
    auto file = cmrc::imgui::get_filesystem().open(filename);

    // load the main font from memory
    io.Fonts->AddFontFromMemoryTTF(
        (void*)file.begin(),
        static_cast<int>(file.size()),
        font_size,
        &font_cfg);

    // load the icon font from memory
    io.Fonts->AddFontFromMemoryTTF(
        (void*)file.begin(),
        static_cast<int>(file.size()),
        font_size * 1.25f,
        &icon_cfg);

    io.Fonts->Build();
}

void GUIRenderer::update_key_state(ImGuiIO& io, const GUIWindowContext& ctx)
{
    const auto& query = ctx.events;

    // update key events
    for (uint i = 0; i < query.num_events; i++) {
        const auto& event = query.input_events.at(i);
        if (event.type == WindowInputEvent::Type::KEY_BUTTON)
            io.AddKeyEvent(to_imgui_key_button(event.key_button.button), event.key_button.state == ButtonState::ON);
        if (event.type == WindowInputEvent::Type::KEY_TYPING)
            io.AddInputCharacter(event.key_typing.code);
    }
}

void GUIRenderer::update_mouse_state(ImGuiIO& io, const GUIWindowContext& ctx)
{
    const auto& query = ctx.events;

    // update mouse button events
    for (uint i = 0; i < query.num_events; i++) {
        const auto& event = query.input_events.at(i);
        if (event.type == WindowInputEvent::Type::MOUSE_BUTTON) {
            io.AddMouseButtonEvent(to_imgui_mouse_button(event.mouse_button.button), event.mouse_button.state == ButtonState::ON);
        } else if (event.type == WindowInputEvent::Type::MOUSE_WHEEL) {
            io.AddMouseWheelEvent(event.mouse_wheel.x, event.mouse_wheel.y);
        } else if (event.type == WindowInputEvent::Type::MOUSE_MOVE) {
            // calculate mouse position
            float x = event.mouse_move.xpos;
            float y = event.mouse_move.ypos;

            // account for viewport window offset
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                int xpos, ypos;
                WSI::api()->get_window_pos(ctx.window, xpos, ypos);
                x += static_cast<float>(xpos);
                y += static_cast<float>(ypos);
            }

            io.AddMousePosEvent(x, y);

            // capture mouse focus over viewport
            if (io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport)
                if (auto viewport = ImGui::FindViewportByPlatformHandle(ctx.window.window))
                    io.AddMouseViewportEvent(viewport->ID);
        }
    }
}

void GUIRenderer::update_viewport_state(ImGuiIO& io, const GUIWindowContext& ctx)
{
    ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(ctx.window.window);
    assert(viewport && "Failed to find viewport by window handle!");

    // update viewport events
    const auto& query = ctx.events;
    for (uint i = 0; i < query.num_events; i++) {
        const auto& event = query.input_events.at(i);
        if (event.type == WindowInputEvent::Type::WINDOW_FOCUS) {
            bool focus = WSI::api()->get_window_focus(ctx.window);
            if (focus) {
                if (io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport)
                    io.AddMouseViewportEvent(viewport->ID);
            }
            io.AddFocusEvent(focus);
        } else if (event.type == WindowInputEvent::Type::WINDOW_MOVE) {
            viewport->PlatformRequestMove = true;
        } else if (event.type == WindowInputEvent::Type::WINDOW_RESIZE) {
            viewport->PlatformRequestResize = true;

            // detect primary viewport
            if (viewport->ParentViewportId == 0) {
                io.DisplaySize.x = static_cast<float>(event.window_resize.width);
                io.DisplaySize.y = static_cast<float>(event.window_resize.height);
            }
        } else if (event.type == WindowInputEvent::Type::WINDOW_CLOSE) {
            viewport->PlatformRequestClose = true;
        }
    }
}

void GUIRenderer::update_monitor_state()
{
    auto& platform_io = ImGui::GetPlatformIO();

    // query monitors
    uint monitor_count = 0;
    WSI::api()->list_monitors(monitor_count, nullptr);
    monitors.resize(monitor_count);
    WSI::api()->list_monitors(monitor_count, monitors.data());

    // update platform monitors
    platform_io.Monitors.clear();
    for (auto& info : monitors) {
        ImGuiPlatformMonitor monitor;
        monitor.MainPos  = ImVec2(float(info.monitor_pos_x), float(info.monitor_pos_y));    // monitor position
        monitor.MainSize = ImVec2(float(info.monitor_width), float(info.monitor_height));   // monitor size
        monitor.WorkPos  = ImVec2(float(info.workarea_pos_x), float(info.workarea_pos_y));  // work area position (excluding taskbar)
        monitor.WorkSize = ImVec2(float(info.workarea_width), float(info.workarea_height)); // work area size (excluding taskbar)
        monitor.DpiScale = info.dpi_scale_x;                                                // DPI scale factor
        platform_io.Monitors.push_back(monitor);
    }
}
#pragma endregion GUIRenderer
