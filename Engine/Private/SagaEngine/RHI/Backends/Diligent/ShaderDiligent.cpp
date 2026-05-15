/// @file ShaderDiligent.cpp
/// @brief Diligent-specific shader compilation helpers (future home of
///        runtime shader compilation and caching).

#include "SagaEngine/RHI/Types/ShaderTypes.h"

namespace SagaEngine::RHI::Diligent
{

// Placeholder — shader compilation currently lives inside
// DiligentRenderBackend::Impl.  As we refactor, shader compile
// and cache utilities will be extracted here:
//
//   ShaderHandle CompileFromSource(IRHI&, const ShaderDesc& desc);
//   ShaderHandle LoadPrecompiled(IRHI&, const std::vector<uint8_t>& bytecode);
//   void         ClearShaderCache();

} // namespace SagaEngine::RHI::Diligent
