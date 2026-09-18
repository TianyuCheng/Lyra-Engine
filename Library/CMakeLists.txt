# find all external dependencies first
find_package(etl REQUIRED)
find_package(glm REQUIRED)
find_package(fmt REQUIRED)
find_package(absl REQUIRED)
find_package(apus REQUIRED)
find_package(entt REQUIRED)
find_package(json REQUIRED)
find_package(toml REQUIRED)
find_package(boxer REQUIRED)
find_package(spdlog REQUIRED)
find_package(libassert REQUIRED)
find_package(efsw CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(nfd REQUIRED)

# create a dependency interface library
add_library(lyra-commons INTERFACE)
add_library(lyra::commons ALIAS lyra-commons)
target_link_libraries(lyra-commons INTERFACE
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
  $<BUILD_INTERFACE:nfd::nfd>
  $<BUILD_INTERFACE:efsw::efsw>
  $<BUILD_INTERFACE:magic_enum::magic_enum>

  # common
  boxer::boxer
)

# lyra-engine
lyra_shared(engine)

target_sources(lyra-engine PRIVATE
  # assets
  Assets/AMSServer.cpp
  Assets/AMSRegistry.cpp
  Assets/AMSWatcher.cpp
  Assets/AMSUtils.cpp
  Assets/AMSPreview.cpp

  # common
  Common/Logger.cpp
  Common/Msgbox.cpp
  Common/STBImpl.cpp

  # fileio
  FileIO/VFSTypes.cpp

  # format
  Assets/Format/MeshAsset.cpp
  Assets/Format/TextureAsset.cpp
  Assets/Format/JsonAsset.cpp
  Assets/Format/TomlAsset.cpp
  Assets/Format/TextAsset.cpp
  Assets/Format/MaterialAsset.cpp
  Assets/Format/ModelAsset.cpp
  Assets/Format/SceneAsset.cpp

  # shader
  Shader/SLCTypes.cpp

  # render
  Render/RHIInits.cpp
  Render/RHITypes.cpp
  Render/FrameGraph.cpp
  Render/FrameGraphAllocator.cpp
  Render/FrameGraphBuilder.cpp
  Render/FrameGraphTexture.cpp

  # window
  Window/WSIState.cpp
  Window/WSITypes.cpp

  # uicore
  UICore/GUITypes.cpp
  UICore/UI.cpp
  UICore/UILayout.cpp
  UICore/UIControls.cpp
  UICore/UIProperty.cpp
  UICore/UITree.cpp
  UICore/UIDock.cpp
  UICore/UIDialog.cpp

  # scenes
  Scenes/SceneTree.cpp

  # engine
  Engine/Application.cpp
  Engine/AssetLayer.cpp
  Engine/SceneLayer.cpp
  Engine/RenderLayer.cpp
  Engine/TimingLayer.cpp
  Engine/ImGuiLayer.cpp
)

# link headers and common
target_link_libraries(lyra-engine PUBLIC lyra::commons)
target_link_libraries(lyra-engine PUBLIC lyra::headers)

# link builtin plugins
target_link_libraries(lyra-engine PRIVATE lyra::mesh)
target_link_libraries(lyra-engine PRIVATE lyra::model)
target_link_libraries(lyra-engine PRIVATE lyra::scene)
target_link_libraries(lyra-engine PRIVATE lyra::imgui)
target_link_libraries(lyra-engine PRIVATE lyra::loader)
target_link_libraries(lyra-engine PRIVATE lyra::packer)
target_link_libraries(lyra-engine PRIVATE lyra::texture)
target_link_libraries(lyra-engine PRIVATE lyra::material)
target_link_libraries(lyra-engine PRIVATE lyra::preview)
