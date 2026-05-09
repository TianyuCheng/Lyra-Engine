# setup plugin
lyra_plugin(zipfs)

# plugin sources
target_sources(lyra-zipfs PRIVATE
  ZipUtils.h
  ZipUtils.cpp
  ZipPlugin.cpp
)

find_package(miniz REQUIRED)
target_link_libraries(lyra-zipfs PRIVATE lyra::engine)
target_link_libraries(lyra-zipfs PRIVATE miniz::miniz)
