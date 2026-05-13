# lyra-texture
lyra_plugin(texture)
target_sources(lyra-texture PRIVATE
    STBCookerPlugin.cpp
    EXRCookerPlugin.cpp
    DDSCookerPlugin.cpp
    KTXCookerPlugin.cpp
    ThumbnailUtils.cpp
    TexturePlugin.cpp
)

find_package(ktx REQUIRED)
find_package(gli REQUIRED)
find_package(miniz REQUIRED)
find_package(tinyexr REQUIRED)
find_package(VulkanHeaders REQUIRED)

target_link_libraries(lyra-texture PRIVATE gli KTX::ktx miniz::miniz Vulkan::Headers)
