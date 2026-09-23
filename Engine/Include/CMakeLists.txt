lyra_header(headers)

target_sources(lyra-headers INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
    # everything
    Lyra/Lyra.hpp

    # detail headers
    Lyra/Utilities/Detail/Arena.h
    Lyra/Utilities/Detail/Blackboard.h
    Lyra/Utilities/Detail/View.h

    # utilities headers
    Lyra/Utilities/Assert.h
    Lyra/Utilities/BitFlags.h
    Lyra/Utilities/Collections.h
    Lyra/Utilities/Compatibility.h
    Lyra/Utilities/Config.h
    Lyra/Utilities/Conversion.h
    Lyra/Utilities/ECS.h
    Lyra/Utilities/Enums.h
    Lyra/Utilities/Function.h
    Lyra/Utilities/GUI.h
    Lyra/Utilities/GUID.h
    Lyra/Utilities/Handle.h
    Lyra/Utilities/Hash.h
    Lyra/Utilities/Logger.h
    Lyra/Utilities/Macros.h
    Lyra/Utilities/Math.h
    Lyra/Utilities/Memory.h
    Lyra/Utilities/Msgbox.h
    Lyra/Utilities/Path.h
    Lyra/Utilities/Plugin.h
    Lyra/Utilities/Pointer.h
    Lyra/Utilities/Promise.h
    Lyra/Utilities/Stdint.h
    Lyra/Utilities/String.h
    Lyra/Utilities/UUID.h

    # windowing system
    Lyra/Windowing/WSIAPI.h
    Lyra/Windowing/WSIDescs.h
    Lyra/Windowing/WSIEnums.h
    Lyra/Windowing/WSIEvent.h
    Lyra/Windowing/WSIState.h
    Lyra/Windowing/WSITypes.h
    Lyra/Windowing/WSIUtils.h

    # graphics
    Lyra/Graphics/RHIAPI.h
    Lyra/Graphics/RHIDescs.h
    Lyra/Graphics/RHIEnums.h
    Lyra/Graphics/RHIError.h
    Lyra/Graphics/RHIHash.h
    Lyra/Graphics/RHIInits.h
    Lyra/Graphics/RHITypes.h
    Lyra/Graphics/RHIUtils.h
    Lyra/Graphics/GPUDeletionQueue.h

    # rendering
    Lyra/Rendering/FrameGraph.h

    # compiler
    Lyra/Compiler/SLCAPI.h
    Lyra/Compiler/SLCEnums.h
    Lyra/Compiler/SLCDescs.h
    Lyra/Compiler/SLCTypes.h
    Lyra/Compiler/SLCUtils.h

    # filesystem
    Lyra/FileSystem/VFSAPI.h
    Lyra/FileSystem/VFSEnums.h
    Lyra/FileSystem/VFSTypes.h
    Lyra/FileSystem/VFSUtils.h

    # asset management system
    Lyra/Assets/AMSAPI.h
    Lyra/Assets/AMSEnums.h
    Lyra/Assets/AMSRegistry.h
    Lyra/Assets/AMSServer.h
    Lyra/Assets/AMSUtils.h
    Lyra/Assets/AMSWatcher.h

    # formats
    Lyra/Assets/Format/JsonAsset.h
    Lyra/Assets/Format/MaterialAsset.h
    Lyra/Assets/Format/MeshAsset.h
    Lyra/Assets/Format/ModelAsset.h
    Lyra/Assets/Format/SceneAsset.h
    Lyra/Assets/Format/TextAsset.h
    Lyra/Assets/Format/TextureAsset.h
    Lyra/Assets/Format/TomlAsset.h

    # runtime
    Lyra/Runtime/Application.h
    Lyra/Runtime/AssetLayer.h
    Lyra/Runtime/RenderLayer.h
    Lyra/Runtime/ImGuiLayer.h
    Lyra/Runtime/SceneLayer.h
    Lyra/Runtime/TimingLayer.h

    # scene
    Lyra/Scene/Camera.h
    Lyra/Scene/Light.h
    Lyra/Scene/SceneNode.h
    Lyra/Scene/SceneTree.h
    Lyra/Scene/Transform.h
    Lyra/Scene/World.h

    # ui system
    Lyra/UISystem/GUIAPI.h
    Lyra/UISystem/GUIEnums.h
    Lyra/UISystem/GUITypes.h
    Lyra/UISystem/ImGui.h
    Lyra/UISystem/UIEnums.h
    Lyra/UISystem/UIIcons.h
    Lyra/UISystem/UIInternals.h
    Lyra/UISystem/UI.h
    Lyra/UISystem/UILayout.h
    Lyra/UISystem/UIControls.h
    Lyra/UISystem/UIProperty.h
    Lyra/UISystem/UITree.h
    Lyra/UISystem/UIDock.h
    Lyra/UISystem/UIDialog.h
)

target_include_directories(lyra-headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
