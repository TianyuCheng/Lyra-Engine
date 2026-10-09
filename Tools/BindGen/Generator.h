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
         * @brief Generates C++20 reflection and script bindings (.gen.cpp or .gen.h).
         */
        static auto generate(const ModuleReflection& module, const std::string& output_filename = "") -> std::string;
    };

} // namespace lyra::reflect

#endif // LYRA_REFLECT_GENERATOR_H
