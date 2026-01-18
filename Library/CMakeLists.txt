# setup library
lyra_shared(engine)

# library sources
target_sources(lyra-engine PRIVATE
    # common sources
    Lyra/Common/Assert.h
    Lyra/Common/BitFlags.h
    Lyra/Common/Collections.h
    Lyra/Common/Compatibility.h
    Lyra/Common/Config.h
    Lyra/Common/ECS.h
    Lyra/Common/Enums.h
    Lyra/Common/Function.h
    Lyra/Common/GUI.h
    Lyra/Common/Hash.h
    Lyra/Common/Logger.cpp
    Lyra/Common/Logger.h
    Lyra/Common/Math.h
    Lyra/Common/Msgbox.cpp
    Lyra/Common/Msgbox.h
    Lyra/Common/Path.h
    Lyra/Common/Plugin.h
    Lyra/Common/Pointer.h
    Lyra/Common/Stdint.h
    Lyra/Common/String.h

    # window system integration api sources
    Lyra/Plugin/WSI/WSIAPI.h
    Lyra/Plugin/WSI/WSIUtils.h
    Lyra/Plugin/WSI/WSIDescs.h
    Lyra/Plugin/WSI/WSIEnums.h
    Lyra/Plugin/WSI/WSIState.h
    Lyra/Plugin/WSI/WSIState.cpp
    Lyra/Plugin/WSI/WSIEvent.h
    Lyra/Plugin/WSI/WSITypes.h
    Lyra/Plugin/WSI/WSITypes.cpp

    # shader language compiler api sources
    Lyra/Plugin/SLC/SLCAPI.h
    Lyra/Plugin/SLC/SLCDescs.h
    Lyra/Plugin/SLC/SLCEnums.h
    Lyra/Plugin/SLC/SLCTypes.h
    Lyra/Plugin/SLC/SLCTypes.cpp
    Lyra/Plugin/SLC/SLCUtils.h

    # render api sources
    Lyra/Plugin/RHI/RHIAPI.h
    Lyra/Plugin/RHI/RHIDescs.h
    Lyra/Plugin/RHI/RHIEnums.h
    Lyra/Plugin/RHI/RHIError.h
    Lyra/Plugin/RHI/RHIHash.h
    Lyra/Plugin/RHI/RHIInits.cpp
    Lyra/Plugin/RHI/RHIInits.h
    Lyra/Plugin/RHI/RHITypes.cpp
    Lyra/Plugin/RHI/RHITypes.h
    Lyra/Plugin/RHI/RHIUtils.h

    # graphics user interface api sources
    Lyra/Plugin/GUI/GUIAPI.h
    Lyra/Plugin/GUI/GUITypes.h
    Lyra/Plugin/GUI/GUITypes.cpp

    # virtual file system api sources
    Lyra/Plugin/VFS/VFSAPI.h
    Lyra/Plugin/VFS/VFSEnums.h
    Lyra/Plugin/VFS/VFSTypes.h
    Lyra/Plugin/VFS/VFSTypes.cpp
    Lyra/Plugin/VFS/VFSUtils.h

    # asset manage system api sources
    Lyra/Plugin/AMS/AMSAPI.h
    Lyra/Plugin/AMS/AMSEnums.h
    Lyra/Plugin/AMS/AMSTypes.h
    Lyra/Plugin/AMS/AMSTypes.cpp
    Lyra/Plugin/AMS/AMSUtils.h

    # engine applet sources
    Lyra/Engine/Applet/Application.h
    Lyra/Engine/Applet/Application.cpp

    # engine render sources
    Lyra/Engine/Render/FrameGraph.h
    Lyra/Engine/Render/FrameGraph.cpp
    Lyra/Engine/Render/FrameGraphAllocator.h
    Lyra/Engine/Render/FrameGraphAllocator.cpp
    Lyra/Engine/Render/FrameGraphBuffer.h
    Lyra/Engine/Render/FrameGraphBuilder.h
    Lyra/Engine/Render/FrameGraphBuilder.cpp
    Lyra/Engine/Render/FrameGraphEnums.h
    Lyra/Engine/Render/FrameGraphPass.h
    Lyra/Engine/Render/FrameGraphResource.h
    Lyra/Engine/Render/FrameGraphTexture.h
    Lyra/Engine/Render/FrameGraphTexture.cpp
    Lyra/Engine/Render/FrameGraphTraits.h

    # engine assets sources
    Lyra/Engine/Assets/TextAsset.h
    Lyra/Engine/Assets/TextAsset.cpp
    Lyra/Engine/Assets/JsonAsset.h
    Lyra/Engine/Assets/JsonAsset.cpp
    Lyra/Engine/Assets/TomlAsset.h
    Lyra/Engine/Assets/TomlAsset.cpp
    Lyra/Engine/Assets/TextureAsset.h
    Lyra/Engine/Assets/TextureAsset.cpp
    Lyra/Engine/Assets/GenericAsset.h

    # engine layers sources
    Lyra/Engine/Layers/AssetLayer.h
    Lyra/Engine/Layers/AssetLayer.cpp
    Lyra/Engine/Layers/ImGuiLayer.h
    Lyra/Engine/Layers/ImGuiLayer.cpp

    # editor sources
    Lyra/Editor/Icons.h
    Lyra/Editor/Colors.h
    Lyra/Editor/Canvas.h
    Lyra/Editor/Canvas.cpp
    Lyra/Editor/Layout.h
    Lyra/Editor/Layout.cpp
    Lyra/Editor/Files.h
    Lyra/Editor/Files.cpp
    Lyra/Editor/Console.h
    Lyra/Editor/Console.cpp
    Lyra/Editor/Hierarchy.h
    Lyra/Editor/Hierarchy.cpp
    Lyra/Editor/Inspector.h
    Lyra/Editor/Inspector.cpp
    Lyra/Editor/SceneView.h
    Lyra/Editor/SceneView.cpp
)

