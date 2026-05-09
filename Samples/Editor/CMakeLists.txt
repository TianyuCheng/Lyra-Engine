lyra_sample(editor)
find_package(cxxopts REQUIRED)
target_sources(lyra-editor PRIVATE main.cpp
    renderer.h
    renderer.cpp
)
