/// @file NetworkChaosLayer.cpp
/// @brief Implements deterministic direct-frame network chaos policy.

#include "SagaEngine/Networking/NetworkChaosLayer.h"

#include "SagaEngine/Diagnostics/DiagnosticSystem.hpp"

#include <algorithm>
#include <utility>

namespace SagaEngine::Networking
{
namespace
{

constexpr std::uint64_t kSplitMixIncrement = 0x9E3779B97F4A7C15ull;

} // namespace

NetworkChaosLayer::NetworkChaosLayer(NetworkChaosConfig config)
{
    Reset(std::move(config));
}

void NetworkChaosLayer::SetDiagnostics(
    SagaEngine::Diagnostics::DiagnosticSystem* diagnostics) noexcept
{
    diagnostics_ = diagnostics;
    RecordQueueDepth();
}

void NetworkChaosLayer::Reset(NetworkChaosConfig config)
{
    config_ = std::move(config);
    rngState_ = config_.seed;
    inputSequence_ = 0;
    deferredFrames_.clear();
    RecordQueueDepth();
}

NetworkChaosResult NetworkChaosLayer::ProcessFrame(
    std::uint64_t currentTick,
    NetworkChaosFrame frame)
{
    NetworkChaosResult result;
    result.decision.inputSequence = ++inputSequence_;

    if (!config_.enabled)
    {
        result.decision.queueDepth = deferredFrames_.size();
        result.frames.push_back(std::move(frame));
        return result;
    }

    RecordCounter(NetworkChaosMetrics::FramesSeen);

    const NetworkChaosDecisionKind decision = ChooseDecision();
    result.decision.kind = decision;

    switch (decision)
    {
        case NetworkChaosDecisionKind::Drop:
            RecordCounter(NetworkChaosMetrics::FramesDropped);
            break;

        case NetworkChaosDecisionKind::DuplicateOnce:
            result.frames.push_back(frame);
            result.frames.push_back(std::move(frame));
            RecordCounter(NetworkChaosMetrics::FramesDelivered, 2.0);
            RecordCounter(NetworkChaosMetrics::FramesDuplicated);
            break;

        case NetworkChaosDecisionKind::Defer:
        {
            if (deferredFrames_.size() >= config_.maxDeferredFrames)
            {
                result.decision.kind = NetworkChaosDecisionKind::QueueFullDrop;
                RecordCounter(NetworkChaosMetrics::QueueFullDrops);
                RecordCounter(NetworkChaosMetrics::FramesDropped);
                break;
            }

            const std::uint64_t delay = std::max<std::uint32_t>(
                config_.deferTicks,
                1u);
            result.decision.releaseTick = currentTick + delay;
            deferredFrames_.push_back(
                DeferredFrame{result.decision.releaseTick, std::move(frame)});
            RecordCounter(NetworkChaosMetrics::FramesDeferred);
            break;
        }

        case NetworkChaosDecisionKind::QueueFullDrop:
            RecordCounter(NetworkChaosMetrics::QueueFullDrops);
            RecordCounter(NetworkChaosMetrics::FramesDropped);
            break;

        case NetworkChaosDecisionKind::Deliver:
        default:
            result.frames.push_back(std::move(frame));
            RecordCounter(NetworkChaosMetrics::FramesDelivered);
            break;
    }

    result.decision.queueDepth = deferredFrames_.size();
    RecordQueueDepth();
    return result;
}

std::vector<NetworkChaosFrame> NetworkChaosLayer::ReleaseDueFrames(
    std::uint64_t currentTick)
{
    std::vector<NetworkChaosFrame> released;
    for (auto it = deferredFrames_.begin(); it != deferredFrames_.end();)
    {
        if (it->releaseTick > currentTick)
        {
            ++it;
            continue;
        }

        released.push_back(std::move(it->frame));
        it = deferredFrames_.erase(it);
    }

    if (!released.empty())
    {
        RecordCounter(
            NetworkChaosMetrics::FramesReleased,
            static_cast<double>(released.size()));
        RecordCounter(
            NetworkChaosMetrics::FramesDelivered,
            static_cast<double>(released.size()));
    }
    RecordQueueDepth();
    return released;
}

const NetworkChaosConfig& NetworkChaosLayer::Config() const noexcept
{
    return config_;
}

std::size_t NetworkChaosLayer::DeferredFrameCount() const noexcept
{
    return deferredFrames_.size();
}

std::uint64_t NetworkChaosLayer::NextRandom() noexcept
{
    rngState_ += kSplitMixIncrement;
    std::uint64_t value = rngState_;
    value = (value ^ (value >> 30u)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27u)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31u);
}

std::uint32_t NetworkChaosLayer::RollPermille() noexcept
{
    return static_cast<std::uint32_t>(NextRandom() % 1000u);
}

NetworkChaosDecisionKind NetworkChaosLayer::ChooseDecision()
{
    const std::uint32_t roll = RollPermille();
    const std::uint32_t drop = ClampedPermille(config_.dropPermille);
    const std::uint32_t duplicate = ClampedPermille(config_.duplicatePermille);
    const std::uint32_t defer = ClampedPermille(config_.deferPermille);

    if (roll < drop)
    {
        return NetworkChaosDecisionKind::Drop;
    }
    if (roll < drop + duplicate)
    {
        return NetworkChaosDecisionKind::DuplicateOnce;
    }
    if (roll < drop + duplicate + defer)
    {
        return NetworkChaosDecisionKind::Defer;
    }
    return NetworkChaosDecisionKind::Deliver;
}

std::uint32_t NetworkChaosLayer::ClampedPermille(
    std::uint32_t value) const noexcept
{
    return std::min<std::uint32_t>(value, 1000u);
}

void NetworkChaosLayer::RecordCounter(const char* name, double amount)
{
    if (!diagnostics_)
    {
        return;
    }

    diagnostics_->Health().IncrementCounter(name, amount);
}

void NetworkChaosLayer::RecordQueueDepth()
{
    if (!diagnostics_)
    {
        return;
    }

    diagnostics_->Health().SetGauge(
        NetworkChaosMetrics::QueueDepth,
        static_cast<double>(deferredFrames_.size()));
}

} // namespace SagaEngine::Networking
