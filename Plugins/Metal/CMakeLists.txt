# Metal RHI Plugin for Lyra Engine

lyra_plugin(metal)

# find required Metal frameworks
find_library(METAL_LIBRARY Metal REQUIRED)
find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)  # For CAMetalLayer

# all sources as Objective-C++ (.mm extension)
target_sources(lyra-metal PRIVATE
  MetalPlugin.mm
  MetalUtils.h
  MetalUtils.mm
  MetalMappings.mm
  MetalInstance.mm
  MetalAdapter.mm
  MetalDevice.mm
  MetalSwapchain.mm
  MetalFrame.mm
  MetalFence.mm
  MetalBuffer.mm
  MetalTexture.mm
  MetalSampler.mm
  MetalShader.mm
  MetalPipeline.mm
  MetalLayout.mm
  MetalCommandBuffer.mm
  MetalCommandPool.mm
  MetalBindGroup.mm
  MetalQuery.mm
  MetalBlas.mm
  MetalTlas.mm
)

# link Metal frameworks
target_link_libraries(lyra-metal PRIVATE lyra::render)
target_link_libraries(lyra-metal PRIVATE
  ${METAL_LIBRARY}
  ${FOUNDATION_LIBRARY}
  ${QUARTZCORE_LIBRARY}
)

# platform-specific settings for macOS
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_compile_definitions(lyra-metal PRIVATE METAL_MACOS=1)

  # enable ARC for Objective-C code
  target_compile_options(lyra-metal PRIVATE -fobjc-arc)
endif()

# suppress Apple Clang warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  target_compile_options(lyra-metal PRIVATE
    -Wno-nullability-completeness
    -Wno-deprecated-declarations
  )
endif()
