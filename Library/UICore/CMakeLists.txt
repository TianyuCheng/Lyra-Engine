lyra_shared(uicore)

target_sources(lyra-uicore PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  Lyra/UICore/GUIAPI.h
  Lyra/UICore/GUITypes.h
)

target_sources(lyra-uicore PRIVATE
  Lyra/UICore/GUITypes.cpp
)

target_link_libraries(lyra-uicore PUBLIC lyra::common)
target_link_libraries(lyra-uicore PUBLIC lyra::shader)
target_link_libraries(lyra-uicore PUBLIC lyra::render)
