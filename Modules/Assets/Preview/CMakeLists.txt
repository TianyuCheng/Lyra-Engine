# lyra-preview
lyra_module(preview OBJECT)

# plugin resources
cmrc_add_resource_library(
  preview-resources
  NAMESPACE preview
  Shaders/PreviewRasterizer.slang
)

target_sources(lyra-preview PRIVATE
  PreviewPlugin.cpp
  BatchPreviewDispatcher.h
  BatchPreviewDispatcher.cpp
)

target_link_libraries(lyra-preview PRIVATE preview-resources)

# move resources under folder
set_target_properties(preview-resources PROPERTIES FOLDER "Resources")
