lyra_shared(engine)

# headers
target_sources(lyra-engine PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES

  # overall header
  Lyra/Lyra.hpp

  # runtime
  Lyra/Runtime/Application.h
  Lyra/Runtime/AssetLayer.h
  Lyra/Runtime/SceneLayer.h
  Lyra/Runtime/CameraLayer.h
  Lyra/Runtime/EditorLayer.h
  Lyra/Runtime/TimingLayer.h

  # scene
  Lyra/Scene/World.h
  Lyra/Scene/Light.h
  Lyra/Scene/Camera.h
  Lyra/Scene/SceneNode.h
  Lyra/Scene/Transform.h
  Lyra/Scene/SceneTree.h

  # frame graph
  Lyra/Render/FrameGraph.h
  Lyra/Render/FrameGraphAllocator.h
  Lyra/Render/FrameGraphBuffer.h
  Lyra/Render/FrameGraphBuilder.h
  Lyra/Render/FrameGraphContext.h
  Lyra/Render/FrameGraphEnums.h
  Lyra/Render/FrameGraphPass.h
  Lyra/Render/FrameGraphResource.h
  Lyra/Render/FrameGraphTexture.h
  Lyra/Render/FrameGraphTraits.h

  # editor
  Lyra/Editor/Canvas.h
  Lyra/Editor/Colors.h
  Lyra/Editor/Icons.h
  Lyra/Editor/Layout.h
  Lyra/Editor/FileView.h
  Lyra/Editor/TreeView.h
  Lyra/Editor/SceneView.h
  Lyra/Editor/ObjectView.h
  Lyra/Editor/LoggerView.h
)

# sources
target_sources(lyra-engine PRIVATE
  # runtime
  Lyra/Runtime/Application.cpp
  Lyra/Runtime/AssetLayer.cpp
  Lyra/Runtime/SceneLayer.cpp
  Lyra/Runtime/CameraLayer.cpp
  Lyra/Runtime/EditorLayer.cpp
  Lyra/Runtime/TimingLayer.cpp

  # scene
  Lyra/Scene/SceneTree.cpp

  # frame graph
  Lyra/Render/FrameGraph.cpp
  Lyra/Render/FrameGraphAllocator.cpp
  Lyra/Render/FrameGraphBuilder.cpp
  Lyra/Render/FrameGraphTexture.cpp

  # editor
  Lyra/Editor/Canvas.cpp
  Lyra/Editor/Layout.cpp
  Lyra/Editor/FileView.cpp
  Lyra/Editor/TreeView.cpp
  Lyra/Editor/SceneView.cpp
  Lyra/Editor/ObjectView.cpp
  Lyra/Editor/LoggerView.cpp
)

# libraries
target_link_libraries(lyra-engine PUBLIC lyra::common)
target_link_libraries(lyra-engine PUBLIC lyra::assets)
target_link_libraries(lyra-engine PUBLIC lyra::fileio)
target_link_libraries(lyra-engine PUBLIC lyra::window)
target_link_libraries(lyra-engine PUBLIC lyra::render)
target_link_libraries(lyra-engine PUBLIC lyra::shader)
target_link_libraries(lyra-engine PUBLIC lyra::uicore)
