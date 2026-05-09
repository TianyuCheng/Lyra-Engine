# setup plugin
lyra_plugin(slang)

# plugin sources
target_sources(lyra-slang PRIVATE
  SlangPlugin.cpp
  SlangUtils.cpp
  SlangUtils.h
)

# link dependencies
find_package(slang REQUIRED)
target_link_libraries(lyra-slang PRIVATE lyra::engine)
target_link_libraries(lyra-slang PRIVATE slang::slang)
