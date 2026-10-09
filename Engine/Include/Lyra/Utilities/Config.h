#pragma once

#ifndef LYRA_ENGINE_UTILITIES_CONFIG_H
#define LYRA_ENGINE_UTILITIES_CONFIG_H

#include <toml++/toml.hpp>
#include <nlohmann/json.hpp>

namespace lyra
{
    using JSON = nlohmann::json;
    using TOML = toml::table;
} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_CONFIG_H
