#ifndef LYRA_ENGINE_LYRA_HPP
#define LYRA_ENGINE_LYRA_HPP

// Common headers
#include <Lyra/Utilities/ECS.h>
#include <Lyra/Utilities/Hash.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Assert.h>
#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/Msgbox.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Compatibility.h>

// Plugins headers
#include <Lyra/Windowing/WSIDescs.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/Compiler/SLCDescs.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/UISystem/Renderer/GUIEnums.h>
#include <Lyra/UISystem/Renderer/GUITypes.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UIProperty.h>
#include <Lyra/UISystem/Widgets/UITree.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UIDialog.h>
#include <Lyra/FileSystem/VFSEnums.h>
#include <Lyra/FileSystem/VFSTypes.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Assets/AMSServer.h>

// Scenes headers
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/Light.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/Transform.h>
#include <Lyra/Scene/SceneTree.h>

// Format headers
#include <Lyra/Assets/Format/TextAsset.h>
#include <Lyra/Assets/Format/JsonAsset.h>
#include <Lyra/Assets/Format/TomlAsset.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Assets/Format/SceneAsset.h>
#include <Lyra/Assets/Format/TextureAsset.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

// Engine headers
#include <Lyra/Runtime/AppEnums.h>
#include <Lyra/Runtime/AppDescs.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Runtime/Application.h>
#include <Lyra/Runtime/AssetLayer.h>
#include <Lyra/Runtime/SceneLayer.h>
#include <Lyra/Runtime/RenderLayer.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/Runtime/UILayer.h>
#include <Lyra/Runtime/ScriptLayer.h>

// Scripting headers
#include <Lyra/Scripting/Scripting.h>

// Rendering headers
#include <Lyra/Rendering/FrameGraph.h>

#endif // LYRA_ENGINE_LYRA_HPP
