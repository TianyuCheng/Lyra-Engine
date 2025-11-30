#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Editor/Icons.h>
#include <Lyra/Editor/Colors.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/SceneView.h>
#include <Lyra/Plugin/RHI/RHIInits.h>

#define LYRA_SCENE_WINDOW_NAME (LYRA_ICON_SCENE "Scene")

using namespace lyra;

SceneView::SceneView()
{
}

void SceneView::bind(Application& app)
{
    // save asset manager into blackboard
    app.get_blackboard().add<SceneView*>(this);

    // bind layout manager events
    app.bind<AppEvent::UPDATE>(&SceneView::update, this);

    // create canvas frames for GameView
    canvas.init(app.get_graphics_descriptor().frames);
}

void SceneView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_SCENE_WINDOW_NAME, layout.main);
    });

    imgui::disable_window_menu_button();
    ImGui::Begin(LYRA_SCENE_WINDOW_NAME);
    {
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
