include(FetchContent)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

# define imgui-node-editor project
FetchContent_Declare(
  nodeeditor
  GIT_REPOSITORY https://github.com/thedmd/imgui-node-editor.git
  GIT_TAG        1e0e494f1a86885eb332ca518543108ccd88d13e
)

# get properties
FetchContent_GetProperties(nodeeditor)

# populate nodeeditor
if(NOT nodeeditor_POPULATED)
  FetchContent_Populate(nodeeditor)
endif()

# add nodeeditor target (if not done so)
if(NOT TARGET nodeeditor)
  add_library(nodeeditor
    ${nodeeditor_SOURCE_DIR}/crude_json.cpp
    ${nodeeditor_SOURCE_DIR}/imgui_canvas.cpp
    ${nodeeditor_SOURCE_DIR}/imgui_node_editor.cpp
    ${nodeeditor_SOURCE_DIR}/imgui_node_editor_api.cpp
  )

  add_library(nodeeditor-headers INTERFACE)
  target_sources(nodeeditor-headers INTERFACE
    FILE_SET  HEADERS
    BASE_DIRS ${nodeeditor_SOURCE_DIR}
    FILES
    ${nodeeditor_SOURCE_DIR}/crude_json.h
    ${nodeeditor_SOURCE_DIR}/imgui_bezier_math.h
    ${nodeeditor_SOURCE_DIR}/imgui_bezier_math.inl
    ${nodeeditor_SOURCE_DIR}/imgui_canvas.h
    ${nodeeditor_SOURCE_DIR}/imgui_extra_math.h
    ${nodeeditor_SOURCE_DIR}/imgui_extra_math.inl
    ${nodeeditor_SOURCE_DIR}/imgui_node_editor.h
    ${nodeeditor_SOURCE_DIR}/imgui_node_editor_internal.h
    ${nodeeditor_SOURCE_DIR}/imgui_node_editor_internal.inl
  )

  target_include_directories(nodeeditor PUBLIC ${nodeeditor_SOURCE_DIR})

  # imgui headers & library
  target_link_libraries(nodeeditor PUBLIC nodeeditor-headers imgui::imgui)

  # alias
  add_library(nodeeditor::nodeeditor ALIAS nodeeditor)
  add_library(nodeeditor::headers ALIAS nodeeditor-headers)

  # folder
  set_target_properties(nodeeditor PROPERTIES FOLDER "Vendors")
endif()

set(nodeeditor_FOUND TRUE)
