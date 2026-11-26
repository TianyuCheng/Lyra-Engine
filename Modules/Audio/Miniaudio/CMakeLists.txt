# setup plugin
lyra_module(miniaudio OBJECT)

# plugin sources
target_sources(lyra-miniaudio PRIVATE
  MiniaudioPlugin.cpp
)

# miniaudio single header is vendored next to the plugin
target_include_directories(lyra-miniaudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
