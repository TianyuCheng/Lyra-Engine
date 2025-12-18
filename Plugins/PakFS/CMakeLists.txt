# setup plugin
lyra_plugin(pakfs)

# plugin sources
target_sources(lyra-pakfs PRIVATE
  PakUtils.h
  PakUtils.cpp
  PakPlugin.cpp
)
