/// @file BufferDiligent.cpp
/// @brief Diligent-specific buffer creation helpers (future home of typed
///        buffer factories for vertex, index, constant, structured buffers).

#include "SagaEngine/RHI/Types/BufferTypes.h"

namespace SagaEngine::RHI::Diligent
{

// Placeholder — buffer creation currently lives inside
// DiligentRenderBackend::Impl.  As we refactor toward IRHI-based
// resource management, typed factory functions will move here:
//
//   BufferHandle CreateVertexBuffer(IRHI&, const void* data, uint64_t size);
//   BufferHandle CreateIndexBuffer(IRHI&, const void* data, uint64_t size);
//   BufferHandle CreateConstantBuffer(IRHI&, uint64_t size);

} // namespace SagaEngine::RHI::Diligent
