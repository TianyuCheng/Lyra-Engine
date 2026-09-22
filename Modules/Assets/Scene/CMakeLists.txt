# lyra-scene
lyra_plugin(scene)

target_sources(lyra-scene PRIVATE
    ScenePlugin.cpp
)

target_link_libraries(lyra-scene PRIVATE lightusd::lightusd)