# link dependencies
find_package(etl REQUIRED)
find_package(glm REQUIRED)
find_package(fmt REQUIRED)
find_package(absl REQUIRED)
find_package(entt REQUIRED)
find_package(json REQUIRED)
find_package(toml REQUIRED)
find_package(boxer REQUIRED)
find_package(imgui REQUIRED)
find_package(spdlog REQUIRED)
find_package(libassert REQUIRED)
find_package(magic_enum REQUIRED)
find_package(Boost REQUIRED COMPONENTS circular_buffer)

# directly exposed to user (with include headers)
target_link_libraries(lyra-engine PUBLIC
    # utility libraries
    $<BUILD_INTERFACE:etl::etl>
    $<BUILD_INTERFACE:absl::base>
    $<BUILD_INTERFACE:absl::hash>
    $<BUILD_INTERFACE:absl::meta>
    $<BUILD_INTERFACE:absl::time>
    $<BUILD_INTERFACE:absl::memory>
    $<BUILD_INTERFACE:absl::utility>
    $<BUILD_INTERFACE:absl::strings>
    $<BUILD_INTERFACE:absl::cleanup>
    $<BUILD_INTERFACE:absl::debugging>
    $<BUILD_INTERFACE:absl::algorithm>
    $<BUILD_INTERFACE:absl::synchronization>

    # math libraries
    $<BUILD_INTERFACE:glm::glm>

    # ecs libraries
    $<BUILD_INTERFACE:EnTT::EnTT>

    # gui libraries
    $<BUILD_INTERFACE:imgui::imgui>

    # debugging libraries
    $<BUILD_INTERFACE:fmt::fmt>
    $<BUILD_INTERFACE:spdlog::spdlog>
    $<BUILD_INTERFACE:libassert::assert>

    # config format libraries
    $<BUILD_INTERFACE:json::json>
    $<BUILD_INTERFACE:toml::toml>

    # misc libraries
    $<BUILD_INTERFACE:Boost::circular_buffer>
    $<BUILD_INTERFACE:magic_enum::magic_enum>
)

# not directly exposed to user
target_link_libraries(lyra-engine PRIVATE boxer::boxer)

# include directories
# cmake install target requires relative path.
# Use generator expression could avoid resolving relative path to absolute path.
target_include_directories(lyra-engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
    $<INSTALL_INTERFACE:include>
)
