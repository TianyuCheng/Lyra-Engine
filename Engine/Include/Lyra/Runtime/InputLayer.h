#pragma once

#ifndef LYRA_ENGINE_RUNTIME_INPUT_LAYER_H
#define LYRA_ENGINE_RUNTIME_INPUT_LAYER_H

#include <Lyra/Runtime/Application.h>
#include <Lyra/InputSystem/InputManager.h>
#include <Lyra/InputSystem/InputFilter.h>

namespace lyra
{
    /**
     * @brief application layer hosting and updating the input management system.
     */
    struct InputLayer
    {
    public:
        using FilterCallback = FilterPipeline::Callback;

        explicit InputLayer();

        void bind(Application& app);
        void init(AppContext& context);
        void update(AppContext& context);

        void set_filter(InputPriority priority, FilterCallback callback)
        {
            filter_pipeline.set_filter(priority, std::move(callback));
        }

        void set_filter(InputPriority priority, Function<InputFilter() const> provider)
        {
            filter_pipeline.set_filter(priority, std::move(provider));
        }

        void clear_filter(InputPriority priority)
        {
            filter_pipeline.clear_filter(priority);
        }

        void clear_all_filters()
        {
            filter_pipeline.clear_all();
        }

        auto&       get_filter_pipeline() { return filter_pipeline; }
        const auto& get_filter_pipeline() const { return filter_pipeline; }

        auto&       get_manager() { return manager; }
        const auto& get_manager() const { return manager; }

    private:
        InputManager   manager;
        FilterPipeline filter_pipeline;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_INPUT_LAYER_H
