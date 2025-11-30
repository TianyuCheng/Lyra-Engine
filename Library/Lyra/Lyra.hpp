#ifndef LYRA_LIBRARY_HPP
#define LYRA_LIBRARY_HPP

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
#include <Lyra/Plugin/WSI/WSIDescs.h>
#include <Lyra/Plugin/WSI/WSITypes.h>
#include <Lyra/Plugin/SLC/SLCDescs.h>
#include <Lyra/Plugin/SLC/SLCTypes.h>
#include <Lyra/Plugin/RHI/RHIDescs.h>
#include <Lyra/Plugin/RHI/RHITypes.h>
#include <Lyra/Plugin/RHI/RHIInits.h>
#include <Lyra/Plugin/GUI/GUITypes.h>
#include <Lyra/Plugin/VFS/VFSEnums.h>
#include <Lyra/Plugin/VFS/VFSTypes.h>
#include <Lyra/Plugin/AMS/AMSUtils.h>
#include <Lyra/Plugin/AMS/AMSTypes.h>

// Engine headers
#include <Lyra/Engine/Applet/Application.h>
#include <Lyra/Engine/Layers/AssetLayer.h>
#include <Lyra/Engine/Layers/ImGuiLayer.h>
#include <Lyra/Engine/Assets/TextAsset.h>
#include <Lyra/Engine/Assets/JsonAsset.h>
#include <Lyra/Engine/Assets/TomlAsset.h>
#include <Lyra/Engine/Assets/TextureAsset.h>
#include <Lyra/Engine/Render/FrameGraph.h>
#include <Lyra/Engine/Render/FrameGraphPass.h>
#include <Lyra/Engine/Render/FrameGraphEnums.h>
#include <Lyra/Engine/Render/FrameGraphContext.h>
#include <Lyra/Engine/Render/FrameGraphBuilder.h>
#include <Lyra/Engine/Render/FrameGraphResource.h>

// Editor files
#include <Lyra/Editor/Canvas.h>
#include <Lyra/Editor/Files.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/Console.h>
#include <Lyra/Editor/Hierarchy.h>
#include <Lyra/Editor/SceneView.h>
#include <Lyra/Editor/Inspector.h>

#endif // LYRA_LIBRARY_HPP
