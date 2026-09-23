#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UIControls.h>

// local imports
#include <Lyra/UISystem/Widgets/UIIcons.h>
#include "Common/EditorLayout.h"
#include "SceneView.h"
#include <Lyra/Runtime/TimingLayer.h>

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
        ui::workspace::dock(LYRA_SCENE_WINDOW_NAME, ui::Area::Main);
    });

    auto clock = blackboard.get<Clock*>();

    ui::panel(LYRA_SCENE_WINDOW_NAME, [&]() {
        bool playing = !clock->paused;

        ui::row(ui::Alignment::Center, [&]() {
            ui::icon_button(LYRA_ICON_PLAY, [&]() {
                clock->paused = false;
            }, "Play", playing ? ui::ButtonRole::Success : ui::ButtonRole::Standard);

            ui::icon_button(LYRA_ICON_PAUSE, [&]() {
                clock->paused = true;
            }, "Pause", !playing ? ui::ButtonRole::Warning : ui::ButtonRole::Standard);

            ui::icon_button(LYRA_ICON_RESTART, [&]() {
                clock->total_time = 0.0f;
                clock->paused     = false;
            }, "Restart", ui::ButtonRole::Standard);
        });

        canvas.update(blackboard);
        canvas.display();
    });
}

void SceneView::render_default(GPUCommandBuffer command)
{
    auto backbuffer = get_backbuffer();
    command.resource_barrier(state_transition(backbuffer.texture, undefined_state(), shader_resource_state(GPUBarrierSync::ALL_SHADING)));
}
