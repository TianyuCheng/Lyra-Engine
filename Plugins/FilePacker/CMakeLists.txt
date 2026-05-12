# lyra-filepacker
lyra_plugin(filepacker)
target_sources(lyra-filepacker PRIVATE
    PakUtils.h
    PakUtils.cpp
    PakPlugin.cpp

    ZipUtils.h
    ZipUtils.cpp
    ZipPlugin.cpp
)

# find dependencies
find_package(miniz CONFIG REQUIRED)
target_link_libraries(lyra-filepacker PRIVATE miniz::miniz)
