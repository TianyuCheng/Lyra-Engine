#include <Lyra/Player/TimingLayer.h>

using namespace lyra;

TimingLayer::TimingLayer()
{
    start_time      = std::chrono::high_resolution_clock::now();
    last_frame_time = start_time;
}

void TimingLayer::bind(Application& app)
{
    // add clock to blackboard
    app.get_blackboard().add<Clock*>(&clock);

    // update timing before everything else
    app.bind<AppEvent::UPDATE_PRE, &TimingLayer::update>(*this);
}

void TimingLayer::update(Blackboard&)
{
    auto now = std::chrono::high_resolution_clock::now();

    // calculate actual delta time
    std::chrono::duration<float> delta = now - last_frame_time;
    last_frame_time                    = now;

    if (clock.paused) {
        clock.delta_time = 0.0f;
    } else {
        clock.delta_time = delta.count() * clock.time_scale;
        clock.total_time += clock.delta_time;
    }
}
