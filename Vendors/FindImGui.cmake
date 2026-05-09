include(FetchContent)

# define imgui project
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        34debc7 # use commit from docking branch
)

# get properties
FetchContent_GetProperties(imgui)

# populate imgui
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

# add imgui target (if not done so)
if(NOT TARGET imgui)
  add_library(imgui
    # imgui core sources
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  )

  add_library(imgui-headers INTERFACE)
  target_sources(imgui-headers INTERFACE
    FILE_SET  HEADERS
    BASE_DIRS ${imgui_SOURCE_DIR}
    FILES
    imgui.h
    imconfig.h
    imgui_internal.h
    imgui_rectpack.h
    imgui_textedit.h
    imgui_truetype.h
  )

  # imgui headers
  target_link_libraries(imgui PUBLIC imgui-headers)

  # alias
  add_library(imgui::imgui ALIAS imgui)
  add_library(imgui::headers ALIAS imgui-headers)
endif()

# mark imgui as found
set(imgui_FOUND TRUE)

# put imgui under folder
set_target_properties(imgui PROPERTIES PREFIX "")
set_target_properties(imgui PROPERTIES FOLDER "Vendors")
