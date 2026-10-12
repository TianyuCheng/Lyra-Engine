#include <doctest/doctest.h>
#include <Lyra/Rendering/Geometry.h>
#include <Lyra/Rendering/GeometryBufferPool.h>
#include <Lyra/Rendering/GeometryCache.h>
#include <Lyra/Rendering/ResourceRetire.h>

using namespace lyra;

TEST_CASE("rendering::geometry_types" * doctest::description("Geometry types, bitflags, and slices"))
{
    SUBCASE("GeometryPresentations bitflags")
    {
        GeometryPresentations flags = GeometryPresentation::TRADITIONAL;
        CHECK(flags.contains(GeometryPresentation::TRADITIONAL));
        CHECK(!flags.contains(GeometryPresentation::MESHLET));
        CHECK(!flags.contains(GeometryPresentation::RAY_TRACING));

        flags.set(GeometryPresentation::MESHLET);
        CHECK(flags.contains(GeometryPresentation::TRADITIONAL));
        CHECK(flags.contains(GeometryPresentation::MESHLET));

        auto combined = GeometryPresentation::TRADITIONAL | GeometryPresentation::RAY_TRACING;
        CHECK(combined.contains(GeometryPresentation::TRADITIONAL));
        CHECK(combined.contains(GeometryPresentation::RAY_TRACING));
        CHECK(!combined.contains(GeometryPresentation::MESHLET));
    }

    SUBCASE("BufferSlice validity")
    {
        BufferSlice empty_slice;
        CHECK(!empty_slice.valid());

        BufferSlice slice;
        slice.offset = 64;
        slice.size   = 128;
        // buffer handle is invalid initially, so slice.valid() should be false
        CHECK(!slice.valid());
    }
}

TEST_CASE("rendering::geometry_buffer_pool" * doctest::description("Megabuffer slab sub-allocation and free list"))
{
    ResourceRetire retire;
    retire.init(3);

    const GPUSize64 slab_size = 1024; // 1 KB test slab
    GeometryBufferPool pool(retire, slab_size);

    SUBCASE("sub-allocation alignment and sizes")
    {
        auto slice1 = pool.allocate(128, 16);
        CHECK(slice1.size == 128);
        CHECK(slice1.offset % 16 == 0);
        CHECK(pool.get_used_bytes() == 128);

        auto slice2 = pool.allocate(64, 64);
        CHECK(slice2.size == 64);
        CHECK(slice2.offset % 64 == 0);
        CHECK(slice2.offset >= slice1.offset + slice1.size);
        CHECK(pool.get_used_bytes() == 128 + 64);

        // free and re-allocate
        pool.free(slice1);
        CHECK(pool.get_used_bytes() == 64);

        auto slice3 = pool.allocate(128, 16);
        CHECK(slice3.size == 128);
        CHECK(pool.get_used_bytes() == 128 + 64);

        pool.free(slice2);
        pool.free(slice3);
        CHECK(pool.get_used_bytes() == 0);
    }

    SUBCASE("dedicated oversized allocation")
    {
        // larger than slab_size
        auto large_slice = pool.allocate(2048, 16);
        CHECK(large_slice.size == 2048);
        CHECK(pool.get_used_bytes() == 2048);
        CHECK(pool.get_total_allocated() >= 2048);

        pool.free(large_slice);
        CHECK(pool.get_used_bytes() == 0);
    }

    SUBCASE("clear resets everything")
    {
        pool.allocate(256, 16);
        pool.allocate(512, 16);
        CHECK(pool.get_used_bytes() == 768);

        pool.clear();
        CHECK(pool.get_used_bytes() == 0);
        CHECK(pool.get_total_allocated() == 0);
        CHECK(pool.get_slab_count() == 0);
    }
}

