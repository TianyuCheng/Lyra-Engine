# setup pathtracer renderer plugin
lyra_module(pathtracer SHARED)

# plugin sources
target_sources(lyra-pathtracer PRIVATE
  PathTracerPlugin.cpp
)
