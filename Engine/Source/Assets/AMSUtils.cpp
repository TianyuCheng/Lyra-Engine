#include <Lyra/Utilities/GUID.h>
#include <Lyra/Assets/AMSUtils.h>

using namespace lyra;

AssetDependencyScope::AssetDependencyScope(JSON& metadata, const Path& source_path, const Path& caches_root)
    : metadata(metadata), source_path(source_path), caches_root(caches_root)
{
    if (metadata.contains("dependencies") && metadata["dependencies"].is_array()) {
        for (const auto& item : metadata["dependencies"]) {
            if (item.is_object() && item.contains("name") && item.contains("guid")) {
                String  name    = item["name"].get<String>();
                AssetID guid    = item["guid"].get<AssetID>();
                prev_deps[name] = guid;
            }
        }
    }
}

AssetID AssetDependencyScope::resolve(AssetID parent_guid, StringView dep_name, AssetTypeID type, StringView cache_rel_path)
{
    String name_str(dep_name);

    // check if already resolved in this scope
    for (const auto& dep : new_deps) {
        if (dep.name == name_str) {
            return dep.guid;
        }
    }

    AssetID id = 0;

    // 1. check previous dependencies from incoming metadata
    auto it = prev_deps.find(name_str);
    if (it != prev_deps.end() && it->second != 0) {
        id = it->second;
    } else {
        // 2. deterministic generation
        id           = deterministic_guid(parent_guid, dep_name);
        uint attempt = 0;
        while (id == 0 || allocated_guids.find(id) != allocated_guids.end()) {
            id = probe_guid(id, attempt++);
        }
    }

    allocated_guids.insert(id);

    AssetDependencyEntry entry;
    entry.name = name_str;
    entry.guid = id;
    entry.type = type;
    entry.path = String(cache_rel_path);
    new_deps.push_back(std::move(entry));

    return id;
}

void AssetDependencyScope::set_path(AssetID id, StringView path)
{
    for (auto& dep : new_deps) {
        if (dep.guid == id) {
            dep.path = String(path);
            return;
        }
    }
}

void AssetDependencyScope::commit()
{
    JSON deps_arr = JSON::array();
    for (const auto& dep : new_deps) {
        JSON obj;
        if (!dep.name.empty()) obj["name"] = dep.name;
        obj["guid"] = dep.guid;
        if (dep.type.valid()) obj["type"] = to_string(dep.type);
        if (!dep.path.empty()) obj["path"] = dep.path;
        deps_arr.push_back(std::move(obj));
    }
    metadata["dependencies"] = std::move(deps_arr);
}
