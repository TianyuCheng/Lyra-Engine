# lyra-camera-control
lyra_module(camera-control OBJECT)

# binding generation
lyra_bindgen(camera-control
    MODULE CameraControl
    SOURCES
        CameraControl.cpp
)

target_link_libraries(lyra-camera-control PUBLIC lyra::engine)
