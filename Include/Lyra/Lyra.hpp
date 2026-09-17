#ifndef LYRA_LYRA_LYRA_HPP
#define LYRA_LYRA_LYRA_HPP

// Common headers
#include <Lyra/Common/ECS.h>
#include <Lyra/Common/Hash.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/Math.h>
#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Assert.h>
#include <Lyra/Common/Config.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/Common/Msgbox.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/BitFlags.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Common/Compatibility.h>

// Plugins headers
#include <Lyra/Window/WSIDescs.h>
#include <Lyra/Window/WSITypes.h>
#include <Lyra/Shader/SLCDescs.h>
#include <Lyra/Shader/SLCTypes.h>
#include <Lyra/Render/RHIDescs.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/Render/RHIInits.h>
#include <Lyra/UICore/GUIEnums.h>
#include <Lyra/UICore/GUITypes.h>
#include <Lyra/UICore/UIEnums.h>
#include <Lyra/UICore/UIIcons.h>
#include <Lyra/UICore/UI.h>
#include <Lyra/UICore/UILayout.h>
#include <Lyra/UICore/UIControls.h>
#include <Lyra/UICore/UIProperty.h>
#include <Lyra/UICore/UITree.h>
#include <Lyra/UICore/UIDock.h>
#include <Lyra/UICore/UIDialog.h>
#include <Lyra/FileIO/VFSEnums.h>
#include <Lyra/FileIO/VFSTypes.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Assets/AMSServer.h>

// Render headers
#include <Lyra/Render/FrameGraph.h>
#include <Lyra/Render/FrameGraphPass.h>
#include <Lyra/Render/FrameGraphEnums.h>
#include <Lyra/Render/FrameGraphContext.h>
#include <Lyra/Render/FrameGraphBuilder.h>
#include <Lyra/Render/FrameGraphResource.h>

// Scenes headers
#include <Lyra/Scenes/World.h>
#include <Lyra/Scenes/Light.h>
#include <Lyra/Scenes/Camera.h>
#include <Lyra/Scenes/SceneNode.h>
#include <Lyra/Scenes/Transform.h>
#include <Lyra/Scenes/SceneTree.h>

// Format headers
#include <Lyra/Assets/Format/TextAsset.h>
#include <Lyra/Assets/Format/JsonAsset.h>
#include <Lyra/Assets/Format/TomlAsset.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Assets/Format/SceneAsset.h>
#include <Lyra/Assets/Format/TextureAsset.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

// Engine headers
#include <Lyra/Engine/Application.h>
#include <Lyra/Engine/AssetLayer.h>
#include <Lyra/Engine/SceneLayer.h>
#include <Lyra/Engine/CameraLayer.h>
#include <Lyra/Engine/TimingLayer.h>
#include <Lyra/Engine/ImGuiLayer.h>

#endif // LYRA_LYRA_LYRA_HPP
