include(FetchContent)

# define lightusd project
FetchContent_Declare(
  lightusd
  GIT_REPOSITORY https://github.com/lighttransport/LightUSD.git
  GIT_TAG        v0.9.1
)

FetchContent_MakeAvailable(lightusd)

add_library(lightusd::lightusd ALIAS lightusd)

# mark lightusd as found
set(lightusd_FOUND TRUE)

# put lightusd under folder
set_target_properties(lightusd PROPERTIES PREFIX "")
set_target_properties(lightusd PROPERTIES FOLDER "Vendors")
