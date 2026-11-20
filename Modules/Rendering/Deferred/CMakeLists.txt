# setup deferred renderer plugin
lyra_module(deferred SHARED)

# plugin sources
target_sources(lyra-deferred PRIVATE
  DeferredPlugin.cpp
)
