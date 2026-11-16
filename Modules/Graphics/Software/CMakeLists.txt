# setup plugin
lyra_module(software MODULE)

find_package(embree 4 CONFIG REQUIRED)

# plugin sources
target_sources(lyra-software PRIVATE
  SoftPlugin.cpp
  SoftCommon.h
  SoftBuffer.h
  SoftBuffer.cpp
  SoftTexture.h
  SoftTexture.cpp
  SoftSampler.h
  SoftSampler.cpp
  SoftFence.h
  SoftFence.cpp
  SoftSwapchain.h
  SoftSwapchain.cpp
  SoftRaytracing.h
  SoftRaytracing.cpp
  SoftRasterizer.h
  SoftRasterizer.cpp
  SoftPipeline.h
  SoftPipeline.cpp
  SoftDevice.h
  SoftDevice.cpp
  SoftCommandBuffer.h
  SoftCommandBuffer.cpp
)

target_link_libraries(lyra-software PRIVATE
  embree
)

target_compile_options(lyra-software PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-mavx2>
)
