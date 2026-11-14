# setup plugin
lyra_module(slang MODULE)

# plugin sources
target_sources(lyra-slang PRIVATE
  SlangCommon.h
  SlangCommon.cpp
  SlangFileSystem.h
  SlangFileSystem.cpp
  SlangReflection.h
  SlangReflection.cpp
  SlangCompiler.h
  SlangCompiler.cpp
  SlangPlugin.cpp
)

# link dependencies
find_package(slang REQUIRED)
target_link_libraries(lyra-slang PRIVATE slang::slang)
