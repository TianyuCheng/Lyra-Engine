# lyra-camera-control
lyra_module(camera-control STATIC)

target_link_libraries(lyra-camera-control PUBLIC lyra::engine)

# binding generation
lyra_bindgen(camera-control
    MODULE CameraControl
    PREFIX Lyra/Components
    HEADERS
        CameraControl.h
    SOURCES
        CameraControl.cpp
)
