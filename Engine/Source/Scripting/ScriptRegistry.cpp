#include <Lyra/Scripting/ScriptRegistry.h>

using namespace lyra;

static auto registry_factories() -> Vector<ScriptRegistry::Factory>&
{
    static Vector<ScriptRegistry::Factory> factories;
    return factories;
}

static auto registry_apis() -> Vector<ScriptAPI>&
{
    static Vector<ScriptAPI> apis;
    return apis;
}

void ScriptRegistry::add(Factory factory)
{
    if (factory) {
        registry_factories().push_back(factory);
    }
}

void ScriptRegistry::add(const ScriptAPI& api)
{
    registry_apis().push_back(api);
}

auto ScriptRegistry::get_factories() -> const Vector<ScriptRegistry::Factory>&
{
    return registry_factories();
}

auto ScriptRegistry::get_apis() -> const Vector<ScriptAPI>&
{
    return registry_apis();
}
