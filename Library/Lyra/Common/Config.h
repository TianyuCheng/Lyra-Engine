#pragma once

#ifndef LYRA_LIBRARY_COMMON_CONFIG_H
#define LYRA_LIBRARY_COMMON_CONFIG_H

#include <toml++/toml.hpp>
#include <nlohmann/json.hpp>

namespace lyra
{
    using JSON = nlohmann::json;
    using TOML = toml::table;
} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_CONFIG_H
