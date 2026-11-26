# setup plugin
lyra_module(mockaudio OBJECT)

# plugin sources
target_sources(lyra-mockaudio PRIVATE
  MockAudioPlugin.cpp
)
