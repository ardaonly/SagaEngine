/// @file DiligentFrameCapture.h
/// @brief Private current-back-buffer capture helper for the Diligent backend.

#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentGraphicsRuntime.h"
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
        Graphics::Backends::Diligent::Runtime::DiligentGraphicsRuntime& runtime,
        RenderFrameCapture& outCapture);

    void Reset();

private:
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_stagingTexture;
};

} // namespace SagaEngine::Render::Backend
