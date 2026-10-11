target_sources(lyra-unit-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bounds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_geometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_intersect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_quadtree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_octree.cpp)
