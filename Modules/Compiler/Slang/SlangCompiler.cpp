#include "SlangCompiler.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <Lyra/FileSystem/VFSTypes.h>
#include <Lyra/FileSystem/VFSPath.h>

using namespace lyra;

static CString builtin_module_source = R"""(
module lyra;

// a custom attribute to indicate if a constant buffer is dynamic uniform buffer
// example usage: [lyra::dynamic]
[__AttributeUsage(_AttributeTargets.Var)]
public struct lyra_dynamicAttribute { };
[__AttributeUsage(_AttributeTargets.Var)]
public struct dynamicAttribute { };

// a custom attribute to mark immediates
// example usage: [[lyra::immediates]] or [lyra::immediates]
[__AttributeUsage(_AttributeTargets.Var)]
public struct lyra_immediatesAttribute { };
[__AttributeUsage(_AttributeTargets.Var)]
public struct immediatesAttribute { };

// explicit bind group / descriptor set assignment for ParameterBlock
// example usage: [[lyra::group(0)]] or [[lyra::set(0)]]
[__AttributeUsage(_AttributeTargets.Var)]
public struct lyra_groupAttribute { public int index; };
[__AttributeUsage(_AttributeTargets.Var)]
public struct groupAttribute { public int index; };

[__AttributeUsage(_AttributeTargets.Var)]
public struct lyra_setAttribute { public int index; };
[__AttributeUsage(_AttributeTargets.Var)]
public struct setAttribute { public int index; };

// explicit binding slot assignment
// example usage: [[lyra::binding(0)]]
[__AttributeUsage(_AttributeTargets.Var)]
public struct lyra_bindingAttribute { public int index; };
[__AttributeUsage(_AttributeTargets.Var)]
public struct bindingAttribute { public int index; };
)""";

static uint get_shader_entry_point_index(slang::ProgramLayout* layout, slang::EntryPointReflection* refl)
{
    for (uint i = 0; i < layout->getEntryPointCount(); i++)
        if (layout->getEntryPointByIndex(i) == refl)
            return i;

    assert(!"Failed to find shader entry point!");
    return ~0u;
}

#pragma region CompilerWrapper
void CompilerWrapper::init()
{
    createGlobalSession(get_global_session().writeRef());
}

CompilerWrapper::CompilerWrapper(const CompilerDescriptor& descriptor)
{
    this->target    = descriptor.target;
    this->fs_loader = descriptor.fs_loader;

    Vector<slang::CompilerOptionEntry> options;

    static String immediates_key = "IMMEDIATES";
    static String immediates_val = "register(b0, space" + std::to_string(D3D12_ImmediateRegisterSpace) + ")";
    // metal has no register spaces; we intentionally use METAL_ImmediateBufferIndex for both the buffer
    // slot (b) and the space field so reflection can identify the immediates by matching both values.
    // create_immediates() mirrors this assumption when validating the reflected offset.space.
    static String immediates_msl = "register(b" + std::to_string(METAL_ImmediateBufferIndex) + ", space" + std::to_string(METAL_ImmediateBufferIndex) + ")";

    // special treatment for immediates
    {
        auto entry               = slang::CompilerOptionEntry{};
        entry.name               = slang::CompilerOptionName::MacroDefine;
        entry.value.kind         = slang::CompilerOptionValueKind::String;
        entry.value.stringValue0 = immediates_key.c_str();
        entry.value.stringValue1 = target == CompileTarget::MSL
                                       ? immediates_msl.c_str()
                                       : immediates_val.c_str();
        options.push_back(entry);
    }

    // invert y-axis for spirv-based shaders
    if (target == CompileTarget::SPIRV) {
        auto entry            = slang::CompilerOptionEntry{};
        entry.name            = slang::CompilerOptionName::VulkanInvertY;
        entry.value.intValue0 = 1;
        options.push_back(entry);
    }

    // include dirs
    for (auto& include : descriptor.includes) {
        auto entry               = slang::CompilerOptionEntry{};
        entry.name               = slang::CompilerOptionName::Include;
        entry.value.kind         = slang::CompilerOptionValueKind::String;
        entry.value.stringValue0 = include;
        options.push_back(entry);
    }

    // preprocessor macros
    for (auto& macro : descriptor.defines) {
        auto entry               = slang::CompilerOptionEntry{};
        entry.name               = slang::CompilerOptionName::MacroDefine;
        entry.value.kind         = slang::CompilerOptionValueKind::String;
        entry.value.stringValue0 = macro.key;
        entry.value.stringValue1 = macro.value;
        options.push_back(entry);
    }

    // emit option (SPIRV)
    if (target == CompileTarget::SPIRV) {
        auto emit            = slang::CompilerOptionEntry{};
        emit.name            = slang::CompilerOptionName::EmitSpirvDirectly;
        emit.value.kind      = slang::CompilerOptionValueKind::Int;
        emit.value.intValue0 = 1;
        options.push_back(emit);
    }

    // preserve entry name (SPIRV)
    if (target == CompileTarget::SPIRV) {
        auto emit            = slang::CompilerOptionEntry{};
        emit.name            = slang::CompilerOptionName::VulkanUseEntryPointName;
        emit.value.kind      = slang::CompilerOptionValueKind::Int;
        emit.value.intValue0 = 1;
        options.push_back(emit);
    }

    auto target_desc    = slang::TargetDesc{};
    target_desc.format  = select_target(descriptor);
    target_desc.profile = select_profile(descriptor);

    Vector<CString> search_paths;
    Vector<String>  all_os_search_dirs;
    for (auto& inc : descriptor.includes) {
        if (inc && inc[0] != '\0') {
            search_paths.push_back(inc);
            search_path_strings.push_back(inc);
            all_os_search_dirs.push_back(inc);
        }
    }

    search_paths.push_back(".");
    all_os_search_dirs.push_back(".");

    file_system = ComPtr<ISlangFileSystemExt>(new SlangVFSFileSystem(fs_loader, all_os_search_dirs, target));

    auto session_desc                     = slang::SessionDesc{};
    session_desc.targets                  = &target_desc;
    session_desc.targetCount              = 1;
    session_desc.fileSystem               = file_system.get();
    session_desc.compilerOptionEntryCount = static_cast<uint>(options.size());
    session_desc.compilerOptionEntries    = options.data();
    session_desc.searchPaths              = search_paths.data();
    session_desc.searchPathCount          = static_cast<SlangInt>(search_paths.size());

    if (SLANG_FAILED(get_global_session()->createSession(session_desc, session.writeRef()))) {
        get_logger()->error("Failed to create Slang session!");
    }

    // some common stuff that might be used in other shaders
    init_builtin_module();
}

