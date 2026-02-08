lyra_shared(fileio)

target_sources(lyra-fileio PUBLIC
  FILE_SET  HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  Lyra/FileIO/VFSAPI.h
  Lyra/FileIO/VFSEnums.h
  Lyra/FileIO/VFSUtils.h
  Lyra/FileIO/VFSTypes.h
)

target_sources(lyra-fileio PRIVATE
  Lyra/FileIO/VFSTypes.cpp
)

target_link_libraries(lyra-fileio PUBLIC lyra::common)
