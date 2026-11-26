#include <doctest/doctest.h>
#include <Lyra/UISystem/Widgets/UINodeEditor.h>
#include <Lyra/UISystem/Widgets/UINodeEnums.h>

using namespace lyra;

TEST_CASE("ui::node" * doctest::description("UI Node Editor API Wrapper Tests"))
{
    SUBCASE("Flag operations")
    {
        auto flags = ui::node::NodeEditorFlag::EnableGridLines | ui::node::NodeEditorFlag::EnableGridSnapping;
        CHECK((flags & ui::node::NodeEditorFlag::EnableGridLines) != ui::node::NodeEditorFlag::None);
        CHECK((flags & ui::node::NodeEditorFlag::EnableGridSnapping) != ui::node::NodeEditorFlag::None);
        CHECK((flags & ui::node::NodeEditorFlag::EnableNodeOutline) == ui::node::NodeEditorFlag::None);
    }

    SUBCASE("NodeEditorContext Lifecycle and Movement")
    {
        ui::node::NodeEditorContext ctx;
        CHECK(ctx.native_handle() != nullptr);

        // move constructor
        ui::node::NodeEditorContext moved_ctx = std::move(ctx);
        CHECK(moved_ctx.native_handle() != nullptr);
        CHECK(ctx.native_handle() == nullptr);

        // move assignment
        ui::node::NodeEditorContext target_ctx;
        target_ctx = std::move(moved_ctx);
        CHECK(target_ctx.native_handle() != nullptr);
        CHECK(moved_ctx.native_handle() == nullptr);
    }

    SUBCASE("Node Editor Positioning & Panning")
    {
        ui::node::reset_panning(Vector2(100.0f, 50.0f));
        auto panning = ui::node::get_panning();
        CHECK_EQ(panning.x, doctest::Approx(100.0f));
        CHECK_EQ(panning.y, doctest::Approx(50.0f));

        // test node position get & set
        ui::node::NodeId test_id = 42;
        ui::node::set_node_position(test_id, Vector2(250.0f, 180.0f), ui::node::CoordinateSpace::Grid);
        auto grid_pos = ui::node::get_node_position(test_id, ui::node::CoordinateSpace::Grid);
        CHECK_EQ(grid_pos.x, doctest::Approx(250.0f));
        CHECK_EQ(grid_pos.y, doctest::Approx(180.0f));
    }

    SUBCASE("Serialization and Deserialization")
    {
        ui::node::NodeEditorContext ctx;
        auto state_str = ui::node::save_editor_state(&ctx);
        // ini state string can be empty or valid ini
        ui::node::load_editor_state(state_str, &ctx);
    }

    SUBCASE("Selection queries and clearing")
    {
        ui::node::clear_selection();
        CHECK_EQ(ui::node::num_selected_nodes(), 0);
        CHECK_EQ(ui::node::num_selected_links(), 0);
        CHECK_FALSE(ui::node::is_node_selected(1));
        CHECK_FALSE(ui::node::is_link_selected(1));
    }
}
