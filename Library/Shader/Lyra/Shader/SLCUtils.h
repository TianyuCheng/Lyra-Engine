#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_SLC_UTILS_H
#define LYRA_LIBRARY_PLUGIN_SLC_UTILS_H

#include <Lyra/Common/String.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Common/BitFlags.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Shader/SLCEnums.h>

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
            if (blob->data) {
                delete blob->data;
                blob->data = nullptr;
            }

            blob->size = 0;
        }
    };

    using OwnedShaderBlob = Own<ShaderBlob, ShaderBlobDeleter>;

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_SLC_UTILS_H
