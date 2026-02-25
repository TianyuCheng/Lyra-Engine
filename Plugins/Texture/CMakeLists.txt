# setup plugin
lyra_plugin(texture)

# plugin sources
target_sources(lyra-texture PRIVATE TexturePlugin.cpp)

find_package(ktx REQUIRED)
find_package(VulkanHeaders REQUIRED)
target_link_libraries(lyra-texture PRIVATE lyra::assets)
target_link_libraries(lyra-texture PRIVATE lyra::render)
target_link_libraries(lyra-texture PRIVATE KTX::ktx Vulkan::Headers)
