#include <Lyra/Windowing/WSIState.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/Runtime/InputLayer.h>

using namespace lyra;

InputLayer::InputLayer()
{
    // do nothing
}

void InputLayer::bind(Application& app)
{
    app.get_toolboard().add<InputManager*>(&manager);
    app.get_toolboard().add<InputLayer*>(this);
    app.bind<AppEvent::UPDATE_PRE, &InputLayer::update>(*this);
}

void InputLayer::update(AppContext& context)
{
    const WindowInput* raw_input = nullptr;
    if (Window* window = context.try_tool<Window>()) {
        raw_input = &window->get_input_state();
    }

    float dt = 0.0f;
    if (Clock* clock = context.try_tool<Clock>()) {
        dt = clock->delta_time;
    }

    InputFilter filter{};
    if (filter_provider) {
        filter = filter_provider();
    }

    manager.update(raw_input, dt, filter);
}
