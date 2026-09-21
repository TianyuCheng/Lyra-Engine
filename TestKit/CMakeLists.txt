lyra_sample(testkit)
find_package(stb REQUIRED)
find_package(doctest REQUIRED)
target_sources(lyra-testkit PRIVATE
  main.cpp
  ./common/helper.cpp
  ./common/uniform.cpp
  ./common/geometry.cpp
  ./common/render.cpp
  ./common/texture.cpp
  ./common/pipeline.cpp
  ./common/app.cpp
)
target_link_libraries(lyra-testkit INTERFACE stb::stb)
target_link_libraries(lyra-testkit PRIVATE doctest::doctest)
target_include_directories(lyra-testkit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

find_package(Vulkan)
if(${Vulkan_FOUND})
  target_compile_definitions(lyra-testkit PRIVATE LYRA_VULKAN_SUPPORT)
endif()

# non-rhi tests
add_subdirectory(scene_graph)
add_subdirectory(asset_server)
add_subdirectory(shader_reflection)
add_subdirectory(material)

# rhi test cases
add_subdirectory(depth_test)
add_subdirectory(ray_tracing)
add_subdirectory(frame_graph)
add_subdirectory(stencil_test)
add_subdirectory(push_constants)
add_subdirectory(dynamic_uniform)
add_subdirectory(texture_sampling)
add_subdirectory(graphics_pipeline)

# add custom target to run test kit
find_package(Python3 REQUIRED COMPONENTS Interpreter)
message(STATUS "Python interpreter: ${Python3_EXECUTABLE}")
message(STATUS "Python version: ${Python3_VERSION}")
add_custom_target(
  testkit
  COMMAND ${Python3_EXECUTABLE} -u ${PROJECT_SOURCE_DIR}/Scripts/runtest.py $<TARGET_FILE:lyra-testkit> --directory ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Run TestKit with HTML report!"
  DEPENDS lyra-testkit
  VERBATIM)
set_target_properties(testkit PROPERTIES FOLDER "Targets")
