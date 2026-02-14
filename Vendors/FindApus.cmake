include(FetchContent)

# define apus project
FetchContent_Declare(
  apus
  GIT_REPOSITORY https://github.com/TianyuCheng/apus.git
  GIT_TAG        master
)

set(APUS_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(APUS_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(apus)

add_library(apus::apus ALIAS apus)

# mark apus as found
set(apus_FOUND TRUE)

# put apus under folder
set_target_properties(apus PROPERTIES PREFIX "")
set_target_properties(apus PROPERTIES FOLDER "Vendors")
