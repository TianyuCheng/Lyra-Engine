# setup plugin
lyra_plugin(glfw)

# plugin sources
target_sources(lyra-glfw PRIVATE GlfwPlugin.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_sources(lyra-glfw PRIVATE GlfwMacOS.mm)
  target_link_libraries(lyra-glfw PRIVATE "-framework Cocoa")
  target_link_libraries(lyra-glfw PRIVATE "-framework QuartzCore")
endif()

# link dependencies
find_package(glfw REQUIRED)
target_link_libraries(lyra-glfw PRIVATE glfw::glfw)
target_link_libraries(lyra-glfw PRIVATE lyra::window)
target_link_libraries(lyra-glfw PRIVATE lyra::render)
