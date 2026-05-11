# lyra-model
lyra_plugin(model)

target_sources(lyra-model PRIVATE
    ModelPlugin.cpp
    STLCookerPlugin.cpp
    OBJCookerPlugin.cpp
    GLTFCookerPlugin.cpp
)

find_package(tinyobjloader CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

target_include_directories(lyra-model PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_link_libraries(lyra-model PRIVATE tinyobjloader::tinyobjloader)
