/// @file GraphicsCoordinateConventionTests.cpp
/// @brief CPU tests for SagaEngine's graphics coordinate convention.

#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Render/Scene/Camera.h"

#include <gtest/gtest.h>

#include <cmath>

namespace
{

struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

[[nodiscard]] Vec4 TransformHomogeneous(const SagaEngine::Math::Mat4& m,
                                        Vec4 v) noexcept
{
    return {
        m.At(0, 0) * v.x + m.At(0, 1) * v.y + m.At(0, 2) * v.z + m.At(0, 3) * v.w,
        m.At(1, 0) * v.x + m.At(1, 1) * v.y + m.At(1, 2) * v.z + m.At(1, 3) * v.w,
        m.At(2, 0) * v.x + m.At(2, 1) * v.y + m.At(2, 2) * v.z + m.At(2, 3) * v.w,
        m.At(3, 0) * v.x + m.At(3, 1) * v.y + m.At(3, 2) * v.z + m.At(3, 3) * v.w,
    };
}

[[nodiscard]] float NdcDepth(const SagaEngine::Math::Mat4& projection,
                             float cameraSpaceZ) noexcept
{
    const Vec4 clip = TransformHomogeneous(
        projection, {0.0f, 0.0f, cameraSpaceZ, 1.0f});
    return clip.z / clip.w;
}

[[nodiscard]] float SignedArea2D(float ax, float ay,
                                 float bx, float by,
                                 float cx, float cy) noexcept
{
    return 0.5f * ((bx - ax) * (cy - ay) - (by - ay) * (cx - ax));
}

[[nodiscard]] SagaEngine::Math::Vec3 ShadowNdcToUv(
    SagaEngine::Math::Vec3 ndc) noexcept
{
    return {ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, ndc.z};
}

struct NormalRows
{
    float row0[3]{1.0f, 0.0f, 0.0f};
    float row1[3]{0.0f, 1.0f, 0.0f};
    float row2[3]{0.0f, 0.0f, 1.0f};
};

[[nodiscard]] bool BuildNormalRows(const SagaEngine::Math::Mat4& model,
                                   NormalRows& rows) noexcept
{
    SagaEngine::Math::Mat4 inv{};
    if (!model.InverseChecked(inv))
        return false;

    const auto normalMatrix = inv.Transposed();
    rows.row0[0] = normalMatrix.At(0, 0);
    rows.row0[1] = normalMatrix.At(0, 1);
    rows.row0[2] = normalMatrix.At(0, 2);
    rows.row1[0] = normalMatrix.At(1, 0);
    rows.row1[1] = normalMatrix.At(1, 1);
    rows.row1[2] = normalMatrix.At(1, 2);
    rows.row2[0] = normalMatrix.At(2, 0);
    rows.row2[1] = normalMatrix.At(2, 1);
    rows.row2[2] = normalMatrix.At(2, 2);
    return true;
}

[[nodiscard]] SagaEngine::Math::Vec3 ApplyNormalRows(
    const NormalRows& rows,
    SagaEngine::Math::Vec3 normal) noexcept
{
    return {
        rows.row0[0] * normal.x + rows.row0[1] * normal.y + rows.row0[2] * normal.z,
        rows.row1[0] * normal.x + rows.row1[1] * normal.y + rows.row1[2] * normal.z,
        rows.row2[0] * normal.x + rows.row2[1] * normal.y + rows.row2[2] * normal.z,
    };
}

} // namespace

TEST(GraphicsCoordinateConvention, PerspectiveNearMapsToExpectedDepth)
{
    const auto projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        1.0472f, 16.0f / 9.0f, 0.1f, 100.0f);

    EXPECT_NEAR(NdcDepth(projection, -0.1f), 0.0f, 1.0e-5f);
}

TEST(GraphicsCoordinateConvention, PerspectiveFarMapsToExpectedDepth)
{
    const auto projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        1.0472f, 16.0f / 9.0f, 0.1f, 100.0f);

    EXPECT_NEAR(NdcDepth(projection, -100.0f), 1.0f, 1.0e-5f);
}

TEST(GraphicsCoordinateConvention, OrthographicNearFarMapToZeroOne)
{
    const auto projection = SagaEngine::Math::Mat4::OrthoRH_ZO(
        -1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);

    EXPECT_NEAR(NdcDepth(projection, -0.1f), 0.0f, 1.0e-5f);
    EXPECT_NEAR(NdcDepth(projection, -100.0f), 1.0f, 1.0e-5f);
}

TEST(GraphicsCoordinateConvention, ViewForwardIsNegativeZ)
{
    const auto view = SagaEngine::Math::Mat4::LookAtRH(
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, 1.0f, 0.0f});

    const auto forward = view.TransformDirection({0.0f, 0.0f, -1.0f});
    EXPECT_NEAR(forward.x, 0.0f, 1.0e-6f);
    EXPECT_NEAR(forward.y, 0.0f, 1.0e-6f);
    EXPECT_NEAR(forward.z, -1.0f, 1.0e-6f);
}

