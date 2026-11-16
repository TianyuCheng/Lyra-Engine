include(FetchContent)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

# define imguizmo project
FetchContent_Declare(
  imguizmo
  GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
  GIT_TAG        1.10
)

# get properties
FetchContent_GetProperties(imguizmo)

# populate imguizmo
if(NOT imguizmo_POPULATED)
  FetchContent_Populate(imguizmo)
endif()

# add imguizmo target (if not done so)
if(NOT TARGET imguizmo)
  add_library(imguizmo
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
  )

  add_library(imguizmo-headers INTERFACE)
  target_sources(imguizmo-headers INTERFACE
    FILE_SET  HEADERS
    BASE_DIRS ${imguizmo_SOURCE_DIR}/src
    FILES
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.h
  )

  # imgui headers & library
  target_link_libraries(imguizmo PUBLIC imguizmo-headers imgui::imgui)

  # alias
  add_library(imguizmo::imguizmo ALIAS imguizmo)
  add_library(imguizmo::headers ALIAS imguizmo-headers)

  # folder
  set_target_properties(imguizmo PROPERTIES FOLDER "Vendors")
endif()

set(imguizmo_FOUND TRUE)
