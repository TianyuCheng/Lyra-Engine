include(FetchContent)

# define lightusd project
FetchContent_Declare(
  lightusd
  GIT_REPOSITORY https://github.com/lighttransport/LightUSD.git
  GIT_TAG        v0.9.1
)

FetchContent_MakeAvailable(lightusd)

if (TARGET tinyusdz_static)
  set(_lightusd_target tinyusdz_static)
elseif (TARGET tinyusdz)
  set(_lightusd_target tinyusdz)
elseif (TARGET lightusd)
  set(_lightusd_target lightusd)
endif()

if (DEFINED _lightusd_target)
  target_include_directories(${_lightusd_target} INTERFACE ${lightusd_SOURCE_DIR}/src)
  add_library(lightusd::lightusd ALIAS ${_lightusd_target})
  set_target_properties(${_lightusd_target} PROPERTIES FOLDER "Vendors")
endif()

# mark lightusd as found
set(lightusd_FOUND TRUE)
set(LightUSD_FOUND TRUE)
