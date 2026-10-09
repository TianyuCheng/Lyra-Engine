find_package(nlohmann_json CONFIG REQUIRED)

# re-export target with namespace
add_library(json::json ALIAS nlohmann_json)
