#ifndef LYRA_TESTLIB_HELPER_TEXTURE_H
#define LYRA_TESTLIB_HELPER_TEXTURE_H

#include "./common.h"

struct SimpleTexture2D
{
    uint           width;
    uint           height;
    GPUBuffer      buffer;
    GPUTexture     texture;
    GPUTextureView view;

    void upload();

    static auto gradient(uint width, uint height) -> SimpleTexture2D;
};

#endif // LYRA_TESTLIB_HELPER_TEXTURE_H
