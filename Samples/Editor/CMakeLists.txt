lyra_sample(editor)
find_package(cxxopts REQUIRED)
target_include_directories(lyra-editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(lyra-editor PRIVATE
  Main.cpp
  Renderer.h
  Renderer.cpp

  Common/Colors.h
  Common/EditorLayout.h
  Common/EditorLayout.cpp
  Common/SelectionModel.h
  Common/SelectionModel.cpp
  Common/ViewportCanvas.h
  Common/ViewportCanvas.cpp

  Panels/AssetBrowserView.h
  Panels/AssetBrowserView.cpp
  Panels/ConsoleView.h
  Panels/ConsoleView.cpp
  Panels/HierarchyView.h
  Panels/HierarchyView.cpp
  Panels/InspectorView.h
  Panels/InspectorView.cpp
  Panels/SceneView.h
  Panels/SceneView.cpp
)
