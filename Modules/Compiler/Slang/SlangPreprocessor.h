#pragma once

#ifndef LYRA_MODULE_COMPILER_SLANG_PREPROCESSOR_H
#define LYRA_MODULE_COMPILER_SLANG_PREPROCESSOR_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Compiler/SLCEnums.h>

namespace lyra
{

    struct SlangPreprocessor
    {
        static auto process(const String& input, CompileTarget target) -> String;

    private:
        static auto process_host_target(const String& input, String& wrappers) -> String;
        static auto process_attributes(const String& input, CompileTarget target) -> String;
        static auto get_reg_type(const String& decl) -> char;
    };

} // namespace lyra

#endif // LYRA_MODULE_COMPILER_SLANG_PREPROCESSOR_H