TEST_CASE("rendering::geometry_cache" * doctest::description("GeometryCache residency, two-tier LRU, and budgeting"))
{
    ResourceRetire retire;
    retire.init(3);

    GeometryCache cache(retire, 4096);

    // create mock mesh asset
    MeshAsset mock_asset;
    mock_asset.min_bounds = Vector3(-1.0f);
    mock_asset.max_bounds = Vector3(1.0f);

    MeshLOD lod0;
    lod0.index_format = GPUIndexFormat::UINT32;
    lod0.index_data.resize(36 * sizeof(uint32_t)); // 36 indices

    MeshAttribute pos_attr;
    pos_attr.semantics     = MeshSemantics::POSITION;
    pos_attr.format        = GPUVertexFormat::FLOAT32x3;
    pos_attr.element_count = 24;
    pos_attr.data.resize(24 * 3 * sizeof(float));
    lod0.attributes.push_back(std::move(pos_attr));

    MeshSurface surface;
    surface.slice.index_count  = 36;
    surface.slice.vertex_count = 24;
    surface.meshlet_count      = 4; // 4 meshlets
    lod0.surfaces.push_back(surface);

    mock_asset.lods.push_back(std::move(lod0));

    MeshAssetHandle handle1{random_guid()};
    MeshAssetHandle handle2{random_guid()};

    SUBCASE("acquire and reference counting")
    {
        auto entry1 = cache.acquire(handle1, mock_asset, GeometryPresentation::TRADITIONAL);
        REQUIRE(entry1 != nullptr);
        CHECK(entry1->ref_count == 1);
        CHECK(cache.active_count() == 1);
        CHECK(cache.standby_count() == 0);
        CHECK(cache.total_references() == 1);
        CHECK(cache.contains(handle1));

        // acquire again -> increments ref count
        auto entry2 = cache.acquire(handle1, mock_asset, GeometryPresentation::TRADITIONAL);
        CHECK(entry2 == entry1);
        CHECK(entry1->ref_count == 2);
        CHECK(cache.total_references() == 2);

        // release once
        cache.release(handle1);
        CHECK(entry1->ref_count == 1);
        CHECK(cache.active_count() == 1);
        CHECK(cache.standby_count() == 0);

        // release again -> drops to 0, moves to standby cache
        cache.release(handle1);
        CHECK(cache.active_count() == 0);
        CHECK(cache.standby_count() == 1);
        CHECK(cache.contains(handle1)); // still resident in standby!

        // re-acquire -> hits standby cache, restored without re-allocation!
        auto entry_restored = cache.acquire(handle1, mock_asset, GeometryPresentation::TRADITIONAL);
        REQUIRE(entry_restored != nullptr);
        CHECK(entry_restored->ref_count == 1);
        CHECK(cache.active_count() == 1);
        CHECK(cache.standby_count() == 0);

        cache.release(handle1);
    }

    SUBCASE("on-demand presentation acquisition")
    {
        auto entry = cache.acquire(handle1, mock_asset, GeometryPresentation::TRADITIONAL);
        REQUIRE(entry != nullptr);
        CHECK(entry->lods[0].traditional.has_value());
        CHECK(!entry->lods[0].meshlet.has_value());

        // acquire meshlet presentation on demand
        bool meshlet_ok = cache.acquire_meshlet(handle1, mock_asset);
        CHECK(meshlet_ok);
        CHECK(entry->lods[0].meshlet.has_value());
        CHECK(entry->lods[0].meshlet->meshlet_count == 4);

        cache.release(handle1);
    }

    SUBCASE("standby TTL frame eviction")
    {
        cache.set_standby_ttl_frames(10);

        cache.acquire(handle1, mock_asset);
        cache.release(handle1);
        CHECK(cache.standby_count() == 1);

        // tick at frame 5 (within 10 frame TTL)
        cache.tick(5);
        CHECK(cache.standby_count() == 1);

        // tick at frame 15 (exceeds 10 frame TTL)
        cache.tick(15);
        CHECK(cache.standby_count() == 0);
    }

    SUBCASE("VRAM budget eviction")
    {
        // set tiny VRAM budget
        cache.set_vram_budget(1);
        cache.set_standby_ttl_frames(1000); // disable TTL eviction for this test

        cache.acquire(handle1, mock_asset);
        cache.release(handle1);
        CHECK(cache.standby_count() == 1);

        // tick under budget pressure should evict standby mesh
        cache.tick(1);
        CHECK(cache.standby_count() == 0);
    }
}
