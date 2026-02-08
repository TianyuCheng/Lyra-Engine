#pragma once

#ifndef LYRA_LIBRARY_RENDER_RHI_HASH_H
#define LYRA_LIBRARY_RENDER_RHI_HASH_H

#include <Lyra/Common/Hash.h>
#include <Lyra/Render/RHIDescs.h>

template <>
struct std::hash<lyra::GPUBufferDescriptor>
{
    std::size_t operator()(const lyra::GPUBufferDescriptor& d) const
    {
        std::size_t res = 0;
        lyra::hash_combine(res, d.size);
        lyra::hash_combine(res, d.usage.value);
        lyra::hash_combine(res, d.virtual_address);
        return res;
    }
};

template <>
struct std::hash<lyra::GPUTextureDescriptor>
{
    std::size_t operator()(const lyra::GPUTextureDescriptor& d) const
    {
        std::size_t res = 0;
        lyra::hash_combine(res, d.size.width);
        lyra::hash_combine(res, d.size.height);
        lyra::hash_combine(res, d.size.depth);
        lyra::hash_combine(res, d.mip_level_count);
        lyra::hash_combine(res, d.array_layers);
        lyra::hash_combine(res, d.dimension);
        lyra::hash_combine(res, d.format);
        lyra::hash_combine(res, d.usage.value);
        return res;
    }
};

#endif // LYRA_LIBRARY_RENDER_RHI_HASH_H
