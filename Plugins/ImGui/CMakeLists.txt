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
  Fonts/Font.ttf
  Shaders/GUIShader.slang
)

target_link_libraries(lyra-imgui PRIVATE imgui-resources)

# move resources under folder
set_target_properties(imgui-resources PROPERTIES FOLDER "Resources")
