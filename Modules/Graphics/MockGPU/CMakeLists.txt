# setup plugin
lyra_module(mockgpu OBJECT)

# plugin sources
target_sources(lyra-mockgpu PRIVATE
  MockPlugin.cpp
)
