# setup plugin
lyra_plugin(pakfs)

# plugin sources
target_sources(lyra-pakfs PRIVATE
  PakUtils.h
  PakUtils.cpp
  PakPlugin.cpp
)

target_link_libraries(lyra-pakfs PRIVATE lyra::engine)
