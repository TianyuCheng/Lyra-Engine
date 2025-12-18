# setup plugin
lyra_plugin(ktx)

# plugin sources
target_sources(lyra-ktx PRIVATE
  KtxPlugin.cpp
  KtxAsset.cpp
)

find_package(ktx REQUIRED)
find_package(VulkanHeaders REQUIRED)
target_link_libraries(lyra-ktx PRIVATE KTX::ktx Vulkan::Headers)
