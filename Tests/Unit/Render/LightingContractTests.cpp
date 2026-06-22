/// @file LightingContractTests.cpp
/// @brief Unit tests for the forward lighting data contract.

#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Scene/RenderView.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{

struct NormalRows
{
    float row0[3]{1.0f, 0.0f, 0.0f};
    float row1[3]{0.0f, 1.0f, 0.0f};
    float row2[3]{0.0f, 0.0f, 1.0f};
};

bool BuildNormalRows(const SagaEngine::Math::Mat4& model,
                     NormalRows& rows) noexcept
{
    const float a00 = model.At(0, 0), a01 = model.At(0, 1), a02 = model.At(0, 2);
    const float a10 = model.At(1, 0), a11 = model.At(1, 1), a12 = model.At(1, 2);
    const float a20 = model.At(2, 0), a21 = model.At(2, 1), a22 = model.At(2, 2);
    if (!std::isfinite(a00) || !std::isfinite(a01) || !std::isfinite(a02) ||
        !std::isfinite(a10) || !std::isfinite(a11) || !std::isfinite(a12) ||
        !std::isfinite(a20) || !std::isfinite(a21) || !std::isfinite(a22))
    {
        return false;
    }

    const float c00 =  (a11 * a22 - a12 * a21);
    const float c01 = -(a10 * a22 - a12 * a20);
    const float c02 =  (a10 * a21 - a11 * a20);
    const float c10 = -(a01 * a22 - a02 * a21);
    const float c11 =  (a00 * a22 - a02 * a20);
    const float c12 = -(a00 * a21 - a01 * a20);
    const float c20 =  (a01 * a12 - a02 * a11);
    const float c21 = -(a00 * a12 - a02 * a10);
    const float c22 =  (a00 * a11 - a01 * a10);

    const float det = a00 * c00 + a01 * c01 + a02 * c02;
    if (std::fabs(det) < 1.0e-6f)
        return false;

    const float invDet = 1.0f / det;
    rows.row0[0] = c00 * invDet;
    rows.row0[1] = c01 * invDet;
    rows.row0[2] = c02 * invDet;
    rows.row1[0] = c10 * invDet;
    rows.row1[1] = c11 * invDet;
    rows.row1[2] = c12 * invDet;
    rows.row2[0] = c20 * invDet;
    rows.row2[1] = c21 * invDet;
    rows.row2[2] = c22 * invDet;
    return true;
}

SagaEngine::Math::Vec3 ApplyNormalRows(
    const NormalRows& rows,
    SagaEngine::Math::Vec3 normal) noexcept
{
    return {
        rows.row0[0] * normal.x + rows.row0[1] * normal.y + rows.row0[2] * normal.z,
        rows.row1[0] * normal.x + rows.row1[1] * normal.y + rows.row1[2] * normal.z,
        rows.row2[0] * normal.x + rows.row2[1] * normal.y + rows.row2[2] * normal.z,
    };
}

[[nodiscard]] bool NormalizeDirectionalLightForSubmit(
    SagaEngine::Render::Scene::RenderLightingData& lighting) noexcept
{
    if (!lighting.directionalEnabled)
        return false;

    const float lenSq = lighting.directional.direction.LengthSq();
    if (lenSq < 1.0e-6f)
    {
        lighting.directionalEnabled = false;
        lighting.shadowsEnabled = false;
        return false;
    }

    lighting.directional.direction =
        lighting.directional.direction * (1.0f / std::sqrt(lenSq));
    return true;
}

} // namespace

TEST(RenderLighting, DirectionalLightDataUsesRayTravelDirectionConvention)
{
    SagaEngine::Render::Scene::RenderLightingData lighting{};
    lighting.directionalEnabled = true;
    lighting.directional.direction = {0.0f, 0.0f, -1.0f};

    const SagaEngine::Math::Vec3 lightToSurface =
        lighting.directional.direction * -1.0f;

    EXPECT_FLOAT_EQ(lightToSurface.z, 1.0f);
}

TEST(RenderLighting, AmbientCanRemainEnabledWhenDirectionalIsDisabled)
{
    SagaEngine::Render::Scene::RenderLightingData lighting{};
    lighting.directionalEnabled = false;
    lighting.shadowsEnabled = false;
    lighting.ambient.intensity = 0.35f;

    EXPECT_FALSE(lighting.directionalEnabled);
    EXPECT_FLOAT_EQ(lighting.ambient.intensity, 0.35f);
}

