#include <Lyra/Assets/AMSRegistry.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Collections.h>
#include <Lyra/Common/Path.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Config.h>

#include <fstream>
#include <filesystem>
#include <cstring>

#include <toml++/toml.hpp>

using namespace lyra;

/**
 * binary file structure:
 * <char[4]> magic "LYRA"
 * <uint> version (default 0)
 * <uint> entry count
 * <AssetEntry[]> array of entries (guid, type, path index)
 * <uint> string table count
 * <uint8_t:length> <string content> (repeated for each string)
 */

static constexpr uint    REGISTRY_VERSION = 0;
static constexpr CString REGISTRY_MAGIC   = "LYRA";

bool AssetRegistry::load(const OSPath& path)
{
    auto ext = Path(path).extension().string();
    if (!fs::exists(path)) {
        return false;
    }
    if (ext == ".bin") {
        return load_binary(path);
    }
    if (ext == ".toml") {
        return load_toml(path);
    }
    spdlog::error("Unsupported registry file: {}", path);
    return false;
}

bool AssetRegistry::save(const OSPath& path)
{
    auto ext = Path(path).extension().string();
    if (!fs::exists(path)) {
        return false;
    }
    if (ext == ".bin") {
        return save_binary(path);
    }
    if (ext == ".toml") {
        return save_toml(path);
    }
    spdlog::error("Unsupported registry file: {}", path);
    return false;
}

bool AssetRegistry::flush(const OSPath& path)
{
    if (!dirty) {
        return false;
    }
    if (save(path)) {
        dirty = false;
        return true;
    }
    return false;
}

bool AssetRegistry::load_binary(const OSPath& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.good()) return false;

    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);

    Vector<char> data(size);
    if (!f.read(data.data(), size)) return false;
    f.close();

    CString ptr = data.data();
    CString end = ptr + size;

    // magic
    if (ptr + 4 > end || std::memcmp(ptr, REGISTRY_MAGIC, 4) != 0) return false;
    ptr += 4;

    // version
    if (ptr + sizeof(uint) > end) return false;
    uint version = *reinterpret_cast<const uint*>(ptr);
    ptr += sizeof(uint);
    (void)version; // ignore for now

    // entries count
    if (ptr + sizeof(uint) > end) return false;
    uint entry_count = *reinterpret_cast<const uint*>(ptr);
    ptr += sizeof(uint);

    entries.resize(entry_count);
    if (entry_count > 0) {
        size_t entries_size = entry_count * sizeof(AssetEntry);
        if (ptr + entries_size > end) return false;
        std::memcpy(entries.data(), ptr, entries_size);
        ptr += entries_size;
    }

    // string table count
    if (ptr + sizeof(uint) > end) return false;
    uint string_count = *reinterpret_cast<const uint*>(ptr);
    ptr += sizeof(uint);

    string_table.resize(string_count);
    for (uint i = 0; i < string_count; ++i) {
        if (ptr + sizeof(uint8_t) > end) return false;
        uint8_t length = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += sizeof(uint8_t);

        if (ptr + length > end) return false;
        string_table[i].assign(ptr, length);
        ptr += length;
    }

    build_lookup_tables();
    dirty = false;
    return true;
}

bool AssetRegistry::save_binary(const OSPath& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f.good()) return false;

    f.write(REGISTRY_MAGIC, 4);
    f.write(reinterpret_cast<CString>(&REGISTRY_VERSION), sizeof(uint));

    uint entry_count = static_cast<uint>(entries.size());
    f.write(reinterpret_cast<CString>(&entry_count), sizeof(uint));
    if (entry_count > 0) {
        f.write(reinterpret_cast<CString>(entries.data()), entry_count * sizeof(AssetEntry));
    }

    uint string_count = static_cast<uint>(string_table.size());
    f.write(reinterpret_cast<CString>(&string_count), sizeof(uint));
    for (const auto& str : string_table) {
        uint8_t length = static_cast<uint8_t>(str.length());
        f.write(reinterpret_cast<CString>(&length), sizeof(uint8_t));
        f.write(str.data(), length);
    }

    f.close();
    return true;
}

