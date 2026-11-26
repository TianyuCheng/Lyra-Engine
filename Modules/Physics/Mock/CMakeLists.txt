# setup plugin
lyra_module(mockphysics OBJECT)

# plugin sources
target_sources(lyra-mockphysics PRIVATE
  MockPhysicsPlugin.cpp
)
