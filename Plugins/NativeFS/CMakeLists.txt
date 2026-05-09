# setup plugin
lyra_plugin(nativefs)

# plugin sources
target_sources(lyra-nativefs PRIVATE
  NativeFSUtils.h
  NativeFSPlugin.cpp
)

target_link_libraries(lyra-nativefs PRIVATE lyra::engine)
