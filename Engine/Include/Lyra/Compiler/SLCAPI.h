#pragma once

#ifndef LYRA_ENGINE_COMPILER_SLCAPI_H
#define LYRA_ENGINE_COMPILER_SLCAPI_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Compiler/SLCEnums.h>
#include <Lyra/Compiler/SLCUtils.h>
#include <Lyra/Compiler/SLCDescs.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Graphics/RHIDescs.h>

namespace lyra
{
    struct ShaderAPI
    {
        // backend api name for shader compiler
        CString (*get_api_name)();

        bool (*create_compiler)(CompilerHandle& compiler, const CompilerDescriptor& descriptor);
        void (*delete_compiler)(CompilerHandle compiler);

        // compile from source
        bool (*create_module)(CompilerHandle compiler, const CompileDescriptor& desc, ShaderModuleHandle& module);
        void (*delete_module)(ShaderModuleHandle module);

        // reflect from modules
        bool (*create_reflection)(CompilerHandle compiler, ShaderEntryPoints entries, ShaderReflectionHandle& reflection);
        void (*delete_reflection)(ShaderReflectionHandle reflection);

        // retrieve shader blobs
        bool (*get_shader_blob)(ShaderEntryPoint entry, ShaderBlob& blob);
        bool (*get_vertex_attributes)(ShaderReflectionHandle reflection, ShaderAttributes attrs, GPUVertexAttribute* attributes);
        bool (*get_bind_group_layouts)(ShaderReflectionHandle reflection, uint& count, GPUBindGroupLayoutDescriptor* layouts);
        bool (*get_bind_group_location)(ShaderReflectionHandle reflection, CString name, uint& location);
        bool (*get_push_constant_ranges)(ShaderReflectionHandle reflection, uint& count, GPUPushConstantRange* ranges);
    };

} // namespace lyra

#endif // LYRA_ENGINE_COMPILER_SLCAPI_H