SlangProfileID CompilerWrapper::select_profile(const CompilerDescriptor& descriptor) const
{
    switch (descriptor.target) {
        case CompileTarget::MSL:
            // metallib_2_4 is the minimal version supporting ray tracing on Metal
            return get_global_session()->findProfile("metallib_2_4");
        case CompileTarget::DXIL:
        case CompileTarget::SPIRV:
        default:
            // sm_6_6 is the minimal version supporting ray tracing
            return get_global_session()->findProfile("sm_6_6");
    }
}

SlangCompileTarget CompilerWrapper::select_target(const CompilerDescriptor& descriptor) const
{
    switch (descriptor.target) {
        case CompileTarget::MSL:
            return SLANG_METAL_LIB;
        case CompileTarget::DXIL:
            return SLANG_DXIL;
        case CompileTarget::HOST:
            return SLANG_SHADER_HOST_CALLABLE;
        case CompileTarget::SPIRV:
        default:
            return SLANG_SPIRV;
    }
}

void CompilerWrapper::init_builtin_module()
{
    // create shader module
    ComPtr<slang::IBlob> diagnostics;
    builtin = session->loadModuleFromSourceString(
        "lyra",                  // module name
        "lyra.slang",            // module path
        builtin_module_source,   // shader source code
        diagnostics.writeRef()); // optional diagnostic container
    diagnose_if_needed(diagnostics, builtin != nullptr ? SLANG_OK : SLANG_FAIL);
}

