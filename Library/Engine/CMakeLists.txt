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
  Lyra/Runtime/EditorLayer.h

  # editor
  Lyra/Editor/Canvas.h
  Lyra/Editor/Colors.h
  Lyra/Editor/Console.h
  Lyra/Editor/Files.h
  Lyra/Editor/Fonts.h
  Lyra/Editor/Hierarchy.h
  Lyra/Editor/Icons.h
  Lyra/Editor/Inspector.h
  Lyra/Editor/Layout.h
  Lyra/Editor/SceneView.h

  # scene
  Lyra/Scene/Node.h
  Lyra/Scene/World.h
  Lyra/Scene/Light.h
  Lyra/Scene/Transform.h

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
)

# sources
target_sources(lyra-engine PRIVATE
  # runtime
  Lyra/Runtime/Application.cpp
  Lyra/Runtime/AssetLayer.cpp
  Lyra/Runtime/EditorLayer.cpp

  # editor
  Lyra/Editor/Canvas.cpp
  Lyra/Editor/Console.cpp
  Lyra/Editor/Files.cpp
  Lyra/Editor/Hierarchy.cpp
  Lyra/Editor/Inspector.cpp
  Lyra/Editor/Layout.cpp
  Lyra/Editor/SceneView.cpp

  # frame graph
  Lyra/Render/FrameGraph.cpp
  Lyra/Render/FrameGraphAllocator.cpp
  Lyra/Render/FrameGraphBuilder.cpp
  Lyra/Render/FrameGraphTexture.cpp
)

# libraries
target_link_libraries(lyra-engine PUBLIC lyra::common)
target_link_libraries(lyra-engine PUBLIC lyra::assets)
target_link_libraries(lyra-engine PUBLIC lyra::fileio)
target_link_libraries(lyra-engine PUBLIC lyra::window)
target_link_libraries(lyra-engine PUBLIC lyra::render)
target_link_libraries(lyra-engine PUBLIC lyra::shader)
target_link_libraries(lyra-engine PUBLIC lyra::uicore)
