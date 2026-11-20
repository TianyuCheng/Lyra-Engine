# setup forward renderer plugin
lyra_module(forward SHARED)

# plugin sources
target_sources(lyra-forward PRIVATE
  ForwardRenderer.h
  ForwardRenderer.cpp
  ForwardPlugin.cpp
)