bool CompilerWrapper::compile(const CompileDescriptor& desc, CompileResultInternal& result)
{
    // compile result shares the session
    result.session = session;
    result.target  = target;

    String source_text;
    if (desc.source && std::strlen(desc.source) > 0) {
        source_text = desc.source;
    } else if (desc.path && std::strlen(desc.path) > 0) {
        if (fs_loader && VFSPath::has_protocol(desc.path)) {
            auto data = fs_loader->read<char>(desc.path);
            if (!data.empty()) {
                source_text = String(data.data(), data.size());
            }
        } else {
            std::filesystem::path p(desc.path);
            if (std::filesystem::exists(p)) {
                std::ifstream file(p, std::ios::binary);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    source_text = buffer.str();
                }
            } else {
                for (const auto& sp : search_path_strings) {
                    if (fs_loader && VFSPath::has_protocol(sp)) {
                        String vpath = sp;
                        if (!vpath.empty() && vpath.back() != '/' && vpath.back() != '\\') {
                            vpath += '/';
                        }
                        vpath += desc.path;
                        if (fs_loader->exists(vpath.c_str())) {
                            auto data = fs_loader->read<char>(vpath.c_str());
                            if (!data.empty()) {
                                source_text = String(data.data(), data.size());
                                break;
                            }
                        }
                    } else if (!VFSPath::has_protocol(sp)) {
                        auto candidate = std::filesystem::path(sp) / p;
                        if (std::filesystem::exists(candidate)) {
                            std::ifstream file(candidate, std::ios::binary);
                            if (file.is_open()) {
                                std::stringstream buffer;
                                buffer << file.rdbuf();
                                source_text = buffer.str();
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // create shader module
    ComPtr<slang::IBlob> diagnostics;
    if (!source_text.empty()) {
        String processed_source = preprocess_lyra_shader_source(source_text, target);
        result.module           = session->loadModuleFromSourceString(
            desc.module,                         // module name
            desc.path ? desc.path : desc.module, // module path
            processed_source.c_str(),            // shader source code
            diagnostics.writeRef());             // optional diagnostic container
    } else {
        result.module = session->loadModule(
            desc.module,
            diagnostics.writeRef());
    }
    diagnose_if_needed(diagnostics, result.module != nullptr ? SLANG_OK : SLANG_FAIL);

    return result.module != nullptr;
}

bool CompilerWrapper::reflect(ShaderEntryPoints entries, ReflectResultInternal& result)
{
    // make sure single module is added exactly once
    HashSet<slang::IComponentType*> modules;
    for (auto& entry : entries) {
        auto module = reinterpret_cast<CompileResultInternal*>(entry.module.pointer);
        modules.insert(module->module);
    }

    // find all required shader entry points
    Vector<slang::IComponentType*>     component_types(modules.begin(), modules.end());
    Vector<ComPtr<slang::IEntryPoint>> entry_points;
    for (auto& entry : entries) {
        auto module = reinterpret_cast<CompileResultInternal*>(entry.module.pointer);
        auto entryp = module->get_entry_point(entry.entry);
        if (!entryp) {
            return false;
        }
        entry_points.push_back(entryp);
        component_types.push_back(entryp);
    }

    // create a composed program for reflection
    ComPtr<slang::IComponentType> composed_program;
    {
        ComPtr<slang::IBlob> diagnostics;
        SlangResult          res = session->createCompositeComponentType(
            component_types.data(),
            component_types.size(),
            composed_program.writeRef(),
            diagnostics.writeRef());
        diagnose_if_needed(diagnostics, res);
        SLANG_RETURN_FALSE_ON_FAIL(res);
    }

    // link the program
    ComPtr<slang::IComponentType> linked_program;
    {
        ComPtr<slang::IBlob> diagnostics;
        auto                 res = composed_program->link(
            linked_program.writeRef(),
            diagnostics.writeRef());
        diagnose_if_needed(diagnostics, res);
        SLANG_RETURN_FALSE_ON_FAIL(res);
    }

    // associate stages with ICompileRequests*
    auto layout = linked_program->getLayout();
    for (auto& entry : entries) {
        auto refl = layout->findEntryPointByName(entry.entry);
        if (!refl) {
            String wrapper = String(entry.entry) + "_Callable";
            refl           = layout->findEntryPointByName(wrapper.c_str());
        }
        if (!refl) {
            get_logger()->error("Failed to find entry point for reflection: {}", entry.entry);
            return false;
        }

        uint index = get_shader_entry_point_index(layout, refl);
        auto stage = to_stage(refl->getStage());
        if (target == CompileTarget::HOST && stage == GPUShaderStage::COMPUTE) {
            String name = entry.entry;
            if (name.rfind("vs", 0) == 0 || name.find("vert") != String::npos) {
                stage = GPUShaderStage::VERTEX;
            } else if (name.rfind("fs", 0) == 0 || name.find("frag") != String::npos) {
                stage = GPUShaderStage::FRAGMENT;
            }
        }

        slang::IMetadata*    metadata;
        ComPtr<slang::IBlob> diagnostics;

        auto res = linked_program->getEntryPointMetadata(index, 0, &metadata, diagnostics.writeRef());
        diagnose_if_needed(diagnostics, res);
        SLANG_RETURN_FALSE_ON_FAIL(res);

        result.metadata.push_back(EntryMetadata{stage, metadata});
    }

    result.target = target;
    result.init(layout);
    return !result.has_error;
}
#pragma endregion CompilerWrapper

#pragma region CompileResultInternal
ComPtr<slang::IEntryPoint> CompileResultInternal::get_entry_point(CString entry) const
{
    // query entry point
    ComPtr<slang::IEntryPoint> entry_point;
    module->findEntryPointByName(entry, entry_point.writeRef());

    if (!entry_point) {
        String wrapper = String(entry) + "_Callable";
        module->findEntryPointByName(wrapper.c_str(), entry_point.writeRef());
    }

    if (!entry_point) {
        get_logger()->error("Failed to get entry point: {}", entry);
        return nullptr;
    }
    return entry_point;
}

ComPtr<slang::IComponentType> CompileResultInternal::get_linked_program(CString entry) const
{
    auto composed_program = get_composed_program(entry);
    if (composed_program == nullptr)
        return nullptr;

    // link
    ComPtr<slang::IComponentType> linked_program;
    ComPtr<slang::IBlob>          diagnostics;

    SlangResult result = composed_program->link(
        linked_program.writeRef(),
        diagnostics.writeRef());
    diagnose_if_needed(diagnostics, result);

    if (SLANG_FAILED(result)) {
        get_logger()->error("Failed to link program: {}", entry);
        return nullptr;
    }
    return linked_program;
}

ComPtr<slang::IComponentType> CompileResultInternal::get_composed_program(CString entry) const
{
    auto entry_point = get_entry_point(entry);
    if (entry_point == nullptr)
        return nullptr;

    // compose modules + entry points
    std::array<slang::IComponentType*, 2> component_types = {module, entry_point};
    ComPtr<slang::IComponentType>         composed_program;
    ComPtr<slang::IBlob>                  diagnostics;

    SlangResult result = session->createCompositeComponentType(
        component_types.data(),
        component_types.size(),
        composed_program.writeRef(),
        diagnostics.writeRef());
    diagnose_if_needed(diagnostics, result);

    if (SLANG_FAILED(result)) {
        get_logger()->error("Failed to compose program: {}", entry);
        return nullptr;
    }
    return composed_program;
}

bool CompileResultInternal::get_shader_blob(CString entry, ShaderBlob& blob)
{
    auto linked_program = get_linked_program(entry);
    if (linked_program == nullptr)
        return false;

    if (target == CompileTarget::HOST) {
        ComPtr<ISlangSharedLibrary> shared_lib;
        ComPtr<slang::IBlob>        diagnostics;

        SlangResult result = linked_program->getEntryPointHostCallable(0, 0, shared_lib.writeRef(), diagnostics.writeRef());
        diagnose_if_needed(diagnostics, result);
        SLANG_RETURN_FALSE_ON_FAIL(result);

        compiled_host_libs[entry] = shared_lib;

        void* func_ptr = shared_lib->findSymbolAddressByName(entry);
        if (!func_ptr) {
            String wrapper = String(entry) + "_Callable";
            func_ptr       = shared_lib->findSymbolAddressByName(wrapper.c_str());
        }

        auto& cached = compiled_blobs[entry];
        cached.resize(sizeof(HostCallableShader));
        auto header        = reinterpret_cast<HostCallableShader*>(cached.data());
        header->magic      = HOST_MAGIC;
        header->library    = shared_lib.get();
        header->entry_func = func_ptr;

        blob.size = static_cast<uint32_t>(cached.size());
        blob.data = cached.data();
        return true;
    }

    // kernel binary
    ComPtr<slang::IBlob> spirv_code;
    ComPtr<slang::IBlob> diagnostics;

    SlangResult result;
    result = linked_program->getEntryPointCode(
        0,
        0,
        spirv_code.writeRef(),
        diagnostics.writeRef());
    diagnose_if_needed(diagnostics, result);
    if (SLANG_FAILED(result)) {
        get_logger()->error("getEntryPointCode for '{}' returned result: 0x{:08x}", entry, (uint32_t)result);
    }
    SLANG_RETURN_FALSE_ON_FAIL(result);

    // copy into engine-owned storage so callers get a stable non-owning view;
    // memory is freed when this CompileResultInternal is destroyed (delete_module).
    auto& cached = compiled_blobs[entry];
    cached.resize(spirv_code->getBufferSize());
    std::memcpy(cached.data(), spirv_code->getBufferPointer(), cached.size());

    blob.size = static_cast<uint32_t>(cached.size());
    blob.data = cached.data();
    return true;
}
#pragma endregion CompileResultInternal
