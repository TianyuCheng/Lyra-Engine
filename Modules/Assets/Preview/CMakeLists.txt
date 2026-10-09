# lyra-preview
lyra_module(preview OBJECT)

target_sources(lyra-preview PRIVATE
    PreviewPlugin.cpp
    SoftwareRasterizer.h
    SoftwareRasterizer.cpp
)
