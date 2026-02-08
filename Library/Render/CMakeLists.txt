lyra_shared(render)

target_sources(lyra-render PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  Lyra/Render/RHIAPI.h
  Lyra/Render/RHIDescs.h
  Lyra/Render/RHIEnums.h
  Lyra/Render/RHIError.h
  Lyra/Render/RHIHash.h
  Lyra/Render/RHIInits.h
  Lyra/Render/RHIUtils.h
  Lyra/Render/RHITypes.h
)

target_sources(lyra-render PRIVATE
  Lyra/Render/RHIInits.cpp
  Lyra/Render/RHITypes.cpp
)

target_link_libraries(lyra-render PUBLIC lyra::common)
target_link_libraries(lyra-render PUBLIC lyra::window)
