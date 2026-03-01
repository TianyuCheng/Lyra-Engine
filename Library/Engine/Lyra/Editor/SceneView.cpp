#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/Render/RHIInits.h>

// local imports
#include "Icons.h"
#include "Layout.h"
#include "SceneView.h"
#include "../Runtime/TimingLayer.h"

#define LYRA_SCENE_WINDOW_NAME (LYRA_ICON_SCENE " Scene")

using namespace lyra;

SceneView::SceneView()
{
    // do nothing
}

void SceneView::bind(Application& app)
{
    // save asset manager into blackboard
    app.get_blackboard().add<SceneView*>(this);

    // bind layout manager events
    app.bind<AppEvent::UPDATE, &SceneView::update>(*this);

    // create canvas frames for GameView
    canvas.init(app.get_graphics_descriptor().frames);
}

void SceneView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_SCENE_WINDOW_NAME, layout.main);
    });

    auto clock = blackboard.get<Clock*>();

    ImGui::Begin(LYRA_SCENE_WINDOW_NAME);
    {
        // center align the buttons
        float button_width = 24.0f;
        float total_width  = button_width * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - total_width) * 0.5f);

        if (ImGui::Button(LYRA_ICON_PLAY, ImVec2(button_width, 0))) {
            clock->paused = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(LYRA_ICON_PAUSE, ImVec2(button_width, 0))) {
            clock->paused = true;
        }

        canvas.update(blackboard);
        canvas.display();
    }
    ImGui::End();
}

void SceneView::render_default(GPUCommandBuffer command)
{
    auto backbuffer = get_backbuffer();
    command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), shader_resource_state(GPUBarrierSync::ALL_SHADING)));
}
