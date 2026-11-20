#include "SoftBuffer.h"

using namespace lyra;
using namespace lyra::softgpu;

SoftBuffer::SoftBuffer(const GPUBufferDescriptor& desc)
    : size(desc.size), usage(desc.usage), label(desc.label)
{
    // allocate memory with alignment
    data.resize(static_cast<usize>(desc.size), 0);
    device_address = reinterpret_cast<uint64_t>(data.data());

    if (desc.mapped_at_creation) {
        map_state     = GPUMapState::MAPPED;
        map_mode      = GPUMapMode::WRITE;
        mapped_offset = 0;
        mapped_size   = desc.size;
    }
}

auto SoftBuffer::get_mapped_range(GPUSize64 offset, GPUSize64 len) -> MappedBufferRange
{
    if (len == 0) {
        len = (size > offset) ? (size - offset) : 0;
    }

    MappedBufferRange range{};
    if (data.empty() || offset >= size) {
        range.data = nullptr;
        range.size = 0;
        return range;
    }

    range.data = data.data() + offset;
    range.size = std::min(len, size - offset);
    return range;
}

void SoftBuffer::map(GPUMapMode mode, GPUSize64 offset, GPUSize64 len)
{
    if (len == 0) {
        len = (size > offset) ? (size - offset) : 0;
    }
    map_state     = GPUMapState::MAPPED;
    map_mode      = mode;
    mapped_offset = offset;
    mapped_size   = len;
}

void SoftBuffer::unmap()
{
    map_state     = GPUMapState::UNMAPPED;
    mapped_offset = 0;
    mapped_size   = 0;
}

void SoftBuffer::destroy()
{
    data.clear();
    data.shrink_to_fit();
    size           = 0;
    device_address = 0;
    map_state      = GPUMapState::UNMAPPED;
}
