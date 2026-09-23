# ==============================================================================
# RHI Tests (Requires GPU device, generates/verifies image output)
# ==============================================================================
lyra_sample(rhi-tests)
find_package(stb REQUIRED)
find_package(doctest REQUIRED)

target_sources(lyra-rhi-tests PRIVATE
  main.cpp
  ./common/helper.cpp
  ./common/uniform.cpp
  ./common/geometry.cpp
  ./common/render.cpp
  ./common/texture.cpp
  ./common/pipeline.cpp
  ./common/app.cpp
)
target_link_libraries(lyra-rhi-tests INTERFACE stb::stb)
target_link_libraries(lyra-rhi-tests PRIVATE doctest::doctest)
target_include_directories(lyra-rhi-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

find_package(Vulkan)
if(${Vulkan_FOUND})
  target_compile_definitions(lyra-rhi-tests PRIVATE LYRA_VULKAN_SUPPORT)
endif()

# alias for backwards compatibility
add_executable(lyra-testkit ALIAS lyra-rhi-tests)

# rhi test cases
add_subdirectory(depth_test)
add_subdirectory(ray_tracing)
add_subdirectory(frame_graph)
add_subdirectory(stencil_test)
add_subdirectory(push_constants)
add_subdirectory(dynamic_uniform)
add_subdirectory(texture_sampling)
add_subdirectory(graphics_pipeline)

# ==============================================================================
# Unit Tests (Fast, headless, no GPU required)
# ==============================================================================
lyra_sample(unit-tests)

target_sources(lyra-unit-tests PRIVATE
  main.cpp
)
target_link_libraries(lyra-unit-tests PRIVATE doctest::doctest)
target_include_directories(lyra-unit-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

# non-rhi / unit test cases
add_subdirectory(scene_graph)
add_subdirectory(asset_server)
add_subdirectory(shader_reflection)
add_subdirectory(material)

# ==============================================================================
# Execution Targets
# ==============================================================================

# run unit tests directly
add_custom_target(
  unit-tests
  COMMAND $<TARGET_FILE:lyra-unit-tests>
  COMMENT "Run Lyra Unit Tests"
  DEPENDS lyra-unit-tests
  VERBATIM)
set_target_properties(unit-tests PROPERTIES FOLDER "Targets")

# run rhi tests directly
add_custom_target(
  rhi-tests
  COMMAND $<TARGET_FILE:lyra-rhi-tests>
  COMMENT "Run Lyra RHI Tests"
  DEPENDS lyra-rhi-tests
  VERBATIM)
set_target_properties(rhi-tests PROPERTIES FOLDER "Targets")

# add custom target to run test kit with HTML report
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(Python3_FOUND)
  message(STATUS "Python interpreter: ${Python3_EXECUTABLE}")
  message(STATUS "Python version: ${Python3_VERSION}")
  add_custom_target(
    testkit
    COMMAND ${Python3_EXECUTABLE} -u ${PROJECT_SOURCE_DIR}/Tools/Scripts/runtest.py $<TARGET_FILE:lyra-rhi-tests> --directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Run TestKit with HTML report!"
    DEPENDS lyra-rhi-tests lyra-unit-tests
    VERBATIM)
  set_target_properties(testkit PROPERTIES FOLDER "Targets")
endif()

enable_testing()
add_test(NAME UnitTests COMMAND lyra-unit-tests)
add_test(NAME RHITests COMMAND lyra-rhi-tests)