bool AssetRegistry::load_toml(const OSPath& path)
{
    try {
        auto config = toml::parse_file(path);
        entries.clear();
        string_table.clear();

        if (auto arr = config["entries"].as_array()) {
            arr->for_each([this](auto& entry) {
                if (auto e = entry.as_array()) {
                    AssetID     guid     = e->get(0)->as_integer()->get();
                    AssetTypeID type     = static_cast<AssetTypeID>(e->get(1)->as_integer()->get());
                    String      path_str = e->get(2)->as_string()->get();
                    update(guid, path_str, type);
                }
            });
        }

        build_lookup_tables();
        dirty = false;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load TOML registry: {}", e.what());
        return false;
    }
}

bool AssetRegistry::save_toml(const OSPath& path)
{
    std::ofstream f(path);
    if (!f.good()) return false;

    toml::array entries_arr;
    for (const auto& entry : entries) {
        toml::array e;
        e.push_back(static_cast<int64_t>(entry.guid));
        e.push_back(static_cast<int64_t>(entry.type));
        e.push_back(string_table[entry.path]);
        entries_arr.push_back(e);
    }

    toml::table root;
    root.insert("entries", entries_arr);

    f << root;
    f.close();
    return true;
}

void AssetRegistry::rebuild(const OSPath& assets_dir)
{
    entries.clear();
    string_table.clear();
    guid_to_entry_index.clear();
    path_to_guid.clear();

    namespace fs = std::filesystem;
    if (!fs::exists(assets_dir) || !fs::is_directory(assets_dir)) {
        spdlog::error("Asset directory does not exist: {}", assets_dir);
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(assets_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".import") {
            Path import_path = entry.path();

            std::ifstream f(import_path);
            if (!f.good()) continue;

            try {
                JSON metadata = JSON::parse(f);
                if (metadata.contains("guid")) {
                    AssetID     guid = metadata["guid"].get<AssetID>();
                    AssetTypeID type = 0;
                    if (metadata.contains("type")) {
                        type = metadata["type"].get<AssetTypeID>();
                    }

                    // path relative to assets_dir, without .import
                    String relative_path = fs::relative(import_path, assets_dir).string();
                    // remove .import
                    relative_path = relative_path.substr(0, relative_path.find_last_of('.'));

                    update(guid, relative_path, type);
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to parse metadata from {}: {}", import_path.string(), e.what());
            }
        }
    }
    dirty = true;
}

void AssetRegistry::update(AssetID guid, StringView path, AssetTypeID type)
{
    uint string_index = 0xFFFFFFFF;
    for (uint i = 0; i < string_table.size(); ++i) {
        if (string_table[i] == path) {
            string_index = i;
            break;
        }
    }

    if (string_index == 0xFFFFFFFF) {
        string_index = static_cast<uint>(string_table.size());
        string_table.push_back(String(path));
    }

    auto it = guid_to_entry_index.find(guid);
    if (it != guid_to_entry_index.end()) {
        uint entry_index          = it->second;
        entries[entry_index].path = string_index;
        entries[entry_index].type = type;
    } else {
        uint entry_index = static_cast<uint>(entries.size());
        entries.push_back({guid, type, string_index});
        guid_to_entry_index[guid] = entry_index;
    }

    path_to_guid[string_table[string_index]] = guid;
    dirty                                    = true;
}

StringView AssetRegistry::get_path(AssetID guid) const
{
    auto it = guid_to_entry_index.find(guid);
    if (it != guid_to_entry_index.end()) {
        return string_table[entries[it->second].path];
    }
    return "";
}

AssetID AssetRegistry::get_guid(StringView path) const
{
    auto it = path_to_guid.find(path);
    if (it != path_to_guid.end()) {
        return it->second;
    }
    return 0;
}

AssetTypeID AssetRegistry::get_type(AssetID guid) const
{
    auto it = guid_to_entry_index.find(guid);
    if (it != guid_to_entry_index.end()) {
        return entries[it->second].type;
    }
    return 0;
}

AssetID AssetRegistry::generate_guid()
{
    AssetID guid;
    do {
        guid = random_guid();
    } while (guid == 0 || guid_to_entry_index.find(guid) != guid_to_entry_index.end());
    return guid;
}

void AssetRegistry::build_lookup_tables()
{
    guid_to_entry_index.clear();
    path_to_guid.clear();
    for (uint i = 0; i < entries.size(); ++i) {
        guid_to_entry_index[entries[i].guid] = i;
        if (entries[i].path < string_table.size()) {
            path_to_guid[string_table[entries[i].path]] = entries[i].guid;
        }
    }
}
