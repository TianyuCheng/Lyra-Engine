# setup plugin
lyra_plugin(zipbuilder)

# plugin sources
target_sources(lyra-zipbuilder PRIVATE
  ZipUtils.h
  ZipUtils.cpp
  ZipPlugin.cpp
)

find_package(miniz REQUIRED)
target_link_libraries(lyra-zipbuilder PRIVATE miniz::miniz)
