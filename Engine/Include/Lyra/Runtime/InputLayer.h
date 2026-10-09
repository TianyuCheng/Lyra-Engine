#pragma once

#ifndef LYRA_ENGINE_RUNTIME_INPUT_LAYER_H
#define LYRA_ENGINE_RUNTIME_INPUT_LAYER_H

#include <Lyra/Runtime/Application.h>
#include <Lyra/InputSystem/InputManager.h>

namespace lyra
{
    /**
     * @brief application layer hosting and updating the input management system.
     */
    struct InputLayer
    {
    public:
        using FilterProvider = Function<InputFilter()>;

        explicit InputLayer();

        void bind(Application& app);
        void update(AppContext& context);

        void set_filter_provider(FilterProvider provider) { filter_provider = std::move(provider); }
        void clear_filter_provider() { filter_provider = nullptr; }

        auto&       get_manager() { return manager; }
        const auto& get_manager() const { return manager; }

    private:
        InputManager   manager;
        FilterProvider filter_provider = nullptr;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_INPUT_LAYER_H
