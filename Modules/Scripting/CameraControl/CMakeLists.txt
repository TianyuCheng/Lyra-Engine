# lyra-camera-control
lyra_module(camera-control OBJECT)

# binding generation
lyra_bindgen(camera-control
    MODULE CameraControl
    PREFIX Lyra/Scene
    HEADERS
        CameraControl.h
    SOURCES
        CameraControl.cpp
)
