#include "MetalUtils.h"

using namespace lyra;

MetalShader::MetalShader()
{
    // do nothing
}

MetalShader::MetalShader(const GPUShaderModuleDescriptor& desc)
{
    auto rhi = get_rhi();
    NSError* error = nil;

    // load from compiled metallib or compile from source
    dispatch_data_t data = dispatch_data_create(desc.data, desc.size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    library = [rhi->device newLibraryWithData:data error:&error];

    if (error) {
        get_logger()->error("Failed to load Metal shader: {}", [[error localizedDescription] UTF8String]);
    }
}

void MetalShader::destroy()
{
    library = nil;
}

bool api::create_shader_module(GPUShaderModuleHandle& handle, const GPUShaderModuleDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalShader(desc);
    auto ind = rhi->shaders.add(obj);
    handle = GPUShaderModuleHandle(ind);
    return obj.valid();
}

void api::delete_shader_module(GPUShaderModuleHandle handle)
{
    get_rhi()->shaders.remove(handle.value);
}
