/// @file PacketDemux.cpp
/// @brief Network-to-pipeline bridge: demuxes replication packets.

#include "SagaEngine/Client/Replication/PacketDemux.h"

#include "SagaEngine/Core/Log/Log.h"

#include <cstring>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

// Packet header size (from Packet.h in the Server module).
inline constexpr std::size_t kPacketHeaderSize = 20;

// PacketType values (from NetworkTypes.h).
inline constexpr std::uint16_t kPacketTypeSnapshot      = 24;
inline constexpr std::uint16_t kPacketTypeDeltaSnapshot = 25;

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void PacketDemux::Configure(PacketDemuxConfig config) noexcept
{
    config_ = config;
}

// ─── Enqueue (network thread, single-producer) ─────────────────────────────

bool PacketDemux::Enqueue(
    std::uint16_t  packetType,
    std::uint64_t  sequence,
    const std::uint8_t* payload,
    std::uint32_t  payloadSize,
    std::uint64_t  recvTick) noexcept
{
    // Bounds check.
    if (payloadSize > config_.maxPayloadSize)
    {
        ++stats_.packetsDropped;
        return false;
    }

    // SPSC ring buffer: atomic head, non-atomic tail (game thread only).
    std::uint32_t currentHead = head_.load(std::memory_order_relaxed);
    std::uint32_t nextHead = (currentHead + 1) & (ring_.size() - 1);

    // Check if queue is full.
    if (nextHead == tail_)
    {
        ++stats_.packetsDropped;
        return false;
    }

    // Write the packet.
    DemuxedPacket& pkt = ring_[currentHead];
    pkt.packetType  = packetType;
    pkt.sequence    = sequence;
    pkt.recvTick    = recvTick;
    pkt.payloadSize = payloadSize;
    std::memcpy(pkt.payload, payload, payloadSize);

    // Publish.
    head_.store(nextHead, std::memory_order_release);
    ++stats_.packetsEnqueued;

    return true;
}

// ─── Callbacks ──────────────────────────────────────────────────────────────

void PacketDemux::SetCallbacks(
    OnFullSnapshotFn    onFull,
    OnDeltaSnapshotFn   onDelta,
    OnBoundsViolationFn onViolation) noexcept
{
    onFull_      = std::move(onFull);
    onDelta_     = std::move(onDelta);
    onViolation_ = std::move(onViolation);
}

void PacketDemux::SetStateMachine(ReplicationStateMachine* sm) noexcept
{
    stateMachine_ = sm;
}

// ─── Process queue (game thread, single-consumer) ──────────────────────────

void PacketDemux::ProcessQueue(ServerTick currentTick) noexcept
{
    while (tail_ != head_.load(std::memory_order_acquire))
    {
        const DemuxedPacket& pkt = ring_[tail_];
        DecodeAndDispatch(pkt, currentTick);

        // Advance tail.
        tail_ = (tail_ + 1) & (ring_.size() - 1);
        ++stats_.packetsDequeued;
    }

    stats_.queueDepth = static_cast<std::uint32_t>(
        (head_.load(std::memory_order_relaxed) - tail_ + ring_.size()) % ring_.size());
}

PacketDemuxStats PacketDemux::Stats() const noexcept
{
    PacketDemuxStats s = stats_;
    s.queueDepth = static_cast<std::uint32_t>(
        (head_.load(std::memory_order_relaxed) - tail_ + ring_.size()) % ring_.size());
    return s;
}

// ─── Decode and dispatch ───────────────────────────────────────────────────

void PacketDemux::DecodeAndDispatch(const DemuxedPacket& pkt, ServerTick currentTick) noexcept
{
    (void)currentTick;

    // State machine acceptance check.
    if (stateMachine_)
    {
        bool isFull = (pkt.packetType == kPacketTypeSnapshot);
        AcceptResult result = stateMachine_->AcceptPacket(
            static_cast<ServerTick>(pkt.recvTick), isFull);

        if (result == AcceptResult::Drop)
        {
            ++stats_.packetsDropped;
            return;
        }

        // Buffer is handled by the state machine — the packet stays in the
        // jitter buffer inside the pipeline.  We still decode it here
        // so the pipeline has structured data to buffer.
        // Accept proceeds to decode below.
    }

    // Record sequence for gap detection.
    if (stateMachine_)
        stateMachine_->RecordSequence(pkt.sequence);

    // Decode based on packet type.
    switch (pkt.packetType)
    {
        case kPacketTypeSnapshot:
        {
            SnapshotDecodeResult result = DecodeSnapshotWire(
                pkt.payload, pkt.payloadSize);

            if (!result.success)
            {
                LOG_WARN(kLogTag,
                         "PacketDemux: snapshot decode failed: %s",
                         result.error.c_str());
                ++stats_.decodeFailures;
                if (onViolation_)
                    onViolation_(result.error.c_str());
                return;
            }

            // Convert to pipeline's DecodedWorldSnapshot.
            DecodedWorldSnapshot snap;
            snap.serverTick  = result.decoded.serverTick;
            snap.entityCount = result.decoded.entityCount;
            snap.crc32       = result.decoded.payloadCRC32;

            // The full snapshot wire format carries entity/component data
            // that needs to be serialized into the pipeline's payload blob.
            // For the full snapshot, the pipeline expects a WorldState
            // serialised blob.  We reconstruct it from the decoded entities.
            // This is a simplification — a production system would carry
            // the raw WorldState::Serialize() blob directly.
            // For now, forward the decoded data through the callback.
            if (onFull_)
            {
                // Build a minimal payload that the apply function can use.
                // The full snapshot apply function will deserialize the
                // WorldState blob.  We need to construct one from the
                // decoded entity records.
                // This is a placeholder — the real implementation would
                // serialize the decoded entities back into a WorldState blob.
                snap.payload = result.decoded.payload;
                onFull_(std::move(snap));
            }
            break;
        }

        case kPacketTypeDeltaSnapshot:
        {
            DeltaDecodeResult result = DecodeDeltaWire(
                pkt.payload, pkt.payloadSize);

            if (!result.success)
            {
                LOG_WARN(kLogTag,
                         "PacketDemux: delta decode failed: %s",
                         result.error.c_str());
                ++stats_.decodeFailures;
                if (onViolation_)
                    onViolation_(result.error.c_str());
                return;
            }

            // Convert to pipeline's DecodedDeltaSnapshot.
            DecodedDeltaSnapshot delta;
            delta.baselineTick     = result.decoded.baselineTick;
            delta.serverTick       = result.decoded.serverTick;
            delta.dirtyEntityCount = result.decoded.entityCount;
            delta.payload          = result.decoded.payload;

            if (onDelta_)
                onDelta_(std::move(delta));
            break;
        }

        default:
        {
            // Unknown packet type for replication — ignore.
            break;
        }
    }
}

} // namespace SagaEngine::Client::Replication
