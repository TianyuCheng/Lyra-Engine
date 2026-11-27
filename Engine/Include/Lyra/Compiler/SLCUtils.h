#pragma once

#ifndef LYRA_ENGINE_COMPILER_SLCUTILS_H
#define LYRA_ENGINE_COMPILER_SLCUTILS_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Result.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Compiler/SLCEnums.h>
#include <Lyra/Graphics/RHIEnums.h>

ENABLE_BIT_FLAGS(lyra::CompileFlag);

namespace lyra
{
    struct Compiler;
    struct ShaderModule;
    struct ShaderReflection;
    struct ShaderEntryPoint;
    struct ShaderDefine;
    struct ShaderAttribute;

    using ShaderError            = CString;
    using ShaderInclude          = CString;
    using ShaderDefines          = Span<ShaderDefine>;
    using ShaderIncludes         = Span<ShaderInclude>;
    using CompilerHandle         = TypedPointerHandle<Compiler>;
    using ShaderModuleHandle     = TypedPointerHandle<ShaderModule>;
    using ShaderReflectionHandle = TypedPointerHandle<ShaderReflection>;
    using ShaderEntryPoints      = Span<ShaderEntryPoint>;
    using ShaderAttributes       = Span<ShaderAttribute>;
    using CompileFlags           = BitFlags<CompileFlag>;

    constexpr uint32_t HOST_MAGIC = 0x534C4843; // 'SLHC'

    extern "C" struct ShaderDefine
    {
        CString key;
        CString value;
    };

    extern "C" struct ShaderBlob
    {
        uint8_t* data;
        uint     size;
    };

    extern "C" struct ShaderEntryPoint
    {
        ShaderModuleHandle module;
        CString            entry;

#ifdef __cplusplus
        ShaderEntryPoint() = default;
        ShaderEntryPoint(ShaderModuleHandle mod, CString ent) : module(mod), entry(ent) {}
        ShaderEntryPoint(const ShaderModule& mod, CString ent);
        ShaderEntryPoint(const Own<ShaderModule>& mod, CString ent);
        ShaderEntryPoint(const ShaderModule* mod, CString ent);
        ShaderEntryPoint(const Result<Own<ShaderModule>>& mod, CString ent);
#endif
    };

    extern "C" struct ShaderAttribute
    {
        CString name   = nullptr;
        uint    offset = 0;
    };

    struct HostParamBinding
    {
        ushort  block_offset  = 0xFFFF;
        ushort  offset        = 0;
        ushort  size          = 0;
        uint8_t set           = 0;
        uint8_t binding       = 0;
        uint8_t is_immediates = 0;
        uint8_t padding       = 0;
    };

    extern "C" struct HostCallableShader
    {
        uint             magic;
        void*            library;
        void*            entry_func;
        uint             param_count;
        HostParamBinding params[32];
        char             name[64];
        ushort           depth_offset = 0xFFFF;
        ushort           flags        = 0;
    };

    constexpr auto resolve_compile_target(CompileTarget target, RHIBackend backend) -> CompileTarget
    {
        if (target != CompileTarget::AUTO) {
            return target;
        }
        switch (backend) {
            case RHIBackend::VULKAN:
                return CompileTarget::SPIRV;
            case RHIBackend::D3D12:
                return CompileTarget::DXIL;
            case RHIBackend::METAL:
                return CompileTarget::MSL;
            case RHIBackend::SOFTGPU:
                return CompileTarget::HOST;
            default:
                return CompileTarget::SPIRV;
        }
    }

    struct ShaderBlobDeleter
    {
        void operator()(ShaderBlob* blob)
        {
            delete blob;
        }
    };

    using OwnedShaderBlob = Own<ShaderBlob, ShaderBlobDeleter>;

} // namespace lyra

#endif // LYRA_ENGINE_COMPILER_SLCUTILS_H
