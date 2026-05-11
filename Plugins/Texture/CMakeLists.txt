find_package(gli REQUIRED)
find_package(miniz REQUIRED)
find_package(tinyexr REQUIRED)

# lyra-texture
lyra_plugin(texture)
target_sources(lyra-texture PRIVATE TexturePlugin.cpp)
target_link_libraries(lyra-texture PRIVATE gli miniz::miniz)
