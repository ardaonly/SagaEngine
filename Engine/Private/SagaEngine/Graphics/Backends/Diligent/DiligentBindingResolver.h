/// @file DiligentBindingResolver.h
/// @brief Private native binding resource resolver for the Diligent adapter.

#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingRecords.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <vector>

namespace Diligent
{
struct IBuffer;
struct IDeviceObject;
struct ISampler;
struct ITextureView;
} // namespace Diligent

namespace SagaEngine::Graphics::Backends::Diligent
{

struct DiligentResolvedBindingResource
{
    const DiligentCompiledBindingEntry* entry = nullptr;
    DiligentNativeBindingResourceRecord record{};
    GraphicsResourceQueryResult query{};
    ::Diligent::IDeviceObject* nativeObject = nullptr;
    ::Diligent::ITextureView* textureView = nullptr;
    ::Diligent::ISampler* sampler = nullptr;
    ::Diligent::IBuffer* buffer = nullptr;
    DiligentBindingFailureReason failure =
        DiligentBindingFailureReason::None;
    bool valid = false;
    bool usedFallback = false;
};

struct DiligentResolvedBindingSet
{
    PipelineHandle pipeline{};
    BindingSetHandle bindingSet{};
    BindingLayoutHandle layout{};
    std::uint64_t pipelineCreationSerial = 0;
    std::uint64_t compatibilityKey = 0;
    GraphicsBindingLayoutDesc canonicalLayout{};
    std::vector<DiligentResolvedBindingResource> resources{};
    std::vector<std::string> diagnostics{};
    DiligentBindingFailureReason failure =
        DiligentBindingFailureReason::None;
    bool valid = false;
};

} // namespace SagaEngine::Graphics::Backends::Diligent
