# setup plugin
lyra_module(softgpu MODULE)

find_package(embree 4 CONFIG REQUIRED)

# plugin sources
target_sources(lyra-softgpu PRIVATE
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

target_link_libraries(lyra-softgpu PRIVATE embree)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  target_compile_options(lyra-softgpu PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-mavx2>
  )
endif()
