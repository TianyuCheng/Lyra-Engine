include(FetchContent)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

# define imnodes project
FetchContent_Declare(
  imnodes
  GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
  GIT_TAG        2d0c280b07b2f62b120034811cc6e41251673961
)

# get properties
FetchContent_GetProperties(imnodes)

# populate imnodes
if(NOT imnodes_POPULATED)
  FetchContent_Populate(imnodes)
endif()

# add imnodes target (if not done so)
if(NOT TARGET imnodes)
  add_library(imnodes
    ${imnodes_SOURCE_DIR}/imnodes.cpp
  )

  add_library(imnodes-headers INTERFACE)
  target_sources(imnodes-headers INTERFACE
    FILE_SET  HEADERS
    BASE_DIRS ${imnodes_SOURCE_DIR}
    FILES
    ${imnodes_SOURCE_DIR}/imnodes.h
    ${imnodes_SOURCE_DIR}/imnodes_internal.h
  )

  # imgui headers & library
  target_link_libraries(imnodes PUBLIC imnodes-headers imgui::imgui)

  # alias
  add_library(imnodes::imnodes ALIAS imnodes)
  add_library(imnodes::headers ALIAS imnodes-headers)

  # folder
  set_target_properties(imnodes PROPERTIES FOLDER "Vendors")
endif()

set(imnodes_FOUND TRUE)
