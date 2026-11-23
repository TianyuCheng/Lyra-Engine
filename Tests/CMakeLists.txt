# ==============================================================================
# RHI Tests (Requires GPU device, generates/verifies image output)
# ==============================================================================
lyra_sample(rhi-tests)
find_package(Stb REQUIRED)
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
  ./common/context.cpp
)
target_link_libraries(lyra-rhi-tests INTERFACE stb::stb)
target_link_libraries(lyra-rhi-tests PRIVATE doctest::doctest)
target_include_directories(lyra-rhi-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

find_package(Vulkan)
if(${Vulkan_FOUND})
  target_compile_definitions(lyra-rhi-tests PRIVATE LYRA_VULKAN_SUPPORT)
endif()

# Target to build all test executables
add_custom_target(lyra-testkit DEPENDS lyra-rhi-tests lyra-unit-tests)
set_target_properties(lyra-testkit PROPERTIES FOLDER "Targets")

# ==============================================================================
# Unit Tests (Fast, headless, no GPU required)
# ==============================================================================
lyra_sample(unit-tests)

target_sources(lyra-unit-tests PRIVATE
  unit_main.cpp
  ./common/test_enums.cpp
  ./common/test_geometry_cache.cpp
  ./common/test_gizmo.cpp
)
target_link_libraries(lyra-unit-tests PRIVATE doctest::doctest)
target_include_directories(lyra-unit-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

# rhi test cases
add_subdirectory(depth_test)
add_subdirectory(depth_bias)
add_subdirectory(depth_compare_ops)
add_subdirectory(culling_winding)
add_subdirectory(blend_modes)
add_subdirectory(color_write_mask)
add_subdirectory(scissor_viewport)
add_subdirectory(primitive_topologies)
add_subdirectory(index_formats_offsets)
add_subdirectory(sampler_modes)
add_subdirectory(multi_render_targets)
add_subdirectory(instanced_drawing)
add_subdirectory(indirect_drawing)
add_subdirectory(compute_storage_image)
add_subdirectory(compute_storage_buffer)
add_subdirectory(compute_shared_memory)
add_subdirectory(compute_indirect_dispatch)
add_subdirectory(ray_tracing)
add_subdirectory(frame_graph)
add_subdirectory(stencil_test)
add_subdirectory(immediates)
add_subdirectory(dynamic_uniform)
add_subdirectory(texture_sampling)
add_subdirectory(graphics_pipeline)
add_subdirectory(ui_system)
add_subdirectory(forward_renderer)
add_subdirectory(deferred_renderer)
add_subdirectory(shading_models)
add_subdirectory(atmosphere)


# non-rhi / unit test cases
add_subdirectory(spatial)
add_subdirectory(scene_graph)
add_subdirectory(asset_server)
add_subdirectory(shader_reflection)
add_subdirectory(material)
add_subdirectory(job_system)
add_subdirectory(scripting)
add_subdirectory(graphics)
add_subdirectory(rendering)

# ==============================================================================
# Execution Targets
# ==============================================================================

# run unit tests directly
add_custom_target(
  unit-tests
  COMMAND $<TARGET_FILE:lyra-unit-tests>
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Run Lyra Unit Tests"
  DEPENDS lyra-unit-tests
  VERBATIM)
set_target_properties(unit-tests PROPERTIES FOLDER "Targets")

# run rhi tests directly
add_custom_target(
  rhi-tests
  COMMAND $<TARGET_FILE:lyra-rhi-tests>
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
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
