/// @file FrameGraphExecutor.cpp
/// @brief Walks the compiled pass list and invokes each execute callback.

#include "SagaEngine/Render/FrameGraphExecutor.h"
#include "SagaEngine/RHI/IRHI.h"

namespace SagaEngine::Render
{

void FrameGraphExecutor::Execute(RHI::IRHI& rhi)
{
    for (auto& pass : m_passes)
    {
        if (pass.execute)
            pass.execute(rhi);
    }
}

} // namespace SagaEngine::Render
