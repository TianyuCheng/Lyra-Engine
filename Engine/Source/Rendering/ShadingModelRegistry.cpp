#include <Lyra/Utilities/Sync.h>
#include <Lyra/Rendering/ShadingModelRegistry.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

using namespace lyra;

static HashMap<ulong, ShadingModelInfo> g_models;

static Mutex g_registry_mutex;
static bool  g_initialized = false;

lyra::GUID BuiltinShadingModels::from_enum(ShadingModel model)
{
    switch (model) {
        case ShadingModel::PBR:
            return PBR;
        case ShadingModel::NPR:
            return NPR;
        case ShadingModel::UNLIT:
            return UNLIT;
        case ShadingModel::SUBSURFACE:
            return SUBSURFACE;
        case ShadingModel::CUSTOM:
            return lyra::GUID{4};
        default:
            return PBR;
    }
}

static void register_builtin_models_unlocked()
{
    // register built-in default models as first-class citizens
    ShadingModelInfo pbr{
        .id                 = BuiltinShadingModels::PBR,
        .name               = "PBR",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_pbr",
        .struct_name        = "PBRShadingModel",
    };
    g_models[pbr.id] = pbr;

    ShadingModelInfo npr{
        .id                 = BuiltinShadingModels::NPR,
        .name               = "NPR",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_npr",
        .struct_name        = "NPRShadingModel",
    };
    g_models[npr.id] = npr;

    ShadingModelInfo unlit{
        .id                 = BuiltinShadingModels::UNLIT,
        .name               = "Unlit",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_unlit",
        .struct_name        = "UnlitShadingModel",
    };
    g_models[unlit.id] = unlit;

    ShadingModelInfo subsurface{
        .id                 = BuiltinShadingModels::SUBSURFACE,
        .name               = "Subsurface",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_pbr",
        .struct_name        = "PBRShadingModel",
    };
    g_models[subsurface.id] = subsurface;
}

void ShadingModelRegistry::init()
{
    LockGuard<Mutex> lock(g_registry_mutex);
    if (g_initialized) {
        return;
    }
    register_builtin_models_unlocked();
    g_initialized = true;
}

void ShadingModelRegistry::shutdown()
{
    LockGuard<Mutex> lock(g_registry_mutex);
    g_models.clear();
    g_initialized = false;
}

void ShadingModelRegistry::register_model(const ShadingModelInfo& info)
{
    LockGuard<Mutex> lock(g_registry_mutex);
    if (!g_initialized) {
        register_builtin_models_unlocked();
        g_initialized = true;
    }
    g_models[info.id] = info;
}

auto ShadingModelRegistry::get_model(GUID id) -> Optional<ShadingModelInfo>
{
    LockGuard<Mutex> lock(g_registry_mutex);
    if (!g_initialized) {
        register_builtin_models_unlocked();
        g_initialized = true;
    }

    auto it = g_models.find(id);
    if (it != g_models.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ShadingModelRegistry::all_models() -> Vector<ShadingModelInfo>
{
    LockGuard<Mutex> lock(g_registry_mutex);
    if (!g_initialized) {
        register_builtin_models_unlocked();
        g_initialized = true;
    }

    Vector<ShadingModelInfo> result;
    result.reserve(g_models.size());
    for (const auto& [_, info] : g_models) {
        result.push_back(info);
    }
    return result;
}
