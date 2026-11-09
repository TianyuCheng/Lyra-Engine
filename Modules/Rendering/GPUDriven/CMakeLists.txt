# setup gpu-driven renderer plugin
lyra_module(renderer-gpu-driven SHARED)

# plugin sources
target_sources(lyra-renderer-gpu-driven PRIVATE
  GPUDrivenRendererPlugin.cpp
)
