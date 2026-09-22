# lyra-loader
lyra_plugin(loader)
target_sources(lyra-loader PRIVATE
    # nativefs
    NativeFSUtils.h
    NativeFS.cpp

    # physfs
    PhysFSUtils.h
    PhysFS.cpp
)

# find dependencies
find_package(apus REQUIRED)
find_package(PhysFS REQUIRED)
target_link_libraries(lyra-loader PRIVATE PhysFS::PhysFS)
