find_package(ktx REQUIRED)
find_package(gli REQUIRED)
find_package(miniz REQUIRED)
find_package(tinyexr REQUIRED)
find_package(VulkanHeaders REQUIRED)

# lyra-ktx
lyra_plugin(ktx)
target_sources(lyra-ktx PRIVATE KtxPlugin.cpp)
target_link_libraries(lyra-ktx PRIVATE lyra::assets lyra::render KTX::ktx Vulkan::Headers)

# lyra-stb
lyra_plugin(stb)
target_sources(lyra-stb PRIVATE StbPlugin.cpp)
target_link_libraries(lyra-stb PRIVATE lyra::assets lyra::render KTX::ktx Vulkan::Headers)

# lyra-exr
lyra_plugin(exr)
target_sources(lyra-exr PRIVATE ExrPlugin.cpp)
target_link_libraries(lyra-exr INTERFACE tinyexr)
target_link_libraries(lyra-exr PRIVATE lyra::assets lyra::render KTX::ktx Vulkan::Headers miniz::miniz)

# lyra-dds
lyra_plugin(dds)
target_sources(lyra-dds PRIVATE DdsPlugin.cpp)
target_link_libraries(lyra-dds INTERFACE gli)
target_link_libraries(lyra-dds PRIVATE lyra::assets lyra::render KTX::ktx Vulkan::Headers)
