#pragma once

#ifndef LYRA_REFLECT_GENERATOR_H
#define LYRA_REFLECT_GENERATOR_H

#include "Model.h"
#include <string>

namespace lyra::reflect
{
    class Generator
    {
    public:
        /**
         * @brief Generates header-only C++20 reflection and script bindings (.gen.h).
         */
        static auto generate(const ModuleReflection& module) -> std::string;
    };

} // namespace lyra::reflect

#endif // LYRA_REFLECT_GENERATOR_H
