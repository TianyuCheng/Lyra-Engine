# setup gpu-driven renderer plugin
lyra_module(gpudriven SHARED)

# plugin sources
target_sources(lyra-gpudriven PRIVATE
  GPUDrivenPlugin.cpp
)
