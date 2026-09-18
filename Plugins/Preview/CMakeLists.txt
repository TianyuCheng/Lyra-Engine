# lyra-preview
lyra_plugin(preview)

target_sources(lyra-preview PRIVATE
    PreviewPlugin.cpp
    SoftwareRasterizer.h
    SoftwareRasterizer.cpp
)
