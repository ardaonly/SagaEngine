/// @file Skeleton.cpp
/// @brief Skeleton rest-pose computation.

#include "SagaEngine/Animation/Skeleton.h"

namespace SagaEngine::Animation
{

void Skeleton::ComputeRestPose(Math::Mat4* outWorldMatrices) const noexcept
{
    const auto count = JointCount();

    for (std::uint8_t i = 0; i < count; ++i)
    {
        const auto& j = joints[i];

        // Local transform: T * R * S
        Math::Mat4 local = Math::Mat4::FromTranslation(j.restPosition)
                         * Math::Mat4::FromQuat(j.restRotation)
                         * Math::Mat4::FromScale(j.restScale);

        if (j.parent != kNoParent && j.parent < i)
            outWorldMatrices[i] = outWorldMatrices[j.parent] * local;
        else
            outWorldMatrices[i] = local;
    }
}

} // namespace SagaEngine::Animation
