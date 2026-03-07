# setup plugin
lyra_plugin(imgui)

# plugin sources
target_sources(lyra-imgui PRIVATE
  ImGuiPlugin.cpp
  GuiRenderer.h
  GuiRenderer.cpp
)

# plugin resources
cmrc_add_resource_library(
  imgui-resources
  NAMESPACE imgui
  Fonts/Texts.ttf
  Fonts/Icons.ttf
  Shaders/GUIShader.slang
)

find_package(imgui REQUIRED)
target_link_libraries(lyra-imgui PUBLIC  imgui)
target_link_libraries(lyra-imgui PRIVATE imgui-resources)
target_link_libraries(lyra-imgui PRIVATE lyra::window)
target_link_libraries(lyra-imgui PRIVATE lyra::shader)
target_link_libraries(lyra-imgui PRIVATE lyra::render)
target_link_libraries(lyra-imgui PRIVATE lyra::uicore)

# move resources under folder
set_target_properties(imgui-resources PROPERTIES FOLDER "Resources")
