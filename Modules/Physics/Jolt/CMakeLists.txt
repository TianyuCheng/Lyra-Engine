find_package(Jolt CONFIG REQUIRED)

# setup plugin
lyra_module(jolt OBJECT)

# plugin sources
target_sources(lyra-jolt PRIVATE
  JoltPlugin.cpp
)

target_link_libraries(lyra-jolt PRIVATE
  Jolt::Jolt
)
