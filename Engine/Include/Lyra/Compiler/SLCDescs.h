#pragma once

#ifndef LYRA_ENGINE_COMPILER_SLCDESCS_H
#define LYRA_ENGINE_COMPILER_SLCDESCS_H

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Compiler/SLCEnums.h>
#include <Lyra/Compiler/SLCUtils.h>
#include <Lyra/Graphics/RHIEnums.h>

namespace lyra
{
    struct FileLoader;

    struct CompilerDescriptor
    {
        CompileFlags   flags     = CompileFlag::NONE;
        CompileTarget  target    = CompileTarget::AUTO;
        ShaderDefines  defines   = {};
        ShaderIncludes includes  = {};
        FileLoader*    fs_loader = nullptr;
        LogLevel       log_level = LogLevel::warn;
    };

    struct CompileDescriptor
    {
        CString module;
        CString path;
        CString source;
    };

} // namespace lyra

#endif // LYRA_ENGINE_COMPILER_SLCDESCS_H
