// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

D3D12Shader::D3D12Shader()
{
    // do nothing
}

D3D12Shader::D3D12Shader(const GPUShaderModuleDescriptor& desc)
{
    binary.clear();
    binary.reserve(desc.size);
    std::copy(desc.data, desc.data + desc.size, std::back_inserter(binary));
}

void D3D12Shader::destroy()
{
    binary.clear();
}
