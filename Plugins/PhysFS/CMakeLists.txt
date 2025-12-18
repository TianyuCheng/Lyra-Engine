# setup plugin
lyra_plugin(physfs)

# plugin sources
target_sources(lyra-physfs PRIVATE
  PhysFSUtils.h
  PhysFSPlugin.cpp
)

# packages
find_package(PhysFS REQUIRED)
target_link_libraries(lyra-physfs PRIVATE ${PHYSFS_LIBRARY})
target_include_directories(lyra-physfs PRIVATE ${PHYSFS_INCLUDE_DIR})

if(APPLE)
  find_library(IOKIT_LIBRARY IOKit REQUIRED)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
  target_link_libraries(lyra-physfs PRIVATE ${IOKIT_LIBRARY})
  target_link_libraries(lyra-physfs PRIVATE ${COREFOUNDATION_LIBRARY})
  target_link_libraries(lyra-physfs PRIVATE "-framework Foundation")
endif()
