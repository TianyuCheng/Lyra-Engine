# setup pathtracer renderer plugin
lyra_module(renderer-pathtracer SHARED)

# plugin sources
target_sources(lyra-renderer-pathtracer PRIVATE
  PathTracerRendererPlugin.cpp
)
