/// @file FrameResources.h
/// @brief Vendor-neutral frame resource vocabulary.

#pragma once

#include <cstdint>

namespace SagaEngine::Graphics
{

enum class GraphicsFrameResourceClass : std::uint8_t
{
    Persistent = 0,
    PerFrameTransient,
    SwapchainSized,
};

struct GraphicsFrameResourceSetDesc
{
    // CPU-side ownership and budget vocabulary only. This does not claim native
    // multi-frame mutation, GPU submission behavior, or no-submit policy.
    GraphicsFrameResourceClass resourceClass =
        GraphicsFrameResourceClass::PerFrameTransient;
    std::uint32_t maxFramesInFlight = 1;
    std::uint64_t bytesPerFrame = 0;
    std::uint64_t defaultAlignment = 16;
};

struct GraphicsFrameResourceBudget
{
    GraphicsFrameResourceClass resourceClass =
        GraphicsFrameResourceClass::PerFrameTransient;
    std::uint64_t currentFrameBytes = 0;
    std::uint64_t peakFrameBytes = 0;
    std::uint64_t failedAllocationCount = 0;
    std::uint32_t maxFramesInFlight = 0;
};

} // namespace SagaEngine::Graphics
