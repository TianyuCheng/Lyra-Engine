# setup forward renderer plugin
lyra_module(renderer-forward SHARED)

# plugin sources
target_sources(lyra-renderer-forward PRIVATE
  ForwardRendererPlugin.cpp
)
