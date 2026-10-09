find_package(tomlplusplus CONFIG REQUIRED)

# re-export target with namespace
add_library(toml::toml ALIAS tomlplusplus::tomlplusplus)
