lyra_shared(assets)

target_sources(lyra-assets PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
    Lyra/Assets/AMSAPI.h
    Lyra/Assets/AMSEnums.h
    Lyra/Assets/AMSUtils.h
    Lyra/Assets/AMSTypes.h
    Lyra/Assets/Assets.h
)

target_sources(lyra-assets PRIVATE
    Lyra/Assets/AMSTypes.cpp
    Lyra/Assets/Assets.cpp
)

target_link_libraries(lyra-assets PUBLIC lyra::common)
target_link_libraries(lyra-assets PUBLIC lyra::fileio)
target_link_libraries(lyra-assets PUBLIC lyra::render)
