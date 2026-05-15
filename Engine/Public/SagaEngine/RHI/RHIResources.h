/// @file RHIResources.h
/// @brief Owning wrappers for RHI handles with RAII release.

#pragma once

#include "SagaEngine/RHI/Types/BufferTypes.h"
#include "SagaEngine/RHI/Types/TextureTypes.h"
#include "SagaEngine/RHI/Types/ShaderTypes.h"
#include "SagaEngine/RHI/Types/PipelineTypes.h"

namespace SagaEngine::RHI
{

// Forward — the concrete IRHI backend is needed for release calls.
class IRHI;

/// Lightweight scoped wrapper around a BufferHandle.
/// Moveable, non-copyable.  Release happens via the IRHI pointer
/// stored at creation time (set by the factory function in IRHI).
struct OwnedBuffer
{
    BufferHandle handle = BufferHandle::kInvalid;
    IRHI*        rhi    = nullptr;

    OwnedBuffer() = default;
    OwnedBuffer(BufferHandle h, IRHI* r) : handle(h), rhi(r) {}
    ~OwnedBuffer();

    OwnedBuffer(OwnedBuffer&& o) noexcept : handle(o.handle), rhi(o.rhi)
    { o.handle = BufferHandle::kInvalid; o.rhi = nullptr; }

    OwnedBuffer& operator=(OwnedBuffer&& o) noexcept;

    OwnedBuffer(const OwnedBuffer&) = delete;
    OwnedBuffer& operator=(const OwnedBuffer&) = delete;

    [[nodiscard]] bool Valid() const noexcept { return handle != BufferHandle::kInvalid; }
};

/// Lightweight scoped wrapper around a PipelineHandle.
struct OwnedPipeline
{
    PipelineHandle handle = PipelineHandle::kInvalid;
    IRHI*          rhi    = nullptr;

    OwnedPipeline() = default;
    OwnedPipeline(PipelineHandle h, IRHI* r) : handle(h), rhi(r) {}
    ~OwnedPipeline();

    OwnedPipeline(OwnedPipeline&& o) noexcept : handle(o.handle), rhi(o.rhi)
    { o.handle = PipelineHandle::kInvalid; o.rhi = nullptr; }

    OwnedPipeline& operator=(OwnedPipeline&& o) noexcept;

    OwnedPipeline(const OwnedPipeline&) = delete;
    OwnedPipeline& operator=(const OwnedPipeline&) = delete;

    [[nodiscard]] bool Valid() const noexcept { return handle != PipelineHandle::kInvalid; }
};

} // namespace SagaEngine::RHI
