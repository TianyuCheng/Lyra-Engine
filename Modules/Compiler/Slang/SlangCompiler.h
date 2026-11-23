#pragma once

#ifndef LYRA_MODULE_COMPILER_SLANG_COMPILER_H
#define LYRA_MODULE_COMPILER_SLANG_COMPILER_H

#include "SlangCommon.h"
#include "SlangFileSystem.h"
#include "SlangReflection.h"
#include <Lyra/Compiler/SLCDescs.h>

using namespace lyra;

struct CompileResultInternal
{
    ComPtr<slang::ISession> session;
    ComPtr<slang::IModule>  module;
    CompileTarget           target = CompileTarget::SPIRV;

    // cached compiled binaries — keyed by entry point name.
    // blob.data pointers returned to callers are stable views into this storage;
    // they are freed automatically when this struct is destroyed (delete_module).
    HashMap<String, Vector<uint8_t>>             compiled_blobs;
    HashMap<String, ComPtr<ISlangSharedLibrary>> compiled_host_libs;

    auto get_entry_point(CString entry) const -> ComPtr<slang::IEntryPoint>;
    auto get_linked_program(CString entry) const -> ComPtr<slang::IComponentType>;
    auto get_composed_program(CString entry) const -> ComPtr<slang::IComponentType>;

    bool get_shader_blob(CString entry, ShaderBlob& blob);
    bool get_host_shader_blob(CString entry, slang::IComponentType* linked_program, ShaderBlob& blob);
};

struct CompilerWrapper
{
    ComPtr<slang::ISession>     session;
    ComPtr<slang::IModule>      builtin;
    Vector<String>              search_path_strings;
    FileLoader*                 fs_loader = nullptr;
    ComPtr<ISlangFileSystemExt> file_system;
    CompileTarget               target;

    static void init();

    explicit CompilerWrapper(const CompilerDescriptor& descriptor);

    auto select_profile(const CompilerDescriptor& descriptor) const -> SlangProfileID;
    auto select_target(const CompilerDescriptor& descriptor) const -> SlangCompileTarget;

    void init_builtin_module();

    bool compile(const CompileDescriptor& desc, CompileResultInternal& result);
    bool reflect(ShaderEntryPoints entries, ReflectResultInternal& result);
};

#endif // LYRA_MODULE_COMPILER_SLANG_COMPILER_H
