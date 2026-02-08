lyra_shared(common)

target_sources(lyra-common PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  Lyra/Common/Assert.h
  Lyra/Common/BitFlags.h
  Lyra/Common/Collections.h
  Lyra/Common/Compatibility.h
  Lyra/Common/Config.h
  Lyra/Common/Enums.h
  Lyra/Common/Function.h
  Lyra/Common/ECS.h
  Lyra/Common/GUI.h
  Lyra/Common/GUID.h
  Lyra/Common/UUID.h
  Lyra/Common/Hash.h
  Lyra/Common/Logger.h
  Lyra/Common/Msgbox.h
  Lyra/Common/Path.h
  Lyra/Common/Plugin.h
  Lyra/Common/Pointer.h
  Lyra/Common/Stdint.h
  Lyra/Common/String.h

  Lyra/Detail/Arena.h
  Lyra/Detail/Blackboard.h
  Lyra/Detail/Slotmap.h
  Lyra/Detail/View.h
)

target_sources(lyra-common PRIVATE
  Lyra/Common/Logger.cpp
  Lyra/Common/Msgbox.cpp
)

# link dependencies
find_package(etl REQUIRED)
find_package(glm REQUIRED)
find_package(fmt REQUIRED)
find_package(absl REQUIRED)
find_package(entt REQUIRED)
find_package(json REQUIRED)
find_package(toml REQUIRED)
find_package(imgui REQUIRED)
find_package(boxer REQUIRED)
find_package(spdlog REQUIRED)
find_package(libassert REQUIRED)
find_package(magic_enum REQUIRED)
find_package(Boost REQUIRED COMPONENTS circular_buffer)

# directly exposed to user (with include headers)
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
target_link_libraries(lyra-common PRIVATE boxer::boxer)

# include directories
# cmake install target requires relative path.
# Use generator expression could avoid resolving relative path to absolute path.
target_include_directories(lyra-common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
  $<INSTALL_INTERFACE:include>
)
