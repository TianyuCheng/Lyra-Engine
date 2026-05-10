#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>

#include "helper.h"

using namespace lyra;

namespace fs = std::filesystem;

struct DummyAsset
{
    static constexpr CString     name = "DummyAsset";
    static constexpr AssetTypeID type = 9999;

    static auto loader() -> AssetLoaderAPI;
    static auto cooker() -> AssetCookerAPI;

    int value = 0;
};

static void* dummy_load(FileLoader* loader, FSPath path)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // simulate slow load
    auto asset   = new DummyAsset();
    auto content = loader->read<char>(path);
    if (!content.empty()) {
        asset->value = 100;
    } else {
        asset->value = 42;
    }
    return asset;
}

static void dummy_unload(void* data)
{
    delete static_cast<DummyAsset*>(data);
}

static uint dummy_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".dummy";
    }
    return 1;
}

static bool dummy_process(JSON& metadata, OSPath source_path, OSPath)
{
    metadata["path"]  = Path(source_path).string();
    metadata["value"] = 100;
    return true;
}

AssetLoaderAPI DummyAsset::loader()
{
    return {
        nullptr,
        dummy_load,
        dummy_unload,
        dummy_extensions};
}

AssetCookerAPI DummyAsset::cooker()
{
    return {
        nullptr,
        dummy_process,
        dummy_extensions};
}

