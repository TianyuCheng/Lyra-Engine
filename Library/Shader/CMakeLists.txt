lyra_shared(shader)

target_sources(lyra-shader PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  Lyra/Shader/SLCAPI.h
  Lyra/Shader/SLCDescs.h
  Lyra/Shader/SLCEnums.h
  Lyra/Shader/SLCUtils.h
  Lyra/Shader/SLCTypes.h
)

target_sources(lyra-shader PRIVATE
  Lyra/Shader/SLCTypes.cpp
)

target_link_libraries(lyra-shader PUBLIC lyra::common)
target_link_libraries(lyra-shader PUBLIC lyra::window)
target_link_libraries(lyra-shader PUBLIC lyra::render)
