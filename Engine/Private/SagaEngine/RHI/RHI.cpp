/// @file RHI.cpp
/// @brief RHI resource RAII wrappers — destructor and move-assignment bodies.

#include "SagaEngine/RHI/RHIResources.h"
#include "SagaEngine/RHI/IRHI.h"

namespace SagaEngine::RHI
{

// ── OwnedBuffer ──────────────────────────────────────────────────────

OwnedBuffer::~OwnedBuffer()
{
    if (handle != BufferHandle::kInvalid && rhi)
        rhi->DestroyBuffer(handle);
}

OwnedBuffer& OwnedBuffer::operator=(OwnedBuffer&& o) noexcept
{
    if (this != &o)
    {
        if (handle != BufferHandle::kInvalid && rhi)
            rhi->DestroyBuffer(handle);

        handle   = o.handle;
        rhi      = o.rhi;
        o.handle = BufferHandle::kInvalid;
        o.rhi    = nullptr;
    }
    return *this;
}

// ── OwnedPipeline ────────────────────────────────────────────────────

OwnedPipeline::~OwnedPipeline()
{
    if (handle != PipelineHandle::kInvalid && rhi)
        rhi->DestroyPipeline(handle);
}

OwnedPipeline& OwnedPipeline::operator=(OwnedPipeline&& o) noexcept
{
    if (this != &o)
    {
        if (handle != PipelineHandle::kInvalid && rhi)
            rhi->DestroyPipeline(handle);

        handle   = o.handle;
        rhi      = o.rhi;
        o.handle = PipelineHandle::kInvalid;
        o.rhi    = nullptr;
    }
    return *this;
}

} // namespace SagaEngine::RHI