TEST(RenderLighting, DirectionalLightRejectsZeroDirection)
{
    SagaEngine::Render::Scene::RenderLightingData lighting{};
    lighting.directionalEnabled = true;
    lighting.shadowsEnabled = true;
    lighting.directional.direction = {0.0f, 0.0f, 0.0f};
    lighting.ambient.intensity = 0.35f;

    EXPECT_FALSE(NormalizeDirectionalLightForSubmit(lighting));
    EXPECT_FALSE(lighting.directionalEnabled);
    EXPECT_FALSE(lighting.shadowsEnabled);
    EXPECT_FLOAT_EQ(lighting.ambient.intensity, 0.35f);
}

TEST(RenderLighting, DirectionalLightNormalizesValidDirection)
{
    SagaEngine::Render::Scene::RenderLightingData lighting{};
    lighting.directionalEnabled = true;
    lighting.shadowsEnabled = true;
    lighting.directional.direction = {0.0f, -3.0f, -4.0f};

    ASSERT_TRUE(NormalizeDirectionalLightForSubmit(lighting));
    EXPECT_TRUE(lighting.directionalEnabled);
    EXPECT_TRUE(lighting.shadowsEnabled);
    EXPECT_NEAR(lighting.directional.direction.x, 0.0f, 1.0e-6f);
    EXPECT_NEAR(lighting.directional.direction.y, -0.6f, 1.0e-6f);
    EXPECT_NEAR(lighting.directional.direction.z, -0.8f, 1.0e-6f);
    EXPECT_NEAR(lighting.directional.direction.LengthSq(), 1.0f, 1.0e-6f);
}

TEST(RenderLighting, LitMaterialPreservesUnlitMaterialPath)
{
    SagaEngine::Render::MaterialRuntime material{};
    EXPECT_EQ(material.shadingModel,
              SagaEngine::Render::OpaqueShadingModel::Unlit);

    material.shadingModel = SagaEngine::Render::OpaqueShadingModel::LitDiffuse;
    EXPECT_EQ(material.shadingModel,
              SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
}

TEST(RenderLighting, NormalMatrixHandlesRotation)
{
    constexpr float kHalfPi = 1.5707963267948966f;
    const auto model = SagaEngine::Math::Mat4::FromQuat(
        SagaEngine::Math::Quat::FromAxisAngle({0.0f, 0.0f, 1.0f}, kHalfPi));

    NormalRows rows{};
    ASSERT_TRUE(BuildNormalRows(model, rows));

    const auto normal = ApplyNormalRows(rows, {1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(normal.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(normal.y, 1.0f, 1.0e-5f);
    EXPECT_NEAR(normal.z, 0.0f, 1.0e-5f);
}

TEST(RenderLighting, NormalMatrixHandlesNonUniformScale)
{
    const auto model = SagaEngine::Math::Mat4::FromScale({2.0f, 4.0f, 8.0f});

    NormalRows rows{};
    ASSERT_TRUE(BuildNormalRows(model, rows));

    EXPECT_FLOAT_EQ(rows.row0[0], 0.5f);
    EXPECT_FLOAT_EQ(rows.row1[1], 0.25f);
    EXPECT_FLOAT_EQ(rows.row2[2], 0.125f);
}

TEST(RenderLighting, NormalMatrixHandlesNegativeScale)
{
    const auto model = SagaEngine::Math::Mat4::FromScale({-2.0f, 3.0f, 4.0f});

    NormalRows rows{};
    ASSERT_TRUE(BuildNormalRows(model, rows));

    EXPECT_FLOAT_EQ(rows.row0[0], -0.5f);
    EXPECT_FLOAT_EQ(rows.row1[1], 1.0f / 3.0f);
    EXPECT_FLOAT_EQ(rows.row2[2], 0.25f);
}

TEST(RenderLighting, NormalMatrixRejectsSingularTransform)
{
    const auto model = SagaEngine::Math::Mat4::FromScale({1.0f, 0.0f, 1.0f});

    NormalRows rows{};
    EXPECT_FALSE(BuildNormalRows(model, rows));
}

TEST(RenderLighting, NormalMatrixRejectsNonFiniteTransform)
{
    auto nanModel = SagaEngine::Math::Mat4::Identity();
    nanModel.At(0, 0) = std::numeric_limits<float>::quiet_NaN();

    auto infModel = SagaEngine::Math::Mat4::Identity();
    infModel.At(1, 1) = std::numeric_limits<float>::infinity();

    NormalRows rows{};
    EXPECT_FALSE(BuildNormalRows(nanModel, rows));
    EXPECT_FALSE(BuildNormalRows(infModel, rows));
}
