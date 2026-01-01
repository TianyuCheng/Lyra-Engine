#include "MetalUtils.h"

using namespace lyra;

MetalShader::MetalShader()
{
    // do nothing
}

MetalShader::MetalShader(const GPUShaderModuleDescriptor& desc)
{
    NSError* error = nil;

    auto rhi = get_rhi();

    // load from compiled metallib or compile from source
    dispatch_data_t data = dispatch_data_create(desc.data, desc.size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    library              = [rhi->device newLibraryWithData:data error:&error];

    if (!library || error) {
        NSString* error_str = error ? [error localizedDescription] : @"unknown error";
        get_logger()->error("Failed to load Metal shader: {}", [error_str UTF8String]);
        throw GPUCompilationInfo([NSString stringWithFormat:@"Failed to load Metal shader: %@", error_str].UTF8String);
    }
}

void MetalShader::destroy()
{
    library = nil;
}
