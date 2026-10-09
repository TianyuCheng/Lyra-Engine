#pragma once

#ifndef LYRA_ENGINE_COMPILER_SLCUTILS_H
#define LYRA_ENGINE_COMPILER_SLCUTILS_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Handle.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Compiler/SLCEnums.h>

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
    using ShaderDefines          = TypedView<ShaderDefine>;
    using ShaderIncludes         = TypedView<ShaderInclude>;
    using CompilerHandle         = TypedPointerHandle<Compiler>;
    using ShaderModuleHandle     = TypedPointerHandle<ShaderModule>;
    using ShaderReflectionHandle = TypedPointerHandle<ShaderReflection>;
    using ShaderEntryPoints      = TypedView<ShaderEntryPoint>;
    using ShaderAttributes       = TypedView<ShaderAttribute>;
    using CompileFlags           = BitFlags<CompileFlag>;

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
    };

    extern "C" struct ShaderAttribute
    {
        CString name   = nullptr;
        uint    offset = 0;
    };

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
