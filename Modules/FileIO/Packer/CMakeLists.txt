# lyra-packer
lyra_plugin(packer)
target_sources(lyra-packer PRIVATE
    PakUtils.h
    PakUtils.cpp
    PakPlugin.cpp

    ZipUtils.h
    ZipUtils.cpp
    ZipPlugin.cpp
)

# find dependencies
find_package(miniz CONFIG REQUIRED)
target_link_libraries(lyra-packer PRIVATE miniz::miniz)
