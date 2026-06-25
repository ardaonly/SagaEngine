/// @file DiligentFrameCapture.h
/// @brief Private current-back-buffer capture helper for the Diligent backend.

#pragma once

#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"

#include "RefCntAutoPtr.hpp"

namespace Diligent
{
struct ITexture;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{

class DiligentFrameCapture final
{
public:
    [[nodiscard]] RenderCaptureResult Capture(
        DiligentDeviceServices services,
        RenderFrameCapture& outCapture);

    void Reset();

private:
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_stagingTexture;
};

} // namespace SagaEngine::Render::Backend
