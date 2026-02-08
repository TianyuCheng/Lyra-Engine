# window plugins
#--------------------------------
add_subdirectory(Glfw)

# shader plugins
#--------------------------------
add_subdirectory(Slang)

# editor plugins
#--------------------------------
add_subdirectory(ImGui)

# fileio plugins
#--------------------------------
add_subdirectory(NativeFS)
add_subdirectory(PhysFS)
add_subdirectory(PakFS)
add_subdirectory(ZipFS)

# assets plugins
#--------------------------------
add_subdirectory(Ktx)

# render plugins
#--------------------------------

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
