#include <Lyra/Utilities/Sync.h>
#include <Lyra/Rendering/ShadingModelRegistry.h>
#include <Lyra/Assets/Format/MaterialAsset.h>

using namespace lyra;

static HashMap<ulong, ShadingModelInfo> g_models;

static Mutex g_registry_mutex;
static bool  g_initialized = false;

ShadingModelID BuiltinShadingModels::from_enum(ShadingModel model)
{
    switch (model) {
        case ShadingModel::STANDARD:
            return STANDARD;
        case ShadingModel::CLEARCOAT:
            return CLEARCOAT;
        case ShadingModel::SUBSURFACE:
            return SUBSURFACE;
        case ShadingModel::UNLIT:
            return UNLIT;
        case ShadingModel::CUSTOM:
            return lyra::GUID{4};
        default:
            return STANDARD;
    }
}

static void register_builtin_models_unlocked()
{
    // register built-in default models as first-class citizens
    ShadingModelInfo standard{
        .id                 = BuiltinShadingModels::STANDARD,
        .name               = "Standard",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_standard",
        .struct_name        = "StandardShadingModel",
    };
    g_models[standard.id] = standard;

    ShadingModelInfo clearcoat{
        .id                 = BuiltinShadingModels::CLEARCOAT,
        .name               = "Clearcoat",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_clearcoat",
        .struct_name        = "ClearcoatShadingModel",
    };
    g_models[clearcoat.id] = clearcoat;

    ShadingModelInfo subsurface{
        .id                 = BuiltinShadingModels::SUBSURFACE,
        .name               = "Subsurface",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_subsurface",
        .struct_name        = "SubsurfaceShadingModel",
    };
    g_models[subsurface.id] = subsurface;

    ShadingModelInfo unlit{
        .id                 = BuiltinShadingModels::UNLIT,
        .name               = "Unlit",
        .shader_module_path = "sys://Shaders/Forward/Forward.slang",
        .entry_point_name   = "fs_unlit",
        .struct_name        = "UnlitShadingModel",
    };
    g_models[unlit.id] = unlit;
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

Optional<ShadingModelInfo> ShadingModelRegistry::get_model(ShadingModelID id)
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
