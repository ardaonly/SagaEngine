/// @file NetworkChaosLayer.h
/// @brief Defines deterministic direct-frame network chaos policy contracts.

#pragma once

#include "SagaEngine/Networking/NetworkTypes.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace SagaEngine::Diagnostics
{
class DiagnosticSystem;
}

namespace SagaEngine::Networking
{

using SagaEngine::Networking::ClientId;

namespace NetworkChaosMetrics
{

inline constexpr const char* FramesSeen = "net.chaos.frames_seen";
inline constexpr const char* FramesDelivered = "net.chaos.frames_delivered";
inline constexpr const char* FramesDropped = "net.chaos.frames_dropped";
inline constexpr const char* FramesDuplicated = "net.chaos.frames_duplicated";
inline constexpr const char* FramesDeferred = "net.chaos.frames_deferred";
inline constexpr const char* FramesReleased = "net.chaos.frames_released";
inline constexpr const char* QueueDepth = "net.chaos.queue_depth";
inline constexpr const char* QueueFullDrops = "net.chaos.queue_full_drops";

} // namespace NetworkChaosMetrics

/// Deterministic policy knobs for direct-frame packet chaos.
struct NetworkChaosConfig
{
    bool enabled = false;                 ///< Disabled policy is pass-through.
    std::uint64_t seed = 0;               ///< Stable seed for deterministic rolls.
    std::uint32_t dropPermille = 0;       ///< 0..1000 drop probability.
    std::uint32_t duplicatePermille = 0;  ///< 0..1000 duplicate-once probability.
    std::uint32_t deferPermille = 0;      ///< 0..1000 defer probability.
    std::uint32_t deferTicks = 1;         ///< Explicit tick delay for deferred frames.
    std::size_t maxDeferredFrames = 0;    ///< Hard cap for deferred frame storage.
};

/// Owned direct-frame payload submitted to or emitted by the chaos layer.
struct NetworkChaosFrame
{
    ClientId clientId = 0;                ///< Client associated with the frame.
    std::vector<std::uint8_t> bytes;      ///< Owned raw packet frame bytes.
};

/// Stable decision kind produced for each submitted direct frame.
enum class NetworkChaosDecisionKind : std::uint8_t
{
    Deliver,
    Drop,
    DuplicateOnce,
    Defer,
    QueueFullDrop,
};

/// Deterministic decision metadata for tests, reports, and future tools.
struct NetworkChaosDecision
{
    NetworkChaosDecisionKind kind = NetworkChaosDecisionKind::Deliver;
    std::uint64_t inputSequence = 0;      ///< Monotonic local frame sequence.
    std::uint64_t releaseTick = 0;        ///< Non-zero when a frame is deferred.
    std::size_t queueDepth = 0;           ///< Deferred queue depth after decision.
};

/// Result of applying chaos policy to one submitted direct frame.
struct NetworkChaosResult
{
    NetworkChaosDecision decision;
    std::vector<NetworkChaosFrame> frames;
};

/// Deterministic in-process packet chaos layer for direct/local harnesses.
class NetworkChaosLayer final
{
public:
    explicit NetworkChaosLayer(NetworkChaosConfig config = {});

    /// Attach optional diagnostics. The layer does not own this pointer.
    void SetDiagnostics(
        SagaEngine::Diagnostics::DiagnosticSystem* diagnostics) noexcept;

    /// Replace policy and reset deterministic local state.
    void Reset(NetworkChaosConfig config);

    /// Apply policy to one owned direct frame at the supplied explicit tick.
    [[nodiscard]] NetworkChaosResult ProcessFrame(
        std::uint64_t currentTick,
        NetworkChaosFrame frame);

    /// Release deferred frames whose release tick is now due.
    [[nodiscard]] std::vector<NetworkChaosFrame> ReleaseDueFrames(
        std::uint64_t currentTick);

    [[nodiscard]] const NetworkChaosConfig& Config() const noexcept;
    [[nodiscard]] std::size_t DeferredFrameCount() const noexcept;

private:
    struct DeferredFrame
    {
        std::uint64_t releaseTick = 0;
        NetworkChaosFrame frame;
    };

    [[nodiscard]] std::uint64_t NextRandom() noexcept;
    [[nodiscard]] std::uint32_t RollPermille() noexcept;
    [[nodiscard]] NetworkChaosDecisionKind ChooseDecision();
    [[nodiscard]] std::uint32_t ClampedPermille(std::uint32_t value) const noexcept;

    void RecordCounter(const char* name, double amount = 1.0);
    void RecordQueueDepth();

    NetworkChaosConfig config_;
    SagaEngine::Diagnostics::DiagnosticSystem* diagnostics_ = nullptr;
    std::uint64_t rngState_ = 0;
    std::uint64_t inputSequence_ = 0;
    std::deque<DeferredFrame> deferredFrames_;
};

} // namespace SagaEngine::Networking
