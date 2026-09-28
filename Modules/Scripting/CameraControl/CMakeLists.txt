# lyra-camera-control
lyra_module(camera-control OBJECT)

# binding generation
lyra_bindgen(lyra-camera-control
    MODULE CameraControl
    PREFIX Lyra/Scene
    HEADERS
        CameraControl.h
        ${CMAKE_SOURCE_DIR}/Engine/Include/Lyra/Scene/Camera.h
        ${CMAKE_SOURCE_DIR}/Engine/Include/Lyra/Scene/Light.h
        ${CMAKE_SOURCE_DIR}/Engine/Include/Lyra/Scene/Transform.h
    SOURCES
        CameraControl.cpp
)

# module sources
target_sources(lyra-camera-control PRIVATE
    CameraControl.h
    CameraControl.cpp
)
