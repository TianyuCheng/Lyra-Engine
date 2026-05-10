# --- Target: lyra-common ---
lyra_shared(common)

target_sources(lyra-common PRIVATE
  Common/Logger.cpp
  Common/Msgbox.cpp
)

# link headers
target_link_libraries(lyra-common PUBLIC lyra::headers)

# link external dependencies
find_package(etl REQUIRED)
find_package(glm REQUIRED)
find_package(fmt REQUIRED)
find_package(absl REQUIRED)
find_package(apus REQUIRED)
find_package(entt REQUIRED)
find_package(json REQUIRED)
find_package(toml REQUIRED)
find_package(imgui REQUIRED)
find_package(boxer REQUIRED)
find_package(spdlog REQUIRED)
find_package(libassert REQUIRED)
find_package(magic_enum REQUIRED)

target_link_libraries(lyra-common PUBLIC
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
  $<BUILD_INTERFACE:absl::flat_hash_map>
  $<BUILD_INTERFACE:absl::flat_hash_set>
  $<BUILD_INTERFACE:absl::btree>
  $<BUILD_INTERFACE:absl::synchronization>
  $<BUILD_INTERFACE:apus::apus>

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
  $<BUILD_INTERFACE:magic_enum::magic_enum>
)

target_link_libraries(lyra-common PRIVATE boxer::boxer)

# --- Target: lyra-engine ---
lyra_shared(engine)

target_sources(lyra-engine PRIVATE
  # file io
  FileIO/VFSTypes.cpp

  # assets
  Assets/AMSRegistry.cpp
  Assets/AMSServer.cpp
  Assets/Types/TextAsset.cpp
  Assets/Types/JsonAsset.cpp
  Assets/Types/TomlAsset.cpp
  Assets/Types/TextureAsset.cpp
  Assets/Types/MeshAsset.cpp
  Assets/Types/MaterialSchema.cpp
  Assets/Types/MaterialAsset.cpp
  Assets/Types/ModelAsset.cpp

  # windows system
  Window/WSIState.cpp
  Window/WSITypes.cpp

  # render
  Render/RHIInits.cpp
  Render/RHITypes.cpp
  Render/FrameGraph.cpp
  Render/FrameGraphAllocator.cpp
  Render/FrameGraphBuilder.cpp
  Render/FrameGraphTexture.cpp

  # shader / compiler
  Shader/SLCTypes.cpp

  # ui layer
  UICore/GUITypes.cpp

  # editor
  Editor/Canvas.cpp
  Editor/Layout.cpp
  Editor/IconGrid.cpp
  Editor/SelectionModel.cpp
  Editor/FileView.cpp
  Editor/TreeView.cpp
  Editor/SceneView.cpp
  Editor/ObjectView.cpp
  Editor/LoggerView.cpp

  # player
  Player/Application.cpp
  Player/AssetLayer.cpp
  Player/SceneLayer.cpp
  Player/CameraLayer.cpp
  Player/EditorLayer.cpp
  Player/TimingLayer.cpp

  # scenes
  Scenes/SceneTree.cpp
)

# link headers and common
target_link_libraries(lyra-engine PUBLIC lyra::headers)
target_link_libraries(lyra-engine PUBLIC lyra::common)

# link external dependencies
find_package(ktx REQUIRED)
find_package(VulkanHeaders REQUIRED)

target_link_libraries(lyra-engine PUBLIC
  # asset libraries
  $<BUILD_INTERFACE:KTX::ktx>
  $<BUILD_INTERFACE:Vulkan::Headers>
)
