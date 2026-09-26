# lyra-camera
lyra_module(camera OBJECT)

# binding generation
lyra_bindgen(lyra-camera
    MODULE Camera
    PREFIX Lyra/Scene
    HEADERS
        Camera.hxx
    SOURCES
        Camera.cpp
)

# module sources
target_sources(lyra-camera PRIVATE
    Camera.hxx
    Camera.cpp
)
