#pragma once

#ifndef LYRA_ENGINE_LYRA_HPP
#define LYRA_ENGINE_LYRA_HPP

// =============================================================================
// 1. Common & Foundations (Utilities)
// =============================================================================

// Types, traits & macros
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/TypeTraits.h>
#include <Lyra/Utilities/Compatibility.h>

// Diagnostics & logging
#include <Lyra/Utilities/Assert.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Msgbox.h>

// Core data structures & functional
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Config.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Collections.h>

// Math, geometry & identifiers
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Hash.h>
#include <Lyra/Utilities/GUID.h>
#include <Lyra/Utilities/UUID.h>
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/Geometry.h>
#include <Lyra/Utilities/Intersect.h>

// Concurrency & asynchronous
#include <Lyra/Utilities/Sync.h>
#include <Lyra/Utilities/Promise.h>
#include <Lyra/Utilities/Coroutine.h>

// Entity-Component System core
#include <Lyra/Utilities/ECS.h>

// =============================================================================
// 2. Core Subsystems
// =============================================================================

// Job system & multithreading
#include <Lyra/JobSystem/Task.h>
#include <Lyra/JobSystem/Dispatch.h>
#include <Lyra/JobSystem/JobSystem.h>
#include <Lyra/JobSystem/JobScheduler.h>

// Virtual File System (VFS)
#include <Lyra/FileSystem/VFSAPI.h>
#include <Lyra/FileSystem/VFSPath.h>
#include <Lyra/FileSystem/VFSEnums.h>
#include <Lyra/FileSystem/VFSTypes.h>
#include <Lyra/FileSystem/VFSUtils.h>

// Asset Management System (AMS)
#include <Lyra/Assets/AMSAPI.h>
#include <Lyra/Assets/AMSEnums.h>
#include <Lyra/Assets/AMSUtils.h>
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/Assets/AMSPreview.h>
#include <Lyra/Assets/AMSRegistry.h>

// Asset format codecs
#include <Lyra/Assets/Format/TextAsset.h>
#include <Lyra/Assets/Format/JsonAsset.h>
#include <Lyra/Assets/Format/TomlAsset.h>
#include <Lyra/Assets/Format/MeshAsset.h>
#include <Lyra/Assets/Format/ModelAsset.h>
#include <Lyra/Assets/Format/SceneAsset.h>
#include <Lyra/Assets/Format/TextureAsset.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

// Windowing & System Integration (WSI)
#include <Lyra/Windowing/WSIAPI.h>
#include <Lyra/Windowing/WSIEnums.h>
#include <Lyra/Windowing/WSIDescs.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/Windowing/WSIEvent.h>
#include <Lyra/Windowing/WSIState.h>

// Render Hardware Interface (RHI)
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Graphics/GPURetireQueue.h>

// Shader Language Compiler (SLC)
#include <Lyra/Compiler/SLCAPI.h>
#include <Lyra/Compiler/SLCEnums.h>
#include <Lyra/Compiler/SLCDescs.h>
#include <Lyra/Compiler/SLCTypes.h>

// =============================================================================
// 3. ECS Scene & Spatial Hierarchy
// =============================================================================

// Components
#include <Lyra/Components/Sun.h>
#include <Lyra/Components/Mesh.h>
#include <Lyra/Components/Light.h>
#include <Lyra/Components/Camera.h>
#include <Lyra/Components/Transform.h>
#include <Lyra/Components/SDFVolume.h>

// Scene graph & spatial acceleration
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/SceneTree.h>
#include <Lyra/Scene/SceneManager.h>
#include <Lyra/Scene/OctTree.h>
#include <Lyra/Scene/QuadTree.h>

// =============================================================================
// 4. Scripting & Reflection
// =============================================================================

#include <Lyra/Scripting/Scripting.h>

// =============================================================================
// 5. Rendering Pipeline & FrameGraph
// =============================================================================

// Renderer contract & descriptors
#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/RendererTypes.h>

// FrameGraph architecture & memory
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/Geometry.h>
#include <Lyra/Rendering/GeometryBufferPool.h>
#include <Lyra/Rendering/GeometryCache.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/SystemTextures.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Rendering/ProceduralMesh.h>

// Scene extraction & execution
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/RenderWorld.h>
#include <Lyra/Rendering/RenderPrimitive.h>
#include <Lyra/Rendering/RendererManager.h>

// Standard rendering passes
#include <Lyra/Rendering/Passes/Pass.h>

// =============================================================================
// 6. UI & Widget System
// =============================================================================

// GUI renderer abstraction
#include <Lyra/UISystem/Renderer/GUIAPI.h>
#include <Lyra/UISystem/Renderer/GUIEnums.h>
#include <Lyra/UISystem/Renderer/GUITypes.h>

// Immediate-mode UI widgets & controls
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UITree.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/UISystem/Widgets/UIIcons.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>
#include <Lyra/UISystem/Widgets/UIDialog.h>
#include <Lyra/UISystem/Widgets/UILayout.h>
#include <Lyra/UISystem/Widgets/UICommand.h>
#include <Lyra/UISystem/Widgets/UIControls.h>
#include <Lyra/UISystem/Widgets/UIProperty.h>

// =============================================================================
// 7. Input System
// =============================================================================

#include <Lyra/InputSystem/InputMap.h>
#include <Lyra/InputSystem/InputEnums.h>
#include <Lyra/InputSystem/InputFilter.h>
#include <Lyra/InputSystem/InputManager.h>

// =============================================================================
// 8. Application Runtime & Layer Stack
// =============================================================================

#include <Lyra/Runtime/AppEnums.h>
#include <Lyra/Runtime/AppDescs.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Runtime/Application.h>
#include <Lyra/Runtime/UILayer.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/Runtime/InputLayer.h>
#include <Lyra/Runtime/AssetLayer.h>
#include <Lyra/Runtime/SceneLayer.h>
#include <Lyra/Runtime/RenderLayer.h>
#include <Lyra/Runtime/ScriptLayer.h>

#endif // LYRA_ENGINE_LYRA_HPP
