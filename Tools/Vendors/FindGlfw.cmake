find_package(glfw3 CONFIG REQUIRED)

# re-export target with namespace
add_library(glfw::glfw ALIAS glfw)
