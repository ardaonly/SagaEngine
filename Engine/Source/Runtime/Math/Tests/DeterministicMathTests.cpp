// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/Math/DeterministicMath.h"
#include "SagaEngine/Math/MathConstants.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/Vec3.h"

#include <gtest/gtest.h>

namespace SagaEngine::Math {
namespace {

TEST(DeterministicMathTests, FixedPointArithmeticHasExactExpectedBits)
{
    constexpr auto oneAndHalf = Deterministic::ToFixed16(1.5f);
    constexpr auto minusTwo = Deterministic::ToFixed16(-2.0f);

    static_assert(oneAndHalf == 98304);
    static_assert(minusTwo == -131072);
    static_assert(Deterministic::Multiply(oneAndHalf, minusTwo) == -196608);

    EXPECT_FLOAT_EQ(Deterministic::FromFixed16(
        Deterministic::Add(oneAndHalf, minusTwo)), -0.5f);
}

TEST(DeterministicMathTests, ScalarPrimitivesRespectKnownBoundaryValues)
{
    ASSERT_TRUE(Deterministic::LockRoundingModeToNearest());
    EXPECT_TRUE(Deterministic::IsRoundingModeDeterministic());
    EXPECT_FLOAT_EQ(Deterministic::Sqrt(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Deterministic::Sqrt(4.0f), 2.0f);
    EXPECT_NEAR(Deterministic::Sin(0.0f), 0.0f, 1.0e-6f);
    EXPECT_NEAR(Deterministic::Cos(0.0f), 1.0f, 1.0e-5f);
    EXPECT_NEAR(Deterministic::Atan2(0.0f, -1.0f), kPi, 1.0e-4f);
}

TEST(DeterministicMathTests, QuaternionRotationPreservesVectorLength)
{
    const auto rotation = Quat::FromAxisAngle(Vec3::Up(), kHalfPi);
    const Vec3 input{1.0f, 0.0f, 0.0f};
    const auto output = rotation.Rotate(input);

    EXPECT_NEAR(output.Length(), input.Length(), 1.0e-5f);
    EXPECT_NEAR(output.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(output.z, -1.0f, 1.0e-5f);
}

} // namespace
} // namespace SagaEngine::Math
