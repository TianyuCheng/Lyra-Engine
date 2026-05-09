# setup plugin
lyra_plugin(vulkan)

# plugin sources
target_sources(lyra-vulkan PRIVATE
  VkPlugin.cpp
  VkUtils.h
  VkUtils.cpp
  VkMappings.cpp
  VkInstance.cpp
  VkSurface.cpp
  VkAdapter.cpp
  VkDevice.cpp
  VkFrame.cpp
  VkSwapchain.cpp
  VkFence.cpp
  VkSemaphore.cpp
  VkShader.cpp
  VkBuffer.cpp
  VkImage.cpp
  VkSampler.cpp
  VkTlas.cpp
  VkBlas.cpp
  VkQuery.cpp
  VkLayout.cpp
  VkPipeline.cpp
  VkCommandPool.cpp
  VkCommandBuffer.cpp
  VkDescriptorPool.cpp
)

# link dependencies
find_package(volk REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
target_link_libraries(lyra-vulkan PRIVATE lyra::engine)
target_link_libraries(lyra-vulkan PRIVATE Vulkan::Headers volk::volk volk::volk_headers GPUOpen::VulkanMemoryAllocator)

# message(STATUS "CXX COMPILER: ${CMAKE_CXX_COMPILER_ID}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
   # added for VulkanMemoryAllocator
  target_compile_options(lyra-vulkan PRIVATE -Wno-nullability-completeness)
endif()
