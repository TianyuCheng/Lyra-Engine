#pragma once

#ifndef LYRA_MODULE_COMPILER_SLANG_COMMON_H
#define LYRA_MODULE_COMPILER_SLANG_COMMON_H

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Compiler/SLCEnums.h>
#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIUtils.h>

using namespace lyra;
using Slang::ComPtr;

auto get_logger() -> Logger;
auto get_global_session() -> ComPtr<slang::IGlobalSession>&;

void diagnose_if_needed(slang::IBlob* diagnosticsBlob, SlangResult result = SLANG_FAIL);

auto to_string(GPUShaderStageFlags flags) -> String;
auto to_string(slang::TypeReflection::Kind kind) -> CString;
auto to_string(slang::ParameterCategory category) -> CString;
auto to_stage(SlangStage stage) -> GPUShaderStage;

auto preprocess_lyra_shader_source(const String& input, CompileTarget target) -> String;

template <typename T>
uint round_up_to_next_multiple_of(T size, T align)
{
    return (size + align - 1) / align * align;
}

#endif // LYRA_MODULE_COMPILER_SLANG_COMMON_H
