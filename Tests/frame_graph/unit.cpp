#include <doctest/doctest.h>
#include <Lyra/Rendering/FrameGraph.h>

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
        alloc.reclaim_unused_if([](const GPUTextureDescriptor&) { return true; });
        alloc.reclaim_unused_if([](const GPUBufferDescriptor&) { return true; });
        alloc.reclaim_unused_if([](const GPUTextureDescriptor&) { return true; },
            [](const GPUBufferDescriptor&) { return true; });
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
