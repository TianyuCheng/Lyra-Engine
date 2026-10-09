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
    Lyra/Utilities/Bounds.h
    Lyra/Utilities/Collections.h
    Lyra/Utilities/Compatibility.h
    Lyra/Utilities/Config.h
    Lyra/Utilities/Conversion.h
    Lyra/Utilities/ECS.h
    Lyra/Utilities/Enums.h
    Lyra/Utilities/Function.h
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
    Lyra/Assets/AMSPreview.h
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
    Lyra/Runtime/AppDescs.h
    Lyra/Runtime/AppEnums.h
    Lyra/Runtime/AppTypes.h
    Lyra/Runtime/Application.h
    Lyra/Runtime/AssetLayer.h
    Lyra/Runtime/RenderLayer.h
    Lyra/Runtime/SceneLayer.h
    Lyra/Runtime/TimingLayer.h
    Lyra/Runtime/UILayer.h

    # scene
    Lyra/Scene/Light.h
    Lyra/Scene/SceneNode.h
    Lyra/Scene/SceneTree.h
    Lyra/Scene/Transform.h
    Lyra/Scene/World.h

    # uisystem
    Lyra/UISystem/Renderer/GUIAPI.h
    Lyra/UISystem/Renderer/GUIEnums.h
    Lyra/UISystem/Renderer/GUITypes.h
    Lyra/UISystem/Widgets/UIEnums.h
    Lyra/UISystem/Widgets/UIIcons.h
    Lyra/UISystem/Widgets/UI.h
    Lyra/UISystem/Widgets/UILayout.h
    Lyra/UISystem/Widgets/UIControls.h
    Lyra/UISystem/Widgets/UIProperty.h
    Lyra/UISystem/Widgets/UITree.h
    Lyra/UISystem/Widgets/UIDock.h
    Lyra/UISystem/Widgets/UIDialog.h
)

target_include_directories(lyra-headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