TEST(GraphicsCoordinateConvention, ProjectionViewModelOrderIsPVM)
{
    SagaEngine::Render::Scene::Camera camera{};
    camera.position = {0.0f, 0.0f, 3.0f};
    camera.view = SagaEngine::Math::Mat4::LookAtRH(
        camera.position, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    camera.projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        1.0472f, 4.0f / 3.0f, 0.1f, 100.0f);
    camera.RecomputeViewProj();

    const auto model = SagaEngine::Math::Mat4::FromTranslation(
        {0.25f, 0.0f, 0.0f});

    const auto direct = TransformHomogeneous(camera.viewProj * model,
                                             {0.0f, 0.0f, 0.0f, 1.0f});
    const auto staged = TransformHomogeneous(
        camera.projection,
        TransformHomogeneous(
            camera.view,
            TransformHomogeneous(model, {0.0f, 0.0f, 0.0f, 1.0f})));

    EXPECT_NEAR(direct.x, staged.x, 1.0e-6f);
    EXPECT_NEAR(direct.y, staged.y, 1.0e-6f);
    EXPECT_NEAR(direct.z, staged.z, 1.0e-6f);
    EXPECT_NEAR(direct.w, staged.w, 1.0e-6f);
}

TEST(GraphicsCoordinateConvention, CounterClockwiseTriangleIsFrontFacing)
{
    EXPECT_GT(SignedArea2D(-1.0f, -1.0f,
                           1.0f, -1.0f,
                           0.0f, 1.0f),
              0.0f);
}

TEST(GraphicsCoordinateConvention, CanonicalUvCornersMapCorrectly)
{
    const SagaEngine::Math::Vec3 topLeft{0.0f, 0.0f, 0.0f};
    const SagaEngine::Math::Vec3 topRight{1.0f, 0.0f, 0.0f};
    const SagaEngine::Math::Vec3 bottomLeft{0.0f, 1.0f, 0.0f};
    const SagaEngine::Math::Vec3 bottomRight{1.0f, 1.0f, 0.0f};

    EXPECT_FLOAT_EQ(topLeft.x, 0.0f);
    EXPECT_FLOAT_EQ(topLeft.y, 0.0f);
    EXPECT_FLOAT_EQ(topRight.x, 1.0f);
    EXPECT_FLOAT_EQ(bottomLeft.y, 1.0f);
    EXPECT_FLOAT_EQ(bottomRight.x, 1.0f);
    EXPECT_FLOAT_EQ(bottomRight.y, 1.0f);
}

TEST(GraphicsCoordinateConvention, ShadowNdcToUvMapsCenterToHalfHalf)
{
    const auto center = ShadowNdcToUv({0.0f, 0.0f, 0.5f});
    const auto top = ShadowNdcToUv({0.0f, 1.0f, 0.5f});
    const auto bottom = ShadowNdcToUv({0.0f, -1.0f, 0.5f});

    EXPECT_FLOAT_EQ(center.x, 0.5f);
    EXPECT_FLOAT_EQ(center.y, 0.5f);
    EXPECT_FLOAT_EQ(top.y, 0.0f);
    EXPECT_FLOAT_EQ(bottom.y, 1.0f);
}

TEST(GraphicsCoordinateConvention, NormalTransformUsesInverseTranspose)
{
    const auto model =
        SagaEngine::Math::Mat4::FromQuat(
            SagaEngine::Math::Quat::FromAxisAngle(
                {0.0f, 0.0f, 1.0f}, 1.5707963267948966f)) *
        SagaEngine::Math::Mat4::FromScale({2.0f, 4.0f, 1.0f});

    NormalRows rows{};
    ASSERT_TRUE(BuildNormalRows(model, rows));

    const auto transformed = ApplyNormalRows(rows, {1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(transformed.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(transformed.y, 0.5f, 1.0e-5f);
    EXPECT_NEAR(transformed.z, 0.0f, 1.0e-5f);
}

TEST(GraphicsCoordinateConvention, BackendAdjustmentAppliedExactlyOnce)
{
    const auto projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        1.0472f, 1.0f, 0.1f, 100.0f);
    const auto clipTop = TransformHomogeneous(
        projection, {0.0f, 1.0f, -3.0f, 1.0f});
    const float ndcTopY = clipTop.y / clipTop.w;
    const auto shadowTopUv = ShadowNdcToUv({0.0f, ndcTopY, 0.5f});

    EXPECT_GT(ndcTopY, 0.0f);
    EXPECT_LT(shadowTopUv.y, 0.5f)
        << "Shadow NDC-to-UV owns the single canonical Y inversion.";
}
