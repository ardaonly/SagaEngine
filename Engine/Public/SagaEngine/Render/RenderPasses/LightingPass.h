/// @file LightingPass.h
/// @brief Deferred lighting pass — reads G-buffer, outputs lit colour.

#pragma once

#include "SagaEngine/Render/RenderGraph/RGTypes.h"
#include "SagaEngine/Render/RenderPasses/GBufferPass.h"
#include "SagaEngine/Math/Vec3.h"

namespace SagaEngine::RHI { class IRHI; }

namespace SagaEngine::Render::Passes
{

/// Simple directional light descriptor.
struct DirectionalLight
{
    Math::Vec3 direction = {0.0f, -1.0f, 0.0f};
    Math::Vec3 colour    = {1.0f, 1.0f, 1.0f};
    float      intensity = 1.0f;
};

/// Reads the G-buffer textures and applies lighting to produce the
/// final lit colour target.  Currently a stub — real implementation
/// will come with the deferred-shading pass migration.
class LightingPass
{
public:
    void SetGBufferInputs(const GBufferOutputs& inputs) { m_gbuffer = inputs; }
    void SetOutputTarget(RG::RGResourceId target)       { m_litTarget = target; }
    void SetSunLight(const DirectionalLight& light)     { m_sun = light; }

    /// Execute the lighting pass.
    void Execute(RHI::IRHI& rhi);

private:
    GBufferOutputs   m_gbuffer{};
    RG::RGResourceId m_litTarget = RG::RGResourceId::kInvalid;
    DirectionalLight m_sun{};
};

} // namespace SagaEngine::Render::Passes
