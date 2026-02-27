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
    Lyra/Assets/Types/TextAsset.h
    Lyra/Assets/Types/JsonAsset.h
    Lyra/Assets/Types/TomlAsset.h
    Lyra/Assets/Types/TextureAsset.h
    Lyra/Assets/Types/MeshAsset.h
    Lyra/Assets/Types/MaterialAsset.h
    Lyra/Assets/Types/ModelAsset.h
)

target_sources(lyra-assets PRIVATE
    Lyra/Assets/AMSTypes.cpp
    Lyra/Assets/Types/TextAsset.cpp
    Lyra/Assets/Types/JsonAsset.cpp
    Lyra/Assets/Types/TomlAsset.cpp
    Lyra/Assets/Types/TextureAsset.cpp
    Lyra/Assets/Types/MeshAsset.cpp
    Lyra/Assets/Types/MaterialAsset.cpp
    Lyra/Assets/Types/ModelAsset.cpp
)

target_link_libraries(lyra-assets PUBLIC lyra::common)
target_link_libraries(lyra-assets PUBLIC lyra::fileio)
target_link_libraries(lyra-assets PUBLIC lyra::render)
