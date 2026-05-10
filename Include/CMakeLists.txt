lyra_header(headers)

target_sources(lyra-headers INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
    # everything
    Lyra/Lyra.hpp

    # helper
    Lyra/Helper/Arena.h
    Lyra/Helper/Blackboard.h
    Lyra/Helper/View.h

    # common headers
    Lyra/Common/Assert.h
    Lyra/Common/BitFlags.h
    Lyra/Common/Collections.h
    Lyra/Common/Compatibility.h
    Lyra/Common/Config.h
    Lyra/Common/Conversion.h
    Lyra/Common/ECS.h
    Lyra/Common/Enums.h
    Lyra/Common/Function.h
    Lyra/Common/GUI.h
    Lyra/Common/GUID.h
    Lyra/Common/Handle.h
    Lyra/Common/Hash.h
    Lyra/Common/Logger.h
    Lyra/Common/Macros.h
    Lyra/Common/Math.h
    Lyra/Common/Memory.h
    Lyra/Common/Msgbox.h
    Lyra/Common/Path.h
    Lyra/Common/Plugin.h
    Lyra/Common/Pointer.h
    Lyra/Common/Promise.h
    Lyra/Common/Stdint.h
    Lyra/Common/String.h
    Lyra/Common/UUID.h

    # windows system
    Lyra/Window/WSIAPI.h
    Lyra/Window/WSIDescs.h
    Lyra/Window/WSIEnums.h
    Lyra/Window/WSIEvent.h
    Lyra/Window/WSIState.h
    Lyra/Window/WSITypes.h
    Lyra/Window/WSIUtils.h

    # render
    Lyra/Render/RHIAPI.h
    Lyra/Render/RHIDescs.h
    Lyra/Render/RHIEnums.h
    Lyra/Render/RHIError.h
    Lyra/Render/RHIHash.h
    Lyra/Render/RHIInits.h
    Lyra/Render/RHITypes.h
    Lyra/Render/RHIUtils.h
    Lyra/Render/FrameGraph.h
    Lyra/Render/FrameGraphAllocator.h
    Lyra/Render/FrameGraphBuffer.h
    Lyra/Render/FrameGraphBuilder.h
    Lyra/Render/FrameGraphCommon.h
    Lyra/Render/FrameGraphContext.h
    Lyra/Render/FrameGraphEnums.h
    Lyra/Render/FrameGraphPass.h
    Lyra/Render/FrameGraphResource.h
    Lyra/Render/FrameGraphTexture.h
    Lyra/Render/FrameGraphTraits.h

    # shader / compiler
    Lyra/Shader/SLCAPI.h
    Lyra/Shader/SLCEnums.h
    Lyra/Shader/SLCDescs.h
    Lyra/Shader/SLCTypes.h
    Lyra/Shader/SLCUtils.h

    # file io
    Lyra/FileIO/VFSAPI.h
    Lyra/FileIO/VFSEnums.h
    Lyra/FileIO/VFSTypes.h
    Lyra/FileIO/VFSUtils.h

    # asset management system
    Lyra/Assets/AMSAPI.h
    Lyra/Assets/AMSEnums.h
    Lyra/Assets/AMSRegistry.h
    Lyra/Assets/AMSServer.h
    Lyra/Assets/AMSUtils.h
    Lyra/Assets/Assets.h
    Lyra/Assets/Types/JsonAsset.h
    Lyra/Assets/Types/MaterialAsset.h
    Lyra/Assets/Types/MaterialSchema.h
    Lyra/Assets/Types/MeshAsset.h
    Lyra/Assets/Types/ModelAsset.h
    Lyra/Assets/Types/TextAsset.h
    Lyra/Assets/Types/TextureAsset.h
    Lyra/Assets/Types/TomlAsset.h

    # editor
    Lyra/Editor/Canvas.h
    Lyra/Editor/Colors.h
    Lyra/Editor/FileView.h
    Lyra/Editor/IconGrid.h
    Lyra/Editor/Icons.h
    Lyra/Editor/Layout.h
    Lyra/Editor/LoggerView.h
    Lyra/Editor/ObjectView.h
    Lyra/Editor/SceneView.h
    Lyra/Editor/SelectionModel.h
    Lyra/Editor/TreeView.h

    # player
    Lyra/Player/Application.h
    Lyra/Player/AssetLayer.h
    Lyra/Player/CameraLayer.h
    Lyra/Player/EditorLayer.h
    Lyra/Player/SceneLayer.h
    Lyra/Player/TimingLayer.h

    # scenes
    Lyra/Scenes/Camera.h
    Lyra/Scenes/Light.h
    Lyra/Scenes/SceneNode.h
    Lyra/Scenes/SceneTree.h
    Lyra/Scenes/Transform.h
    Lyra/Scenes/World.h

    # effect
    Lyra/Effect/Effect.h
    Lyra/Effect/MaterialGraph.h

    # ui layer
    Lyra/UICore/GUIAPI.h
    Lyra/UICore/GUIEnums.h
    Lyra/UICore/GUITypes.h
)

target_include_directories(lyra-headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
