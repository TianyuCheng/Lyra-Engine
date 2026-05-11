# setup plugin
lyra_plugin(pakbuilder)

# plugin sources
target_sources(lyra-pakbuilder PRIVATE
  PakUtils.h
  PakUtils.cpp
  PakPlugin.cpp
)
