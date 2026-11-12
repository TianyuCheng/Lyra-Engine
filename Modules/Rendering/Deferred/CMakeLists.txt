# setup deferred hybrid renderer plugin
lyra_module(renderer-deferred SHARED)

# plugin sources
target_sources(lyra-renderer-deferred PRIVATE
  DeferredHybridRendererPlugin.cpp
)
