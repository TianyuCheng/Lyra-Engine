# lyra-camera
lyra_module(camera OBJECT)

# binding generation
lyra_bindgen(lyra-camera
    MODULE Camera
    PREFIX Lyra/Scene
    HEADERS
        Camera.hxx
        CameraControl.h
)

# module sources
target_sources(lyra-camera PRIVATE
    Camera.hxx
    CameraControl.h
    CameraPlugin.h
    CameraPlugin.cpp
)
