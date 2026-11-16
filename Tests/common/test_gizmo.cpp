#include <doctest/doctest.h>
#include <Lyra/UISystem/Widgets/UIGizmo.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace lyra;

TEST_CASE("ui::gizmo" * doctest::description("UI Gizmo Wrapper Decomposition & Operation Tests"))
{
    SUBCASE("Operation Bitflags")
    {
        auto op = ui::gizmo::Operation::Translate;
        CHECK((static_cast<uint32_t>(op) & static_cast<uint32_t>(ui::gizmo::Operation::TranslateX)) != 0);
        CHECK((static_cast<uint32_t>(op) & static_cast<uint32_t>(ui::gizmo::Operation::TranslateY)) != 0);
        CHECK((static_cast<uint32_t>(op) & static_cast<uint32_t>(ui::gizmo::Operation::TranslateZ)) != 0);
        CHECK((static_cast<uint32_t>(op) & static_cast<uint32_t>(ui::gizmo::Operation::RotateX)) == 0);

        auto universal = ui::gizmo::Operation::Universal;
        CHECK((static_cast<uint32_t>(universal) & static_cast<uint32_t>(ui::gizmo::Operation::Translate)) != 0);
        CHECK((static_cast<uint32_t>(universal) & static_cast<uint32_t>(ui::gizmo::Operation::Rotate)) != 0);
        CHECK((static_cast<uint32_t>(universal) & static_cast<uint32_t>(ui::gizmo::Operation::Scale)) != 0);
    }

    SUBCASE("Decompose and Recompose")
    {
        Vector3 orig_trans(5.0f, -3.0f, 12.0f);
        Vector3 orig_rot(0.0f, 45.0f, 0.0f); // degrees
        Vector3 orig_scale(2.0f, 2.0f, 2.0f);

        Matrix4x4 mat(1.0f);
        ui::gizmo::recompose(orig_trans, orig_rot, orig_scale, mat);

        Vector3 out_trans, out_rot, out_scale;
        ui::gizmo::decompose(mat, out_trans, out_rot, out_scale);

        CHECK_EQ(out_trans.x, doctest::Approx(orig_trans.x).epsilon(0.01f));
        CHECK_EQ(out_trans.y, doctest::Approx(orig_trans.y).epsilon(0.01f));
        CHECK_EQ(out_trans.z, doctest::Approx(orig_trans.z).epsilon(0.01f));

        CHECK_EQ(out_rot.y, doctest::Approx(orig_rot.y).epsilon(0.01f));

        CHECK_EQ(out_scale.x, doctest::Approx(orig_scale.x).epsilon(0.01f));
        CHECK_EQ(out_scale.y, doctest::Approx(orig_scale.y).epsilon(0.01f));
        CHECK_EQ(out_scale.z, doctest::Approx(orig_scale.z).epsilon(0.01f));
    }
}
