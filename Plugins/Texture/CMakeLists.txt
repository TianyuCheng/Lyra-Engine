# setup plugin
lyra_plugin(texture)

# plugin sources
target_sources(lyra-texture PRIVATE TexturePlugin.cpp)

find_package(ktx REQUIRED)
find_package(gli REQUIRED)
find_package(miniz REQUIRED)
find_package(tinyexr REQUIRED)
find_package(VulkanHeaders REQUIRED)
target_link_libraries(lyra-texture PRIVATE lyra::assets)
target_link_libraries(lyra-texture PRIVATE lyra::render)
target_link_libraries(lyra-texture PRIVATE KTX::ktx Vulkan::Headers miniz::miniz)
target_link_libraries(lyra-texture INTERFACE tinyexr gli)
