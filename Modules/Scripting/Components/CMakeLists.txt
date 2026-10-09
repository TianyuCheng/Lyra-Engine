# lyra-components
lyra_module(components OBJECT)

# binding generation
lyra_bindgen(lyra-components
    MODULE Components
    PREFIX Lyra/Scene
    HEADERS
        Components.h
        ${CMAKE_SOURCE_DIR}/Engine/Include/Lyra/Scene/Mesh.h
        ${CMAKE_SOURCE_DIR}/Engine/Include/Lyra/Scene/Light.h
        ${CMAKE_SOURCE_DIR}/Engine/Include/Lyra/Scene/Camera.h
        ${CMAKE_SOURCE_DIR}/Engine/Include/Lyra/Scene/Transform.h
    SOURCES
        Components.cpp
)

# module sources
target_sources(lyra-components PRIVATE
    Components.h
    Components.cpp
)