TEST_CASE("ams::asset_server")
{
    // setup temporary directory for tests
    auto temp_dir = fs::temp_directory_path() / "lyra_ams_test";
    fs::create_directories(temp_dir);

    auto registry      = temp_dir / "Assets.toml";
    auto asset_path    = temp_dir / "test.dummy";
    auto metadata_path = temp_dir / "test.dummy.import";

    // create dummy asset and metadata
    {
        std::ofstream f(asset_path);
        f << "dummy data";
        f.close();
    }
    {
        JSON metadata;
        metadata["guid"]  = 123456789;
        metadata["value"] = 100;
        std::ofstream f(metadata_path);
        f << metadata.dump();
        f.close();
    }

    // initialize fileloader
    FileLoader loader(FSLoader::NATIVE);
    loader.mount("/", temp_dir.string().c_str(), 0);

    // initialize assetserver
    AMSDescriptor desc        = {};
    desc.workers              = 4;
    desc.registry             = registry.c_str();
    desc.loader.assets        = &loader;
    desc.importer.assets_path = temp_dir.c_str();

    AssetServer ams(desc);
    ams.register_asset<DummyAsset>();

    SUBCASE("Import Asset")
    {
        auto source_dir = temp_dir / "Source";
        auto caches_dir = temp_dir / "Caches";
        fs::create_directories(source_dir);
        fs::create_directories(caches_dir);

        // reconfigure ams for importing
        AMSDescriptor import_desc        = desc;
        import_desc.importer.assets_path = source_dir.c_str();
        import_desc.importer.caches_path = caches_dir.c_str();

        AssetServer import_ams(import_desc);
        import_ams.register_asset<DummyAsset>();
        import_ams.register_asset<DummyAsset, DummyAsset>(); // register as cooker too

        auto raw_path = source_dir / "new_test.dummy";
        {
            std::ofstream f(raw_path);
            f << "raw dummy content";
            f.close();
        }

        // ensure metadata does not exist from previous run
        Path expected_metadata = source_dir / "new_test.dummy.import";
        if (fs::exists(expected_metadata)) fs::remove(expected_metadata);

        auto future = import_ams.import_asset("new_test.dummy");
        auto guid   = future.get();
        CHECK_NE(guid, 0);

        // metadata should be created next to the source asset
        expected_metadata = source_dir / "new_test.dummy.import";

        // wait for async import to finish
        int timeout = 100; // 1 second
        while (!fs::exists(expected_metadata) && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        REQUIRE(fs::exists(expected_metadata));

        // verify metadata content
        std::ifstream f(expected_metadata);
        JSON          metadata = JSON::parse(f);
        CHECK_EQ(metadata["guid"].get<lyra::GUID>(), guid);
        CHECK_EQ(metadata["type"].get<lyra::AssetTypeID>(), DummyAsset::type);
        f.close();
    }

    SUBCASE("Basic Asynchronous Loading")
    {
        auto handle = ams.load_asset<DummyAsset>("test.dummy");
        CHECK(handle.valid());

        // should be loading (data is null)
        auto asset = ams.get_asset(handle);
        // it might be loaded already if the thread is fast, but usually it's null here
        // because we added a sleep in dummy_load.

        // wait for it to load
        int timeout = 100; // 1 second
        while (ams.get_asset(handle) == nullptr && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        asset = ams.get_asset(handle);
        REQUIRE_NE(asset, nullptr);
        CHECK_EQ(asset->value, 100);
    }

    SUBCASE("Reference Counting")
    {
        auto handle1 = ams.load_asset<DummyAsset>("test.dummy");
        auto handle2 = ams.load_asset<DummyAsset>("test.dummy");

        CHECK_EQ(handle1.uuid, handle2.uuid);

        // wait for load
        while (ams.get_asset(handle1) == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        ams.unload_asset(handle1);
        CHECK_NE(ams.get_asset(handle2), nullptr); // still in memory

        ams.unload_asset(handle2);
        // still in memory until purge
        CHECK_NE(ams.get_asset(handle2), nullptr);

        ams.purge();
        // should be gone
        CHECK_EQ(ams.get_asset(handle1), nullptr);
    }

    SUBCASE("Concurrent Loading")
    {
        constexpr int                        num_threads = 10;
        std::vector<std::thread>             threads;
        std::vector<AssetHandle<DummyAsset>> handles(num_threads);

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&ams, &handles, i]() {
                handles[i] = ams.load_asset<DummyAsset>("test.dummy");
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        for (int i = 0; i < num_threads; ++i) {
            CHECK(handles[i].valid());
            CHECK_EQ(handles[i].uuid, handles[0].uuid);
        }

        // wait for load
        while (ams.get_asset(handles[0]) == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        for (int i = 0; i < num_threads; ++i) {
            ams.unload_asset(handles[i]);
        }

        ams.purge();
        CHECK_EQ(ams.get_asset(handles[0]), nullptr);
    }

    // cleanup
    fs::remove_all(temp_dir);
}

TEST_CASE("ams::registry_dependencies")
{
    auto temp_dir = fs::temp_directory_path() / "lyra_registry_test";
    fs::create_directories(temp_dir);

    AssetID         parent_guid = 1;
    AssetID         dep1_guid   = 2;
    AssetID         dep2_guid   = 3;
    Vector<AssetID> deps        = {dep1_guid, dep2_guid};

    SUBCASE("Binary Serialization")
    {
        AssetRegistry reg;
        reg.update(parent_guid, "parent", 100, deps);
        reg.update(dep1_guid, "dep1", 100);
        reg.update(dep2_guid, "dep2", 100);

        auto bin_path = temp_dir / "reg_test.bin";
        CHECK(reg.save(bin_path.c_str()));

        AssetRegistry reg2;
        CHECK(reg2.load(bin_path.c_str()));

        auto loaded_deps = reg2.get_dependencies(parent_guid);
        CHECK_EQ(loaded_deps.size(), 2);
        CHECK_EQ(loaded_deps[0], dep1_guid);
        CHECK_EQ(loaded_deps[1], dep2_guid);
    }

    SUBCASE("TOML Serialization")
    {
        AssetRegistry reg;
        reg.update(parent_guid, "parent", 100, deps);

        auto toml_path = temp_dir / "reg_test.toml";
        CHECK(reg.save(toml_path.c_str()));

        AssetRegistry reg2;
        CHECK(reg2.load(toml_path.c_str()));

        auto loaded_deps = reg2.get_dependencies(parent_guid);
        CHECK_EQ(loaded_deps.size(), 2);
        CHECK_EQ(loaded_deps[0], dep1_guid);
        CHECK_EQ(loaded_deps[1], dep2_guid);
    }

    fs::remove_all(temp_dir);
}

TEST_CASE("ams::asset_dependencies")
{
    auto temp_dir = fs::temp_directory_path() / "lyra_ams_dep_test";
    fs::create_directories(temp_dir);

    auto registry = temp_dir / "Assets.bin";

    // Setup parent and child metadata
    {
        JSON child_meta;
        child_meta["guid"] = 2001;
        child_meta["type"] = DummyAsset::type;
        std::ofstream f(temp_dir / "child.dummy.import");
        f << child_meta.dump();
    }
    {
        JSON parent_meta;
        parent_meta["guid"]         = 1001;
        parent_meta["type"]         = DummyAsset::type;
        parent_meta["dependencies"] = {2001};
        std::ofstream f(temp_dir / "parent.dummy.import");
        f << parent_meta.dump();
    }
    // Create actual dummy files
    {
        std::ofstream f(temp_dir / "parent.dummy");
        f << "p";
        std::ofstream f2(temp_dir / "child.dummy");
        f2 << "c";
    }

    FileLoader loader(FSLoader::NATIVE);
    loader.mount("/", temp_dir.string().c_str(), 0);

    AMSDescriptor desc        = {};
    desc.workers              = 1;
    desc.registry             = registry.c_str();
    desc.loader.assets        = &loader;
    desc.importer.assets_path = temp_dir.c_str();

    AssetServer ams(desc);
    ams.register_asset<DummyAsset>();

    SUBCASE("Recursive Loading")
    {
        auto parent_handle = ams.load_asset<DummyAsset>("parent.dummy");
        CHECK(parent_handle.valid());

        // Child should also be loading automatically
        AssetHandle<DummyAsset> child_handle{2001};

        // Wait for both to load
        int timeout = 100;
        while ((ams.get_asset(parent_handle) == nullptr || ams.get_asset(child_handle) == nullptr) && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        REQUIRE_NE(ams.get_asset(parent_handle), nullptr);
        REQUIRE_NE(ams.get_asset(child_handle), nullptr);
    }

    SUBCASE("Recursive Unloading")
    {
        auto                    parent_handle = ams.load_asset<DummyAsset>("parent.dummy");
        AssetHandle<DummyAsset> child_handle{2001};

        // Wait for load
        while (ams.get_asset(parent_handle) == nullptr || ams.get_asset(child_handle) == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Unload parent
        ams.unload_asset(parent_handle);

        // Purge
        ams.purge();

        // Both should be gone if child refcnt dropped to 0
        CHECK_EQ(ams.get_asset(parent_handle), nullptr);
        CHECK_EQ(ams.get_asset(child_handle), nullptr);
    }

    fs::remove_all(temp_dir);
}
