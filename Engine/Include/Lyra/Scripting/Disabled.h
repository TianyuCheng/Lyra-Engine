#pragma once

#ifndef LYRA_ENGINE_SCRIPTING_DISABLED_H
#define LYRA_ENGINE_SCRIPTING_DISABLED_H

namespace lyra
{
    /**
     * @brief Tag component used by the Inspector and systems to disable a component
     * without removing its underlying data.
     */
    template <typename T>
    struct Disabled
    {
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCRIPTING_DISABLED_H
