# lyra-scene
lyra_module(scene OBJECT)

target_sources(lyra-scene PRIVATE
    ScenePlugin.cpp
)

target_link_libraries(lyra-scene PRIVATE lightusd::lightusd)
