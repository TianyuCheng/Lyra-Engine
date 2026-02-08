#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/Render/RHIInits.h>

// local imports
#include "Icons.h"
#include "Layout.h"
#include "SceneView.h"

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
