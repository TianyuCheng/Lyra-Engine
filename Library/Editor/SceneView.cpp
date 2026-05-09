#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/Render/RHIInits.h>

// local imports
#include <Lyra/Editor/Icons.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/SceneView.h>
#include <Lyra/Player/TimingLayer.h>

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
        float button_size = ImGui::GetFrameHeight() + 4.0f;
        float total_width = button_size * 3 + ImGui::GetStyle().ItemSpacing.x * 2;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - total_width) * 0.5f);

        // state styling
        bool playing = !clock->paused;

        if (playing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Green for play
        if (ImGui::Button(LYRA_ICON_PLAY, ImVec2(button_size, button_size))) {
            clock->paused = false;
        }
        if (playing) ImGui::PopStyleColor();

        ImGui::SameLine();

        if (!playing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.1f, 1.0f)); // Orange for pause
        if (ImGui::Button(LYRA_ICON_PAUSE, ImVec2(button_size, button_size))) {
            clock->paused = true;
        }
        if (!playing) ImGui::PopStyleColor();

        ImGui::SameLine();

        if (ImGui::Button(LYRA_ICON_RESTART, ImVec2(button_size, button_size))) {
            clock->total_time = 0.0f;
            clock->paused     = false;
        }

        ImGui::Dummy(ImVec2(0, 2.0f));

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
