# lyra-material
lyra_module(material OBJECT)
target_sources(lyra-material PRIVATE
    MaterialPlugin.cpp
)
target_link_libraries(lyra-material PRIVATE lightusd::lightusd)
