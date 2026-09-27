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
        explicit InputLayer();

        void bind(Application& app);
        void update(AppContext& context);

        auto&       get_manager() { return manager; }
        const auto& get_manager() const { return manager; }

    private:
        InputManager manager;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_INPUT_LAYER_H
