lyra_shared(window)

target_sources(lyra-window PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  Lyra/Window/WSIAPI.h
  Lyra/Window/WSIUtils.h
  Lyra/Window/WSIDescs.h
  Lyra/Window/WSIEnums.h
  Lyra/Window/WSIState.h
  Lyra/Window/WSIEvent.h
  Lyra/Window/WSITypes.h
)

target_sources(lyra-window PRIVATE
  Lyra/Window/WSIState.cpp
  Lyra/Window/WSITypes.cpp
)

target_link_libraries(lyra-window PUBLIC lyra::common)
