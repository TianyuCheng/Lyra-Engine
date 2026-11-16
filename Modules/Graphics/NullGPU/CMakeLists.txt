# setup plugin
lyra_module(nullgpu OBJECT)

# plugin sources
target_sources(lyra-nullgpu PRIVATE
  NullPlugin.cpp
)
