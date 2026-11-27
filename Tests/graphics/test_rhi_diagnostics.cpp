#include <doctest/doctest.h>
#include <fstream>
#include <Lyra/Graphics/RHIDiagnostics.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

TEST_CASE("graphics::rhi_diagnostics" * doctest::description("RHI transparent diagnostic overlay and crash root-cause analyzer"))
{
    // initialize RHI with CRASH_DIAGNOSTICS flag
    RHIDescriptor diag_desc{};
    diag_desc.flags   = RHIFlag::CRASH_DIAGNOSTICS;
    diag_desc.backend = RHIBackend::SOFTGPU;

    auto rhi = RHI::init(diag_desc);
    REQUIRE(rhi.get() != nullptr);
    CHECK(RHIDiagnostics::is_enabled());

    auto adapter = rhi->request_adapter({});
    auto device  = adapter.request_device({});
    REQUIRE(device.valid());

    SUBCASE("Transparent command interception via RenderAPI overlay")
    {
        uint initial_count = RHIDiagnostics::get_total_recorded();

        auto cmd = device.create_command_buffer({});
        REQUIRE(cmd.valid());

        // execute commands through standard RHI interface
        cmd.push_debug_group("GBufferPass");
        cmd.insert_debug_marker("PreDrawMarker");
        cmd.draw(120, 1, 0, 0);
        cmd.draw_indexed(600, 2, 0, 0, 0);
        cmd.pop_debug_group();

        cmd.push_debug_group("ComputeCullPass");
        cmd.dispatch_workgroups(32, 16, 1);
        cmd.pop_debug_group();

        auto entries = RHIDiagnostics::get_entries();
        CHECK(RHIDiagnostics::get_total_recorded() == initial_count + 8);

        // verify pass push intercepted as PASS_BEGIN
        CHECK(entries[initial_count + 0].op == static_cast<uint>(RHIBreadcrumbOp::PASS_BEGIN));
        CHECK(String(entries[initial_count + 0].pass_name) == "GBufferPass");

        // verify marker intercepted as DEBUG_MARKER
        CHECK(entries[initial_count + 1].op == static_cast<uint>(RHIBreadcrumbOp::DEBUG_MARKER));
        CHECK(String(entries[initial_count + 1].marker_name) == "PreDrawMarker");

        // verify draw intercepted as DRAW
        CHECK(entries[initial_count + 2].op == static_cast<uint>(RHIBreadcrumbOp::DRAW));
        CHECK(entries[initial_count + 2].arg0 == 120);
        CHECK(entries[initial_count + 2].arg1 == 1);
        CHECK(String(entries[initial_count + 2].pass_name) == "GBufferPass");

        // verify draw indexed intercepted as DRAW_INDEXED
        CHECK(entries[initial_count + 3].op == static_cast<uint>(RHIBreadcrumbOp::DRAW_INDEXED));
        CHECK(entries[initial_count + 3].arg0 == 600);
        CHECK(entries[initial_count + 3].arg1 == 2);

        // verify pass pop intercepted as PASS_END
        CHECK(entries[initial_count + 4].op == static_cast<uint>(RHIBreadcrumbOp::PASS_END));

        // verify compute pass
        CHECK(entries[initial_count + 5].op == static_cast<uint>(RHIBreadcrumbOp::PASS_BEGIN));
        CHECK(String(entries[initial_count + 5].pass_name) == "ComputeCullPass");

        CHECK(entries[initial_count + 6].op == static_cast<uint>(RHIBreadcrumbOp::DISPATCH));
        CHECK(entries[initial_count + 6].arg0 == 32);
        CHECK(entries[initial_count + 6].arg1 == 16);
        CHECK(entries[initial_count + 6].arg2 == 1);

        CHECK(entries[initial_count + 7].op == static_cast<uint>(RHIBreadcrumbOp::PASS_END));
    }

    SUBCASE("Crash analysis and root-cause dump formatting")
    {
        auto cmd = device.create_command_buffer({});
        REQUIRE(cmd.valid());

        cmd.push_debug_group("VolumetricFogPass");
        cmd.dispatch_workgroups(160, 90, 64);
        // simulated hang right here

        auto report = RHIDiagnostics::analyze("VK_ERROR_DEVICE_LOST: GPU hung during compute shader execution", "Vulkan");

        CHECK(report.hung_entry.op == static_cast<uint>(RHIBreadcrumbOp::DISPATCH));
        CHECK(report.hung_entry.arg0 == 160);
        CHECK(report.hung_entry.arg1 == 90);
        CHECK(report.hung_entry.arg2 == 64);
        CHECK(String(report.hung_entry.pass_name) == "VolumetricFogPass");
        CHECK(report.failure_reason.find("VK_ERROR_DEVICE_LOST") != String::npos);

        // verify formatted dump output
        String formatted = RHIDiagnostics::format_report(report);
        CHECK(formatted.find("LYRA GPU CRASH & BREADCRUMB DUMP") != String::npos);
        CHECK(formatted.find("VolumetricFogPass") != String::npos);
        CHECK(formatted.find("DISPATCH") != String::npos);
        CHECK(formatted.find("dispatch(160,90,64)") != String::npos);
        CHECK(formatted.find("<-- [HUNG / DEVICE LOSS]") != String::npos);

        // verify file persistence
        Path temp_dir  = fs::temp_directory_path();
        Path dump_file = RHIDiagnostics::write_crash_dump(report, temp_dir);

        CHECK(fs::exists(dump_file));
        std::ifstream file(dump_file);
        CHECK(file.is_open());
        String content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        CHECK(content.find("VolumetricFogPass") != String::npos);
        file.close();

        fs::remove(dump_file);
    }
}
