#ifndef LYRA_TESTLIB_HELPER_RENDER_H
#define LYRA_TESTLIB_HELPER_RENDER_H

#include "./common.h"

struct RenderTarget
{
    uint             width;
    uint             height;
    GPUTextureFormat format;
    GPUTexture       texture;
    GPUTextureView   view;
    GPUBuffer        buffer;

    auto copy_src() const -> GPUTexelCopyTextureInfo;
    auto copy_dst() const -> GPUTexelCopyBufferInfo;
    auto copy_ext() const -> GPUExtent3D;
    void save(CString name);

    static auto create(GPUTextureFormat format, uint width, uint height, uint samples = 1) -> RenderTarget;
};

#endif // LYRA_TESTLIB_HELPER_RENDER_H
