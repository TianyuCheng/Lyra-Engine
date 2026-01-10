# setup plugin
lyra_plugin(zip)

# plugin sources
target_sources(lyra-zip PRIVATE
  ZipUtils.h
  ZipUtils.cpp
  ZipPlugin.cpp
)

find_package(miniz REQUIRED)
target_link_libraries(lyra-zip PRIVATE miniz::miniz)
