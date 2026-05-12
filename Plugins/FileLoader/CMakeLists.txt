# lyra-fileloader
lyra_plugin(fileloader)
target_sources(lyra-fileloader PRIVATE
    # nativefs
    NativeFSUtils.h
    NativeFS.cpp

    # physfs
    PhysFSUtils.h
    PhysFS.cpp
)

# find dependencies
find_package(apus REQUIRED)
find_package(PhysFS CONFIG REQUIRED)
target_link_libraries(lyra-fileloader PRIVATE PhysFS::PhysFS)
