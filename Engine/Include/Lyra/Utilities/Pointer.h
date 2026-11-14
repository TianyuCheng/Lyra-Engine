#pragma once

#ifndef LYRA_ENGINE_UTILITIES_POINTER_H
#define LYRA_ENGINE_UTILITIES_POINTER_H

#include <memory>
#include <utility>
#include <Lyra/Utilities/Macros.h>

namespace lyra
{
    template <typename T, class Deleter = std::default_delete<T>>
    using Own = std::unique_ptr<T, Deleter>;

    template <typename T>
    using Ref = std::shared_ptr<T>;

    template <typename T, typename... Args>
    FORCE_INLINE Ref<T> make_ref(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    FORCE_INLINE Own<T> make_own(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    struct Destroyer
    {
        void operator()(T* pointer)
        {
            pointer->destroy();
        }
    };

    template <typename T>
    using OwnedResource = std::unique_ptr<T, Destroyer<T>>;

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_POINTER_H
