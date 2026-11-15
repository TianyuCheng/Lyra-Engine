#include "ForwardRenderer.h"
#include <Lyra/Utilities/Logger.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIProperty.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>

using namespace lyra;

bool ForwardRenderer::init(GPUDevice in_device, Compiler in_compiler)
{
    device   = in_device;
    compiler = in_compiler;

    if (!RHI::api() || !compiler.handle.valid()) {
        return false;
    }

    // 1. initialize core forward pass
    if (!forward_pass.init(device, compiler)) {
        return false;
    }

    // 2. initialize depth prepass
    depth_prepass.init(device, compiler);

    // 3. initialize shadow pass
    shadow_pass.init(device, compiler);

    // 4. initialize light culling pass for Forward+
    light_culling_pass.init(device, compiler);

    // 5. initialize atmospheric skybox pass
    skybox_pass.init(device, compiler);

    // 6. initialize tonemapping pass
    tonemap_pass.init(device, compiler);

    initialized = forward_pass.valid();
    return initialized;
}

void ForwardRenderer::shutdown()
{
    if (initialized) {
        forward_pass.destroy();
        depth_prepass.destroy();
        shadow_pass.destroy();
        light_culling_pass.destroy();
        skybox_pass.destroy();
        tonemap_pass.destroy();
        initialized = false;
    }
}

void ForwardRenderer::resize(uint width, uint height)
{
    // target dimensions updated dynamically per-frame from RenderFrameContext
}

FrameGraphHandle<FrameGraphTexture> ForwardRenderer::render(const RenderFrameContext& ctx)
{
    if (!ctx.color_target.valid()) {
        return ctx.color_target;
    }

    GPUExtent2D extent = (ctx.render_extent.width > 0 && ctx.render_extent.height > 0)
                             ? ctx.render_extent
                             : ctx.display_extent;

    if (extent.width == 0 || extent.height == 0) {
        return ctx.color_target;
    }

    auto& builder = ctx.builder;

    // ensure depth target handle is valid
    auto depth_h = ctx.depth_target;
    if (!depth_h.valid()) {
        auto depth_desc   = GPUTextureDescriptor{};
        depth_desc.label  = "forward_depth_buffer";
        depth_desc.size   = {extent.width, extent.height, 1};
        depth_desc.format = ctx.depth_format;
        depth_desc.usage  = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
        depth_h           = builder.create<FrameGraphTexture>(depth_desc);
    }

    // 1. Cascaded shadow mapping pass (if shadows enabled and directional lights exist)
    if (settings.enable_shadows && !ctx.scene.directional_lights.empty() && shadow_pass.valid()) {
        shadow_pass.add(builder, ctx.view, ctx.scene);
    }

    // 2. Depth Prepass (if enabled in settings)
    bool prepass_active = false;
    if (settings.debug_mode == 0 && depth_prepass.valid()) {
        auto dp_data   = depth_prepass.add(builder, ctx.view, ctx.scene, extent);
        depth_h        = dp_data.depth_texture;
        prepass_active = true;
    }

    // 3. Clustered Light Culling (Forward+)
    if (light_culling_pass.valid()) {
        light_culling_pass.add(builder, ctx.view, ctx.scene);
    }

    // 4. Atmospheric Skybox Pass (rendered over clear color at far plane)
    if (skybox_pass.valid()) {
        skybox_pass.add(builder, ctx.color_target, depth_h, ctx.view, ctx.scene);
    }

    // 5. Forward Base Pass
    passes::ForwardPassParams params{
        .color_target         = ctx.color_target,
        .depth_target         = depth_h,
        .view                 = ctx.view,
        .scene                = ctx.scene,
        .extent               = extent,
        .color_format         = ctx.color_format,
        .depth_format         = ctx.depth_format,
        .depth_prepass_active = prepass_active,
        .color_load_op        = GPULoadOp::LOAD, // load skybox / scene clear
    };

    auto fp_data = forward_pass.add(builder, params);

    // 6. Update telemetry stats
    stats.draw_calls         = fp_data.draw_calls;
    stats.triangles_rendered = fp_data.triangles_rendered;

    // 7. Export outputs bundle if caller requested them
    if (ctx.outputs) {
        ctx.outputs->depth = fp_data.depth_target;
    }

    return fp_data.color_target;
}

void ForwardRenderer::draw_debug_panel()
{
    ui::section("Forward / Forward+ Renderer", LYRA_ICON_CAMERA, [&]() {
        ui::section("Telemetry", LYRA_ICON_DEBUG, [&]() {
            ui::properties([&]() {
                String dc_str = std::to_string(stats.draw_calls);
                ui::label("Draw Calls");
                ui::label(dc_str.c_str(), ui::StatusRole::Info);

                String tri_str = std::to_string(stats.triangles_rendered);
                ui::label("Triangles");
                ui::label(tri_str.c_str(), ui::StatusRole::Info);

                ui::label("Forward Pass");
                ui::label(forward_pass.valid() ? "Ready" : "Offline",
                          forward_pass.valid() ? ui::StatusRole::Success : ui::StatusRole::Error);

                ui::label("Depth Prepass");
                ui::label(depth_prepass.valid() ? "Active" : "Offline",
                          depth_prepass.valid() ? ui::StatusRole::Success : ui::StatusRole::Muted);

                ui::label("Shadow Cascades");
                ui::label(shadow_pass.valid() ? "Active" : "Offline",
                          shadow_pass.valid() ? ui::StatusRole::Success : ui::StatusRole::Muted);

                ui::label("Light Culling");
                ui::label(light_culling_pass.valid() ? "Active" : "Offline",
                          light_culling_pass.valid() ? ui::StatusRole::Success : ui::StatusRole::Muted);
            });
        });

        ui::separator();

        ui::section("Settings", LYRA_ICON_SETTINGS, [&]() {
            ui::properties([&]() {
                ui::toggle("Enable Shadows", settings.enable_shadows);
                ui::toggle("Enable Ambient Occlusion", settings.enable_ambient_occl);
                ui::toggle("Enable Reflections", settings.enable_reflections);
                ui::toggle("Wireframe Mode", settings.enable_wireframe);

                ui::ScalarConfig exposure_cfg{.speed = 0.05f, .min = 0.1f, .max = 10.0f, .reset = 1.0f};
                ui::number("Exposure", settings.exposure, exposure_cfg);

                static constexpr CString debug_items[] = {
                    "Lit (PBR)",
                    "Normals",
                    "Albedo",
                    "Wireframe",
                    "Light Heatmap"
                };
                usize current_idx = settings.debug_mode < 5 ? settings.debug_mode : 0;
                ui::dropdown("Debug Mode", debug_items[current_idx], debug_items, 5, [&](usize selected) {
                    settings.debug_mode = static_cast<uint>(selected);
                });
            });
        });
    });
}
