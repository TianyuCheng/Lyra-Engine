# setup library
lyra_shared(engine)

# library sources
target_sources(lyra-engine PRIVATE
    # vendor sources
    Lyra/Vendor/GLM.h
    Lyra/Vendor/ECS.h
    Lyra/Vendor/JSON.h
    Lyra/Vendor/TOML.h
    Lyra/Vendor/IMGUI.h

    # common sources
    Lyra/Common/Assert.h
    Lyra/Common/BitFlags.h
    Lyra/Common/Blackboard.h
    Lyra/Common/Compatibility.h
    Lyra/Common/Container.h
    Lyra/Common/Enums.h
    Lyra/Common/Function.h
    Lyra/Common/Hash.h
    Lyra/Common/Logger.h
    Lyra/Common/Logger.cpp
    Lyra/Common/Msgbox.h
    Lyra/Common/Msgbox.cpp
    Lyra/Common/Path.h
    Lyra/Common/Plugin.h
    Lyra/Common/Pointer.h
    Lyra/Common/Slotmap.h
    Lyra/Common/Stdint.h
    Lyra/Common/String.h
    Lyra/Common/View.h

    # window system sources
    Lyra/Window/WSIAPI.h
    Lyra/Window/WSIUtils.h
    Lyra/Window/WSIDescs.h
    Lyra/Window/WSIEnums.h
    Lyra/Window/WSIState.h
    Lyra/Window/WSIState.cpp
    Lyra/Window/WSIEvent.h
    Lyra/Window/WSITypes.h
    Lyra/Window/WSITypes.cpp

    # shader compiler sources
    Lyra/Shader/SLCAPI.h
    Lyra/Shader/SLCDescs.h
    Lyra/Shader/SLCEnums.h
    Lyra/Shader/SLCTypes.h
    Lyra/Shader/SLCTypes.cpp
    Lyra/Shader/SLCUtils.h

    # render sources
    Lyra/Render/RHI/RHIAPI.h
    Lyra/Render/RHI/RHIDescs.h
    Lyra/Render/RHI/RHIEnums.h
    Lyra/Render/RHI/RHIError.h
    Lyra/Render/RHI/RHIHash.h
    Lyra/Render/RHI/RHIInits.cpp
    Lyra/Render/RHI/RHIInits.h
    Lyra/Render/RHI/RHITypes.cpp
    Lyra/Render/RHI/RHITypes.h
    Lyra/Render/RHI/RHIUtils.h
    Lyra/Render/RPI/FrameGraph.h
    Lyra/Render/RPI/FrameGraph.cpp
    Lyra/Render/RPI/FrameGraphAllocator.h
    Lyra/Render/RPI/FrameGraphAllocator.cpp
    Lyra/Render/RPI/FrameGraphBuffer.h
    Lyra/Render/RPI/FrameGraphBuilder.h
    Lyra/Render/RPI/FrameGraphBuilder.cpp
    Lyra/Render/RPI/FrameGraphEnums.h
    Lyra/Render/RPI/FrameGraphPass.h
    Lyra/Render/RPI/FrameGraphResource.h
    Lyra/Render/RPI/FrameGraphTexture.h
    Lyra/Render/RPI/FrameGraphTexture.cpp
    Lyra/Render/RPI/FrameGraphTraits.h

    # guikit sources
    Lyra/GuiKit/GUIAPI.h
    Lyra/GuiKit/GUITypes.h
    Lyra/GuiKit/GUITypes.cpp

    # fileio sources
    Lyra/FileIO/FSAPI.h
    Lyra/FileIO/FSEnums.h
    Lyra/FileIO/FSTypes.h
    Lyra/FileIO/FSTypes.cpp
    Lyra/FileIO/FSUtils.h

    # assets sources
    Lyra/Assets/AMSAPI.h
    Lyra/Assets/AMSEnums.h
    Lyra/Assets/AMSTypes.h
    Lyra/Assets/AMSTypes.cpp
    Lyra/Assets/AMSUtils.h

    # appkit sources
    Lyra/AppKit/AppTypes.h
    Lyra/AppKit/AppTypes.cpp
    Lyra/AppKit/AppEnums.h
    Lyra/AppKit/AppIcons.h
    Lyra/AppKit/AppColors.h

    # engine sources
    Lyra/Engine/Assets/TextAsset.h
    Lyra/Engine/Assets/TextAsset.cpp
    Lyra/Engine/Assets/JsonAsset.h
    Lyra/Engine/Assets/JsonAsset.cpp
    Lyra/Engine/Assets/TomlAsset.h
    Lyra/Engine/Assets/TomlAsset.cpp
    Lyra/Engine/Assets/TextureAsset.h
    Lyra/Engine/Assets/TextureAsset.cpp
    Lyra/Engine/Assets/GenericAsset.h
    Lyra/Engine/System/AssetManager.cpp
    Lyra/Engine/System/ThemeManager.h
    Lyra/Engine/System/ThemeManager.cpp
    Lyra/Engine/System/LayoutManager.h
    Lyra/Engine/System/LayoutManager.cpp
    Lyra/Engine/System/ImGuiManager.h
    Lyra/Engine/System/ImGuiManager.cpp
    Lyra/Engine/Editor/Files.h
    Lyra/Engine/Editor/Files.cpp
    Lyra/Engine/Editor/Console.h
    Lyra/Engine/Editor/Console.cpp
    Lyra/Engine/Editor/Hierarchy.h
    Lyra/Engine/Editor/Hierarchy.cpp
    Lyra/Engine/Editor/Inspector.h
    Lyra/Engine/Editor/Inspector.cpp
    Lyra/Engine/Editor/SceneView.h
    Lyra/Engine/Editor/SceneView.cpp
    Lyra/Engine/Common/Canvas.h
    Lyra/Engine/Common/Canvas.cpp
)

# link dependencies
find_package(glm REQUIRED)
find_package(fmt REQUIRED)
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
target_link_libraries(lyra-engine PRIVATE boxer::boxer fmt::fmt-header-only)

# include directories
# cmake install target requires relative path.
# Use generator expression could avoid resolving relative path to absolute path.
target_include_directories(lyra-engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
    $<INSTALL_INTERFACE:include>
)
