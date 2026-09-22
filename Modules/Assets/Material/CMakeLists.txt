# lyra-material
lyra_plugin(material)
target_sources(lyra-material PRIVATE
    MaterialPlugin.cpp
)
target_link_libraries(lyra-material PRIVATE lightusd::lightusd)
