# build d3d12 backend on Windows
if(WIN32)
  add_subdirectory(D3D12)
endif()

# build metal backend on MacOS
if(APPLE)
  add_subdirectory(Metal)
endif()

# build vulkan when it is supported
find_package(Vulkan)
if(${Vulkan_FOUND})
  add_subdirectory(Vulkan)
endif()
