/// @file DiligentBindingCompiler.h
/// @brief Private compiler from Saga binding vocabulary to Diligent metadata.

#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingRecords.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

namespace SagaEngine::Graphics::Backends::Diligent
{

[[nodiscard]] DiligentCompiledBindingLayout CompileDiligentBindingLayout(
    BindingLayoutHandle handle,
    const GraphicsBindingLayoutDesc& normalizedLayout);

[[nodiscard]] DiligentNativeBindingSetRecord CompileDiligentNativeBindingSet(
    BindingSetHandle handle,
    const GraphicsBindingSetDesc& normalizedBindingSet,
    const DiligentCompiledBindingLayout& compiledLayout,
    GraphicsBindingResourceQueryFn queryResource,
    void* userData);

} // namespace SagaEngine::Graphics::Backends::Diligent
