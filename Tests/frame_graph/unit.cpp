#include <doctest/doctest.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/FrameGraphMemory.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Rendering/ViewUniforms.h>

using namespace lyra;

TEST_CASE("fg::execution_order" * doctest::description("FrameGraph Execution Order & Topological Sort"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;
    Vector<int>         execution_trace;

    FrameGraphTexture imported_tex;
    imported_tex.texture = GPUTextureHandle{1};
    auto base_tex        = builder.import(imported_tex);

    struct PassDataA
    {
        FrameGraph::TextureHandle tex;
    };

    struct PassDataB
    {
        FrameGraph::TextureHandle tex;
    };

    auto data_a = builder.add_pass<PassDataA>(
        "PassA",
        [&](PassDataA& data, FrameGraphPassBuilder& pb) {
            data.tex = pb.render(base_tex);
        },
        [&](const PassDataA&, FrameGraphResources&, FrameGraphContext*) {
            execution_trace.push_back(1);
        });

    builder.add_pass<PassDataB>(
        "PassB",
        [&](PassDataB& data, FrameGraphPassBuilder& pb) {
            data.tex = pb.sample(data_a.tex);
            pb.preserve(); // preserve final output
        },
        [&](const PassDataB&, FrameGraphResources&, FrameGraphContext*) {
            execution_trace.push_back(2);
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);

    graph->execute(nullptr, &allocator);

    REQUIRE_EQ(execution_trace.size(), 2);
    CHECK_EQ(execution_trace[0], 1);
    CHECK_EQ(execution_trace[1], 2);
}

TEST_CASE("fg::culling" * doctest::description("FrameGraph Dead Pass Culling"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;
    Vector<String>      executed_passes;

    FrameGraphTexture imported_tex;
    imported_tex.texture = GPUTextureHandle{1};
    auto tex             = builder.import(imported_tex);

    // Pass 1: Writes to a texture that is never consumed or preserved -> should be culled!
    builder.add_pass(
        "CulledPass",
        [&](FrameGraphPassBuilder& pb) {
            tex = pb.render(tex);
        },
        [&](FrameGraphResources&, FrameGraphContext*) {
            executed_passes.push_back("CulledPass");
        });

    // Pass 2: Preserved pass -> must execute
    builder.add_pass(
        "ActivePass",
        [&](FrameGraphPassBuilder& pb) {
            pb.preserve();
        },
        [&](FrameGraphResources&, FrameGraphContext*) {
            executed_passes.push_back("ActivePass");
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);

    graph->execute(nullptr, &allocator);

    REQUIRE_EQ(executed_passes.size(), 1);
    CHECK_EQ(executed_passes[0], "ActivePass");
}

TEST_CASE("fg::ssa_write_after_read" * doctest::description("FrameGraph SSA Anti-Dependency & No False Cycles"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;
    Vector<int>         execution_trace;

    FrameGraphBuffer imported_buf;
    imported_buf.buffer = GPUBufferHandle{1};
    imported_buf.size   = 1024;
    auto buf            = builder.import(imported_buf);

    // Pass 1: writes buffer v0 -> produces v1
    builder.add_pass(
        "PassWrite1",
        [&](FrameGraphPassBuilder& pb) {
            buf = pb.write(buf);
        },
        [&](FrameGraphResources&, FrameGraphContext*) {
            execution_trace.push_back(1);
        });

    // Pass 2: reads buffer v1
    builder.add_pass(
        "PassRead1",
        [&](FrameGraphPassBuilder& pb) {
            buf = pb.read(buf);
            pb.preserve();
        },
        [&](FrameGraphResources&, FrameGraphContext*) {
            execution_trace.push_back(2);
        });

    // Pass 3: writes buffer v1 -> produces v2 (Write-after-read hazard against Pass 2)
    builder.add_pass(
        "PassWrite2",
        [&](FrameGraphPassBuilder& pb) {
            buf = pb.write(buf);
            pb.preserve();
        },
        [&](FrameGraphResources&, FrameGraphContext*) {
            execution_trace.push_back(3);
        });

    // Pass 4: reads buffer v2
    builder.add_pass(
        "PassRead2",
        [&](FrameGraphPassBuilder& pb) {
            buf = pb.read(buf);
            pb.preserve();
        },
        [&](FrameGraphResources&, FrameGraphContext*) {
            execution_trace.push_back(4);
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);

    graph->execute(nullptr, &allocator);

    REQUIRE_EQ(execution_trace.size(), 4);
    CHECK_EQ(execution_trace[0], 1);
    CHECK_EQ(execution_trace[1], 2);
    CHECK_EQ(execution_trace[2], 3);
    CHECK_EQ(execution_trace[3], 4);
}

TEST_CASE("fg::buffer_barriers" * doctest::description("FrameGraph Buffer State Transitions & Barriers"))
{
    FrameGraphBuffer buffer;
    buffer.size  = 2048;
    buffer.state = undefined_buffer_state();

    FrameGraphBarrierBatch barriers;

    // Transition from undefined to shader resource read
    buffer.pre_read(nullptr, nullptr, FrameGraphReadOp::SAMPLE, &barriers);

    CHECK_EQ(barriers.buffer_barriers.size(), 1);
    CHECK_EQ(barriers.buffer_barriers[0].size, 2048);
    CHECK_EQ(buffer.state.access, shader_resource_buffer_state(GPUBarrierSync::ALL).access);
    CHECK_EQ(buffer.state.sync, shader_resource_buffer_state(GPUBarrierSync::ALL).sync);

    // Transition from shader resource read to unordered access write
    buffer.pre_write(nullptr, nullptr, FrameGraphWriteOp::WRITE, &barriers);

    CHECK_EQ(barriers.buffer_barriers.size(), 2);
    CHECK_EQ(buffer.state.access, unordered_access_buffer_state(GPUBarrierSync::ALL).access);
    CHECK_EQ(buffer.state.sync, unordered_access_buffer_state(GPUBarrierSync::ALL).sync);
}

TEST_CASE("rhi::new_transition_states" * doctest::description("New RHI Transition States"))
{
    auto ds_read = depth_stencil_read_texture_state();
    CHECK_EQ(ds_read.layout, GPUBarrierLayout::DEPTH_STENCIL_READ);
    CHECK_EQ(ds_read.access, GPUBarrierAccess::DEPTH_STENCIL_READ);
    CHECK_EQ(ds_read.sync, GPUBarrierSync::DEPTH_STENCIL);

    auto vb_state = vertex_buffer_state();
    CHECK_EQ(vb_state.access, GPUBarrierAccess::VERTEX_BUFFER);
    CHECK_EQ(vb_state.sync, GPUBarrierSync::VERTEX_SHADING);

    auto ib_state = index_buffer_state();
    CHECK_EQ(ib_state.access, GPUBarrierAccess::INDEX_BUFFER);
    CHECK_EQ(ib_state.sync, GPUBarrierSync::INDEX_INPUT);

    auto indirect_state = indirect_argument_state();
    CHECK_EQ(indirect_state.access, GPUBarrierAccess::INDIRECT_ARGUMENT);
    CHECK_EQ(indirect_state.sync, GPUBarrierSync::EXECUTE_INDIRECT);

    auto ub_state = uniform_buffer_state();
    CHECK_EQ(ub_state.access, GPUBarrierAccess::UNIFORM_BUFFER);

    auto as_build = acceleration_structure_build_state();
    CHECK_EQ(as_build.access, GPUBarrierAccess::ACCELERATION_STRUCTURE_WRITE);
    CHECK_EQ(as_build.sync, GPUBarrierSync::ACCELERATION_STRUCTURE_BUILD);

    auto as_read = acceleration_structure_read_state();
    CHECK_EQ(as_read.access, GPUBarrierAccess::ACCELERATION_STRUCTURE_READ);
    CHECK_EQ(as_read.sync, GPUBarrierSync::RAYTRACING);

    auto resolve_src = resolve_src_texture_state();
    CHECK_EQ(resolve_src.layout, GPUBarrierLayout::RESOLVE_SOURCE);
    CHECK_EQ(resolve_src.access, GPUBarrierAccess::RESOLVE_SOURCE);
    CHECK_EQ(resolve_src.sync, GPUBarrierSync::RESOLVE);

    auto resolve_dst = resolve_dst_texture_state();
    CHECK_EQ(resolve_dst.layout, GPUBarrierLayout::RESOLVE_DEST);
    CHECK_EQ(resolve_dst.access, GPUBarrierAccess::RESOLVE_DEST);
    CHECK_EQ(resolve_dst.sync, GPUBarrierSync::RESOLVE);

    CHECK_EQ(get_index_format_size(GPUIndexFormat::UINT16), 2);
    CHECK_EQ(get_index_format_size(GPUIndexFormat::UINT32), 4);
}

TEST_CASE("fg::direct_import" * doctest::description("FrameGraph Direct Builder Import & TLAS Resource"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;
    bool                pass_executed = false;

    // Direct import before any passes exist
    FrameGraphTexture imported_tex;
    imported_tex.texture = GPUTextureHandle{123};
    auto tex_handle      = builder.import(imported_tex);

    FrameGraphTlas imported_tlas;
    imported_tlas.tlas = GPUTlasHandle{456};
    auto tlas_handle   = builder.import(imported_tlas);

    builder.add_pass(
        "DirectImportConsumerPass",
        [&](FrameGraphPassBuilder& pb) {
            tex_handle  = pb.read(tex_handle);
            tlas_handle = pb.read(tlas_handle);
            pb.preserve();
        },
        [&](FrameGraphResources& res, FrameGraphContext*) {
            auto* t = res.get<FrameGraphTexture>(tex_handle);
            REQUIRE(t != nullptr);
            CHECK_EQ(t->texture.value, 123);
            auto* a = res.get<FrameGraphTlas>(tlas_handle);
            REQUIRE(a != nullptr);
            CHECK_EQ(a->tlas.value, 456);
            pass_executed = true;
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);

    graph->execute(nullptr, &allocator);
    CHECK(pass_executed);
}

TEST_CASE("fg::redundant_barriers_and_depth" * doctest::description("Skip Redundant Barriers and Depth Read State"))
{
    // 1. Redundant texture barrier skip on Read-after-Read
    FrameGraphTexture tex;
    tex.format = GPUTextureFormat::RGBA8UNORM;
    tex.state  = undefined_texture_state();

    FrameGraphBarrierBatch barriers;
    tex.pre_read(nullptr, nullptr, FrameGraphReadOp::SAMPLE, &barriers);
    CHECK_EQ(barriers.texture_barriers.size(), 1);
    CHECK_EQ(tex.state.layout, GPUBarrierLayout::SHADER_RESOURCE);

    // Second sample to the same texture should be skipped (no redundant barrier)
    tex.pre_read(nullptr, nullptr, FrameGraphReadOp::SAMPLE, &barriers);
    CHECK_EQ(barriers.texture_barriers.size(), 1); // still 1!

    // 2. Depth texture read should transition to depth_stencil_read_texture_state, not UAV!
    FrameGraphTexture depth_tex;
    depth_tex.format = GPUTextureFormat::DEPTH24PLUS_STENCIL8;
    depth_tex.state  = undefined_texture_state();

    depth_tex.pre_read(nullptr, nullptr, FrameGraphReadOp::READ, &barriers);
    CHECK_EQ(barriers.texture_barriers.size(), 2);
    CHECK_EQ(depth_tex.state.layout, GPUBarrierLayout::DEPTH_STENCIL_READ);
    CHECK_EQ(depth_tex.state.access, GPUBarrierAccess::DEPTH_STENCIL_READ);

    // 3. FrameGraphAllocator reclaim, tick, clear, and destruction safety
    {
        FrameGraphAllocator alloc;
        CHECK_EQ(alloc.allocated_texture_count(), 0);
        CHECK_EQ(alloc.allocated_buffer_count(), 0);
        alloc.reclaim_unused();
        alloc.reclaim_unused_if([](const GPUTextureDescriptor&) {
            return true;
        });
        alloc.reclaim_unused_if([](const GPUBufferDescriptor&) {
            return true;
        });
        alloc.reclaim_unused_if([](const GPUTextureDescriptor&) {
            return true;
        },
            [](const GPUBufferDescriptor&) {
                return true;
            });
        alloc.tick(60);
        alloc.clear();
        CHECK_EQ(alloc.allocated_texture_count(), 0);
        CHECK_EQ(alloc.allocated_buffer_count(), 0);
    }
}

TEST_CASE("fg::allocator_real_resources" * doctest::description("FrameGraphAllocator real allocation, pooling, and recycling"))
{
    FrameGraphAllocator allocator;

    SUBCASE("Buffer allocation and pooling reuse")
    {
        GPUBufferDescriptor desc{};
        desc.label = "fg_test_buffer";
        desc.size  = 256;
        desc.usage = GPUBufferUsage::STORAGE;

        auto buf1 = allocator.allocate(desc);
        CHECK(buf1.value != 0);
        CHECK_EQ(allocator.allocated_buffer_count(), 1);

        // recycle buf1
        allocator.recycle(desc, buf1);

        // re-allocating identical descriptor must reuse buf1
        auto buf2 = allocator.allocate(desc);
        CHECK_EQ(buf1, buf2);
        CHECK_EQ(allocator.allocated_buffer_count(), 1);

        // allocating different descriptor creates a second buffer
        GPUBufferDescriptor desc2 = desc;
        desc2.size                = 512;
        auto buf3                 = allocator.allocate(desc2);
        CHECK_NE(buf1, buf3);
        CHECK_EQ(allocator.allocated_buffer_count(), 2);
    }

    SUBCASE("Texture allocation, recycling, and aging reclamation")
    {
        GPUTextureDescriptor desc{};
        desc.label        = "fg_test_tex";
        desc.size         = GPUExtent3D{64, 64, 1};
        desc.format       = GPUTextureFormat::RGBA8UNORM;
        desc.usage        = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::RENDER_ATTACHMENT;
        desc.sample_count = 1;

        auto tex1 = allocator.allocate(desc);
        CHECK(tex1.texture.value != 0);
        CHECK(tex1.view.value != 0);
        CHECK_EQ(allocator.allocated_texture_count(), 1);

        allocator.recycle(desc, tex1);

        // re-allocate reuses existing texture and view
        auto tex2 = allocator.allocate(desc);
        CHECK_EQ(tex1.texture, tex2.texture);
        CHECK_EQ(tex1.view, tex2.view);
        CHECK_EQ(allocator.allocated_texture_count(), 1);

        // recycle and age
        allocator.recycle(desc, tex2);
        allocator.tick(2); // age unused frames
        allocator.tick(2);
        allocator.reclaim_unused();
        CHECK_EQ(allocator.allocated_texture_count(), 0);
    }
}

TEST_CASE("fg::frame_arena" * doctest::description("FrameArena transient bind group allocation, ring buffering, and reset"))
{
    FrameArena arena(3, 1024, 65536);
    CHECK_EQ(arena.current_slot(), 0);
    CHECK_EQ(arena.heap_count(), 0);
    CHECK_EQ(arena.uniform_page_count(), 0);

    GPUBindGroupDescriptor desc{};
    auto                   bg = arena.allocate(desc);
    CHECK(bg.handle.valid());
    CHECK_EQ(arena.heap_count(), 3);
    CHECK_EQ(arena.current_slot(), 0);

    // 1. test uniform allocation and alignment
    struct DummyCam
    {
        float view[16];
        float proj[16];
    };
    DummyCam cam_data{};
    cam_data.view[0] = 42.0f;
    cam_data.proj[0] = 84.0f;

    auto u1 = arena.create_uniform(cam_data);
    CHECK(u1.valid());
    CHECK_EQ(u1.offset, 0);
    CHECK_EQ(u1.size, sizeof(DummyCam));
    CHECK_EQ(u1.as<DummyCam>()->view[0], 42.0f);
    CHECK_EQ(u1.as<DummyCam>()->proj[0], 84.0f);

    auto u2 = arena.allocate_uniform(128, 256);
    CHECK(u2.valid());
    CHECK_EQ(u2.offset, 256); // aligned to 256 bytes after sizeof(DummyCam) = 128
    CHECK_EQ(u2.size, 128);

    // 2. advance frames and check ring buffering
    arena.tick();
    CHECK_EQ(arena.current_slot(), 1);

    auto u_slot1 = arena.allocate_uniform(64, 256);
    CHECK(u_slot1.valid());
    CHECK_EQ(u_slot1.offset, 0);

    arena.tick();
    CHECK_EQ(arena.current_slot(), 2);

    auto u_slot2 = arena.allocate_uniform(64, 256);
    CHECK(u_slot2.valid());
    CHECK_EQ(u_slot2.offset, 0);

    // wrap around back to slot 0
    arena.tick();
    CHECK_EQ(arena.current_slot(), 0);

    // slot 0 was reset, next allocation starts back at offset 0
    auto u_slot0_reset = arena.allocate_uniform(64, 256);
    CHECK(u_slot0_reset.valid());
    CHECK_EQ(u_slot0_reset.offset, 0);

    // 3. test CameraUniforms from view
    RenderView view{};
    view.camera_position = Vector3(1.0f, 2.0f, 3.0f);
    view.near_plane      = 0.5f;
    view.far_plane       = 500.0f;
    auto cu              = CameraUniforms::from_view(view);
    CHECK_EQ(cu.camera_pos.x, 1.0f);
    CHECK_EQ(cu.camera_pos.y, 2.0f);
    CHECK_EQ(cu.camera_pos.z, 3.0f);
    CHECK_EQ(cu.near_plane, 0.5f);
    CHECK_EQ(cu.far_plane, 500.0f);

    auto cu_slice = arena.create_uniform(cu);
    CHECK(cu_slice.valid());
    CHECK_EQ(cu_slice.size, sizeof(CameraUniforms));
    CHECK_EQ(cu_slice.as<CameraUniforms>()->camera_pos.x, 1.0f);

    // 4. test ViewUniforms caching and re-use
    ViewUniforms vu{};
    vu.init(view, arena);
    CHECK(vu.slice.valid());
    CHECK_EQ(vu.slice.size, sizeof(CameraUniforms));
    CHECK_EQ(vu.slice.as<CameraUniforms>()->camera_pos.x, 1.0f);

    GPUBindGroupLayout layout1{};
    layout1.handle.value = 1001;

    GPUBindGroupLayout layout2{};
    layout2.handle.value = 1002;

    auto bg1_a = vu.bind_group(layout1, arena);
    CHECK(bg1_a.handle.valid());

    auto bg1_b = vu.bind_group(layout1, arena);
    CHECK_EQ(bg1_a.handle.value, bg1_b.handle.value); // Re-uses exact same bind group!

    auto bg2 = vu.bind_group(layout2, arena);
    CHECK(bg2.handle.valid());
    CHECK_NE(bg1_a.handle.value, bg2.handle.value);

    // FrameGraphContext integration
    FrameGraphContext fg_ctx{};
    fg_ctx.arena         = &arena;
    fg_ctx.view_uniforms = &vu;
    auto bg_ctx          = fg_ctx.view_uniforms->bind_group(layout1, *fg_ctx.arena);
    CHECK_EQ(bg_ctx.handle.value, bg1_a.handle.value);
}

TEST_CASE("fg::transient_resource_lifecycle" * doctest::description("FrameGraph Transient Resource Allocation & Recycling"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;
    bool                producer_executed = false;
    bool                consumer_executed = false;

    GPUTextureDescriptor tex_desc{};
    tex_desc.label        = "transient_color";
    tex_desc.size         = GPUExtent3D{128, 128, 1};
    tex_desc.format       = GPUTextureFormat::RGBA8UNORM;
    tex_desc.usage        = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::RENDER_ATTACHMENT;
    tex_desc.sample_count = 1;

    struct PassDataA
    {
        FrameGraph::TextureHandle tex;
    };

    auto data_a = builder.add_pass<PassDataA>(
        "TransientProducer",
        [&](PassDataA& data, FrameGraphPassBuilder& pb) {
            data.tex = pb.create<FrameGraphTexture>(tex_desc);
            data.tex = pb.render(data.tex);
        },
        [&](const PassDataA&, FrameGraphResources&, FrameGraphContext*) {
            producer_executed = true;
        });

    struct PassDataB
    {
        FrameGraph::TextureHandle tex;
    };

    builder.add_pass<PassDataB>(
        "TransientConsumer",
        [&](PassDataB& data, FrameGraphPassBuilder& pb) {
            data.tex = pb.sample(data_a.tex);
            pb.preserve(); // ensure pass and resource are preserved
        },
        [&](const PassDataB&, FrameGraphResources&, FrameGraphContext*) {
            consumer_executed = true;
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);

    graph->execute(nullptr, &allocator);

    CHECK(producer_executed);
    CHECK(consumer_executed);
    // Transient texture should have been allocated and then recycled into the allocator
    CHECK_EQ(allocator.allocated_texture_count(), 1);
}

TEST_CASE("fg::memory_hint_semantics" * doctest::description("GPUMemoryHint Size and Semantics"))
{
    CHECK_EQ(sizeof(GPUMemoryHint), sizeof(uint));

    auto dedicated = GPUMemoryHint::dedicated();
    CHECK(dedicated.is_dedicated());
    CHECK_FALSE(dedicated.is_aliased());
    CHECK_EQ(dedicated.slot, 0);

    auto aliased = GPUMemoryHint::aliased(3);
    CHECK(aliased.is_aliased());
    CHECK_FALSE(aliased.is_dedicated());
    CHECK_EQ(aliased.slot, 3);

    CHECK_EQ(dedicated, GPUMemoryHint{0});
    CHECK_EQ(aliased, GPUMemoryHint{3});
    CHECK_NE(dedicated, aliased);
}

TEST_CASE("fg::memory_planner_disjoint_and_overlapping" * doctest::description("FrameGraphMemoryPlanner Interval Graph Coloring"))
{
    // Test 1: Two disjoint intervals should share the same alias slot
    {
        Vector<ResourceLifetimeInterval> intervals(2);
        intervals[0] = {0, 0, 1, true}; // [0, 1]
        intervals[1] = {1, 2, 3, true}; // [2, 3]

        auto plan = FrameGraphMemoryPlanner::compute_plan(intervals);
        REQUIRE_EQ(plan.resource_hints.size(), 2);
        CHECK(plan.resource_hints[0].is_aliased());
        CHECK(plan.resource_hints[1].is_aliased());
        // Disjoint -> same slot
        CHECK_EQ(plan.resource_hints[0].slot, plan.resource_hints[1].slot);
        CHECK_EQ(plan.max_color_slots, 1);
    }

    // Test 2: Two overlapping intervals must receive distinct alias slots
    {
        Vector<ResourceLifetimeInterval> intervals(2);
        intervals[0] = {0, 0, 2, true}; // [0, 2]
        intervals[1] = {1, 1, 3, true}; // [1, 3]

        auto plan = FrameGraphMemoryPlanner::compute_plan(intervals);
        REQUIRE_EQ(plan.resource_hints.size(), 2);
        CHECK(plan.resource_hints[0].is_aliased());
        CHECK(plan.resource_hints[1].is_aliased());
        // Overlapping -> different slots
        CHECK_NE(plan.resource_hints[0].slot, plan.resource_hints[1].slot);
        CHECK_EQ(plan.max_color_slots, 2);
    }

    // Test 3: Complex chain: A:[0, 1], B:[1, 2], C:[2, 3]
    // A and B overlap at pass 1 -> distinct slots
    // B and C overlap at pass 2 -> distinct slots
    // A:[0, 1] and C:[2, 3] are disjoint -> can share slot!
    {
        Vector<ResourceLifetimeInterval> intervals(3);
        intervals[0] = {0, 0, 1, true};
        intervals[1] = {1, 1, 2, true};
        intervals[2] = {2, 2, 3, true};

        auto plan = FrameGraphMemoryPlanner::compute_plan(intervals);
        REQUIRE_EQ(plan.resource_hints.size(), 3);
        CHECK_NE(plan.resource_hints[0].slot, plan.resource_hints[1].slot);
        CHECK_NE(plan.resource_hints[1].slot, plan.resource_hints[2].slot);
        // A and C can share slot
        CHECK_EQ(plan.resource_hints[0].slot, plan.resource_hints[2].slot);
        CHECK_EQ(plan.max_color_slots, 2);
    }
}

TEST_CASE("fg::transient_memory_aliasing_integration" * doctest::description("FrameGraph Transient Memory Aliasing Integration"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;

    GPUTextureDescriptor tex_desc{};
    tex_desc.label        = "chain_tex";
    tex_desc.size         = GPUExtent3D{64, 64, 1};
    tex_desc.format       = GPUTextureFormat::RGBA8UNORM;
    tex_desc.usage        = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::RENDER_ATTACHMENT;
    tex_desc.sample_count = 1;

    struct PassDataA
    {
        FrameGraph::TextureHandle tex;
    };
    struct PassDataB
    {
        FrameGraph::TextureHandle in_tex;
        FrameGraph::TextureHandle out_tex;
    };
    struct PassDataC
    {
        FrameGraph::TextureHandle in_tex;
        FrameGraph::TextureHandle out_tex;
    };
    struct PassDataD
    {
        FrameGraph::TextureHandle in_tex;
    };

    // Pass A: creates and writes TexA (lifetime: Pass A .. Pass B)
    auto pass_a = builder.add_pass<PassDataA>(
        "PassA",
        [&](PassDataA& data, FrameGraphPassBuilder& pb) {
            data.tex = pb.create<FrameGraphTexture>(tex_desc);
            data.tex = pb.render(data.tex);
        },
        [&](const PassDataA&, FrameGraphResources&, FrameGraphContext*) {
        });

    // Pass B: reads TexA, creates and writes TexB (TexA ends here; TexB lifetime: Pass B .. Pass C)
    auto pass_b = builder.add_pass<PassDataB>(
        "PassB",
        [&](PassDataB& data, FrameGraphPassBuilder& pb) {
            data.in_tex  = pb.sample(pass_a.tex);
            data.out_tex = pb.create<FrameGraphTexture>(tex_desc);
            data.out_tex = pb.render(data.out_tex);
        },
        [&](const PassDataB&, FrameGraphResources&, FrameGraphContext*) {
        });

    // Pass C: reads TexB, creates and writes TexC (TexB ends here; TexC lifetime: Pass C .. Pass D)
    auto pass_c = builder.add_pass<PassDataC>(
        "PassC",
        [&](PassDataC& data, FrameGraphPassBuilder& pb) {
            data.in_tex  = pb.sample(pass_b.out_tex);
            data.out_tex = pb.create<FrameGraphTexture>(tex_desc);
            data.out_tex = pb.render(data.out_tex);
        },
        [&](const PassDataC&, FrameGraphResources&, FrameGraphContext*) {
        });

    // Pass D: reads TexC and preserves it
    builder.add_pass<PassDataD>(
        "PassD",
        [&](PassDataD& data, FrameGraphPassBuilder& pb) {
            data.in_tex = pb.sample(pass_c.out_tex);
            pb.preserve();
        },
        [&](const PassDataD&, FrameGraphResources&, FrameGraphContext*) {
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);

    // Verify physical resources got assigned aliased memory hints
    REQUIRE_GE(graph->physical_resource_count(), 3);

    auto res_a = graph->get_physical_resource(0);
    auto res_b = graph->get_physical_resource(1);
    auto res_c = graph->get_physical_resource(2);

    REQUIRE(res_a != nullptr);
    REQUIRE(res_b != nullptr);
    REQUIRE(res_c != nullptr);

    auto hint_a = res_a->get_memory_hint();
    auto hint_b = res_b->get_memory_hint();
    auto hint_c = res_c->get_memory_hint();

    CHECK(hint_a.is_aliased());
    CHECK(hint_b.is_aliased());
    CHECK(hint_c.is_aliased());

    // TexA (Pass A..B) and TexB (Pass B..C) overlap at Pass B -> distinct slots!
    CHECK_NE(hint_a.slot, hint_b.slot);
    // TexB (Pass B..C) and TexC (Pass C..D) overlap at Pass C -> distinct slots!
    CHECK_NE(hint_b.slot, hint_c.slot);
    // TexA (Pass A..B) and TexC (Pass C..D) are disjoint -> must share the same slot!
    CHECK_EQ(hint_a.slot, hint_c.slot);

    // Execute the graph with allocator
    graph->execute(nullptr, &allocator);
}

TEST_CASE("fg::aliased_barrier_transitions" * doctest::description("Aliasing Barrier Transitions for Transient Resources"))
{
    // Verify aliased undefined transition states
    auto tex_aliased_state = aliased_undefined_texture_state();
    CHECK_EQ(tex_aliased_state.layout, GPUBarrierLayout::UNDEFINED);
    CHECK_EQ(tex_aliased_state.sync, GPUBarrierSync::ALL);
    CHECK_EQ(tex_aliased_state.access, GPUBarrierAccess::NO_ACCESS);

    auto buf_aliased_state = aliased_undefined_buffer_state();
    CHECK_EQ(buf_aliased_state.sync, GPUBarrierSync::ALL);
    CHECK_EQ(buf_aliased_state.access, GPUBarrierAccess::NO_ACCESS);

    // Verify dedicated undefined transition states
    auto tex_dedicated_state = undefined_texture_state();
    CHECK_EQ(tex_dedicated_state.layout, GPUBarrierLayout::UNDEFINED);
    CHECK_EQ(tex_dedicated_state.sync, GPUBarrierSync::NONE);
    CHECK_EQ(tex_dedicated_state.access, GPUBarrierAccess::NO_ACCESS);

    FrameGraphAllocator allocator;

    // Test 1: Dedicated texture initial transition
    {
        GPUTextureDescriptor desc{};
        desc.label       = "dedicated_tex";
        desc.size        = GPUExtent3D{32, 32, 1};
        desc.format      = GPUTextureFormat::RGBA8UNORM;
        desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT;
        desc.memory_hint = GPUMemoryHint::dedicated();

        FrameGraphTexture tex;
        tex.create(&allocator, desc);
        CHECK_EQ(tex.state.sync, GPUBarrierSync::NONE);

        FrameGraphBarrierBatch barriers;
        tex.pre_write(nullptr, nullptr, FrameGraphWriteOp::RENDER, &barriers);

        REQUIRE_EQ(barriers.texture_barriers.size(), 1);
        const auto& b = barriers.texture_barriers[0];
        CHECK_EQ(b.src_layout, GPUBarrierLayout::UNDEFINED);
        CHECK_EQ(b.src_sync, GPUBarrierSync::NONE);
        CHECK_EQ(b.dst_layout, GPUBarrierLayout::RENDER_TARGET);
        CHECK_EQ(b.dst_sync, GPUBarrierSync::RENDER_TARGET);

        tex.destroy(&allocator, desc);
    }

    // Test 2: Aliased texture initial transition must emit an execution barrier (ALL -> target)
    {
        GPUTextureDescriptor desc{};
        desc.label       = "aliased_tex";
        desc.size        = GPUExtent3D{32, 32, 1};
        desc.format      = GPUTextureFormat::RGBA8UNORM;
        desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT;
        desc.memory_hint = GPUMemoryHint::aliased(1);

        FrameGraphTexture tex;
        tex.create(&allocator, desc);
        // Aliased texture starts with ALL sync to synchronize against previous memory owner
        CHECK_EQ(tex.state.sync, GPUBarrierSync::ALL);

        FrameGraphBarrierBatch barriers;
        tex.pre_write(nullptr, nullptr, FrameGraphWriteOp::RENDER, &barriers);

        REQUIRE_EQ(barriers.texture_barriers.size(), 1);
        const auto& b = barriers.texture_barriers[0];
        CHECK_EQ(b.src_layout, GPUBarrierLayout::UNDEFINED);
        CHECK_EQ(b.src_sync, GPUBarrierSync::ALL);
        CHECK_EQ(b.dst_layout, GPUBarrierLayout::RENDER_TARGET);
        CHECK_EQ(b.dst_sync, GPUBarrierSync::RENDER_TARGET);

        tex.destroy(&allocator, desc);
    }

    // Test 3: Aliased buffer initial transition
    {
        GPUBufferDescriptor desc{};
        desc.label       = "aliased_buf";
        desc.size        = 1024;
        desc.usage       = GPUBufferUsage::STORAGE;
        desc.memory_hint = GPUMemoryHint::aliased(2);

        FrameGraphBuffer buf;
        buf.create(&allocator, desc);
        CHECK_EQ(buf.state.sync, GPUBarrierSync::ALL);

        FrameGraphBarrierBatch barriers;
        buf.pre_write(nullptr, nullptr, FrameGraphWriteOp::WRITE, &barriers);

        REQUIRE_EQ(barriers.buffer_barriers.size(), 1);
        const auto& b = barriers.buffer_barriers[0];
        CHECK_EQ(b.src_sync, GPUBarrierSync::ALL);
        CHECK_EQ(b.dst_sync, GPUBarrierSync::ALL);

        buf.destroy(&allocator, desc);
    }
}

TEST_CASE("fg::aliased_hazard_tracking" * doctest::description("Aliasing Hazard Tracking & Predecessor Identification"))
{
    // A:[0, 1], B:[1, 2], C:[2, 3]
    Vector<ResourceLifetimeInterval> intervals(3);
    intervals[0] = {10, 0, 1, true}; // pid 10
    intervals[1] = {20, 1, 2, true}; // pid 20
    intervals[2] = {30, 2, 3, true}; // pid 30

    auto plan = FrameGraphMemoryPlanner::compute_plan(intervals);
    REQUIRE_EQ(plan.resource_hints.size(), 3);
    REQUIRE_EQ(plan.has_aliased_predecessor.size(), 3);
    REQUIRE_EQ(plan.predecessor_physical_id.size(), 3);

    // Resource 0 (A) is the first occupant of slot 1 -> no predecessor
    CHECK_FALSE(plan.has_aliased_predecessor[0]);
    CHECK_EQ(plan.predecessor_physical_id[0], ~0u);

    // Resource 1 (B) is the first occupant of slot 2 -> no predecessor
    CHECK_FALSE(plan.has_aliased_predecessor[1]);
    CHECK_EQ(plan.predecessor_physical_id[1], ~0u);

    // Resource 2 (C) reuses slot 1 from Resource 0 (A) -> aliased predecessor is pid 10!
    CHECK(plan.has_aliased_predecessor[2]);
    CHECK_EQ(plan.predecessor_physical_id[2], 10);
    CHECK_EQ(plan.resource_hints[2].slot, plan.resource_hints[0].slot);
}

TEST_CASE("fg::async_compute_pass_declaration" * doctest::description("Async Compute Pass Queue Declaration & Builders"))
{
    FrameGraphBuilder builder;

    struct PassData
    {
    };

    auto p1 = builder.add_pass<PassData>(
        "GraphicsPass",
        [](PassData&, FrameGraphPassBuilder& pb) {
            CHECK_EQ(pb.get_queue(), GPUQueueType::DEFAULT);
        },
        [](const PassData&, FrameGraphResources&, FrameGraphContext*) {
        });

    auto p2 = builder.add_pass<PassData>(
        "ExplicitComputePass",
        [](PassData&, FrameGraphPassBuilder& pb) {
            pb.set_queue(GPUQueueType::COMPUTE);
            CHECK_EQ(pb.get_queue(), GPUQueueType::COMPUTE);
        },
        [](const PassData&, FrameGraphResources&, FrameGraphContext*) {
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);
    CHECK_FALSE(graph->has_async_compute()); // culled because unreferenced
}

TEST_CASE("fg::async_compute_single_queue_batch" * doctest::description("Single-Queue Fast Path Batch Generation"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;

    GPUTextureDescriptor desc{};
    desc.label  = "tex";
    desc.size   = GPUExtent3D{16, 16, 1};
    desc.format = GPUTextureFormat::RGBA8UNORM;
    desc.usage  = GPUTextureUsage::RENDER_ATTACHMENT;

    struct Data
    {
        FrameGraph::TextureHandle tex;
    };

    builder.add_pass<Data>(
        "GPass1",
        [&](Data& d, FrameGraphPassBuilder& pb) {
            d.tex = pb.create<FrameGraphTexture>(desc);
            d.tex = pb.render(d.tex);
            pb.preserve();
        },
        [](const Data&, FrameGraphResources&, FrameGraphContext*) {
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);
    CHECK_FALSE(graph->has_async_compute());
    CHECK_EQ(graph->get_sync_points().size(), 0);
    REQUIRE_EQ(graph->get_queue_batches().size(), 1);
    CHECK_EQ(graph->get_queue_batches()[0].queue_type, GPUQueueType::DEFAULT);

    graph->execute(nullptr, &allocator);
}

TEST_CASE("fg::async_compute_cross_queue_sync" * doctest::description("Async Compute Cross-Queue Synchronization & Batching"))
{
    FrameGraphBuilder   builder;
    FrameGraphAllocator allocator;

    GPUTextureDescriptor tex_desc{};
    tex_desc.label  = "depth_tex";
    tex_desc.size   = GPUExtent3D{32, 32, 1};
    tex_desc.format = GPUTextureFormat::DEPTH32FLOAT;
    tex_desc.usage  = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;

    GPUBufferDescriptor buf_desc{};
    buf_desc.label = "light_grid";
    buf_desc.size  = 256;
    buf_desc.usage = GPUBufferUsage::STORAGE;

    struct PassAData
    {
        FrameGraph::TextureHandle depth;
    };

    struct PassBData
    {
        FrameGraph::TextureHandle depth;
        FrameGraph::BufferHandle  grid;
    };

    struct PassCData
    {
        FrameGraph::TextureHandle depth;
        FrameGraph::BufferHandle  grid;
    };

    // Pass A: Graphics queue produces depth
    auto pass_a = builder.add_pass<PassAData>(
        "DepthPrepass",
        [&](PassAData& d, FrameGraphPassBuilder& pb) {
            d.depth = pb.create<FrameGraphTexture>(tex_desc);
            d.depth = pb.render(d.depth);
        },
        [](const PassAData&, FrameGraphResources&, FrameGraphContext*) {
        });

    // Pass B: Async Compute queue reads depth, produces light grid buffer
    auto pass_b = builder.add_pass<PassBData>(
        "ClusteredLightCulling",
        [&](PassBData& d, FrameGraphPassBuilder& pb) {
            pb.set_queue(GPUQueueType::COMPUTE);
            d.depth = pb.read(pass_a.depth);
            d.grid  = pb.create<FrameGraphBuffer>(buf_desc);
            d.grid  = pb.write(d.grid);
        },
        [](const PassBData&, FrameGraphResources&, FrameGraphContext*) {
        });

    // Pass C: Graphics queue reads light grid buffer and preserves
    builder.add_pass<PassCData>(
        "DeferredLighting",
        [&](PassCData& d, FrameGraphPassBuilder& pb) {
            d.grid = pb.read(pass_b.grid);
            pb.preserve();
        },
        [](const PassCData&, FrameGraphResources&, FrameGraphContext*) {
        });

    auto graph = builder.build();
    REQUIRE(graph != nullptr);

    // Multi-queue async compute detected
    CHECK(graph->has_async_compute());

    // Should create 2 cross-queue sync points:
    // S0: DEFAULT (Pass A) -> COMPUTE (Pass B)
    // S1: COMPUTE (Pass B) -> DEFAULT (Pass C)
    auto sync_points = graph->get_sync_points();
    REQUIRE_EQ(sync_points.size(), 2);

    CHECK_EQ(sync_points[0].from_queue, GPUQueueType::DEFAULT);
    CHECK_EQ(sync_points[0].to_queue, GPUQueueType::COMPUTE);

    CHECK_EQ(sync_points[1].from_queue, GPUQueueType::COMPUTE);
    CHECK_EQ(sync_points[1].to_queue, GPUQueueType::DEFAULT);

    // Should partition into 3 discrete queue batches:
    // Batch 0: DEFAULT (DepthPrepass), signals S0
    // Batch 1: COMPUTE (ClusteredLightCulling), waits S0, signals S1
    // Batch 2: DEFAULT (DeferredLighting), waits S1
    auto batches = graph->get_queue_batches();
    REQUIRE_EQ(batches.size(), 3);

    CHECK_EQ(batches[0].queue_type, GPUQueueType::DEFAULT);
    CHECK_EQ(batches[0].wait_syncs.size(), 0);
    REQUIRE_EQ(batches[0].signal_syncs.size(), 1);
    CHECK_EQ(batches[0].signal_syncs[0], 0);

    CHECK_EQ(batches[1].queue_type, GPUQueueType::COMPUTE);
    REQUIRE_EQ(batches[1].wait_syncs.size(), 1);
    CHECK_EQ(batches[1].wait_syncs[0], 0);
    REQUIRE_EQ(batches[1].signal_syncs.size(), 1);
    CHECK_EQ(batches[1].signal_syncs[0], 1);

    CHECK_EQ(batches[2].queue_type, GPUQueueType::DEFAULT);
    REQUIRE_EQ(batches[2].wait_syncs.size(), 1);
    CHECK_EQ(batches[2].wait_syncs[0], 1);
    CHECK_EQ(batches[2].signal_syncs.size(), 0);

    // Execute multi-queue graph with allocator
    graph->execute(nullptr, &allocator);
}
