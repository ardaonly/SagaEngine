/// @file FragmentAssembler.cpp
/// @brief Implementation of MTU-safe packet fragmentation and reassembly.

#include "SagaServer/Networking/Core/FragmentAssembler.h"

#include <SagaEngine/Core/Log/Log.h>

#include <algorithm>
#include <cstring>

namespace SagaEngine::Networking {

// ─── Feed ────────────────────────────────────────────────────────────────────

FragmentAssembler::FeedResult FragmentAssembler::Feed(
    const std::uint8_t*        bytes,
    std::size_t                size,
    std::uint64_t              nowMs,
    std::vector<std::uint8_t>& outPayload)
{
    // Reject runts that can't even hold the header.  Checked before we
    // touch the hash map so a flood of garbage doesn't churn the table.
    if (bytes == nullptr || size < kFragmentHeaderSize)
    {
        LOG_WARN("Packet", "FragmentAssembler: runt fragment");
        return FeedResult::Invalid;
    }

    FragmentHeader header{};
    std::memcpy(&header, bytes, kFragmentHeaderSize);

    // Reasonableness checks.  A malicious peer could advertise a billion
    // fragments; the limits below bound heap cost per connection.
    if (header.fragmentCount == 0 ||
        header.fragmentCount > kMaxFragmentCount ||
        header.fragmentIndex >= header.fragmentCount ||
        header.totalSize == 0 ||
        header.totalSize > kMaxAssembledSize)
    {
        LOG_WARN("Packet",
                 "FragmentAssembler: header bounds rejected (count=%u idx=%u size=%u)",
                 static_cast<unsigned>(header.fragmentCount),
                 static_cast<unsigned>(header.fragmentIndex),
                 static_cast<unsigned>(header.totalSize));
        return FeedResult::Invalid;
    }

    const std::size_t bodySize = size - kFragmentHeaderSize;

    // Opportunistic eviction on every feed keeps the hash map from bloating
    // if messages are never completed.  Cheap — at most kMaxConcurrentAssemblies
    // entries to scan.
    EvictExpired(nowMs);

    auto it = assemblies_.find(header.messageId);
    if (it == assemblies_.end())
    {
        // New message.  Reject the slot if we're already at capacity — a
        // misbehaving peer cannot pin the assembler by starting fresh
        // messages without finishing them.
        if (assemblies_.size() >= kMaxConcurrentAssemblies)
        {
            LOG_WARN("Packet", "FragmentAssembler: slot table full");
            return FeedResult::Invalid;
        }

        Assembly fresh;
        fresh.fragmentCount = header.fragmentCount;
        fresh.fragmentsSeen = 0;
        fresh.totalSize     = header.totalSize;
        fresh.lastUpdateMs  = nowMs;
        fresh.received.assign(header.fragmentCount, false);
        fresh.buffer.resize(header.totalSize);

        it = assemblies_.emplace(header.messageId, std::move(fresh)).first;
    }
    else
    {
        // Sanity: once a message is in flight its shape must not change.
        // Mismatching fragmentCount or totalSize means either a bug on the
        // sender or a spoofed packet; kill the slot to fail safe.
        Assembly& existing = it->second;
        if (existing.fragmentCount != header.fragmentCount ||
            existing.totalSize     != header.totalSize)
        {
            LOG_WARN("Packet",
                     "FragmentAssembler: shape mismatch on mid=%u, dropping",
                     static_cast<unsigned>(header.messageId));
            assemblies_.erase(it);
            return FeedResult::Invalid;
        }
    }

    Assembly& slot = it->second;

    // Compute where this fragment lands in the pre-sized output buffer.
    // Every fragment except the last must use the same "normal" chunk size;
    // the last one fills whatever remains.  We derive the normal chunk size
    // from totalSize and fragmentCount rather than trusting the peer to
    // tell us in every header — that keeps the wire format small.
    const std::uint32_t normalChunk = (slot.totalSize + slot.fragmentCount - 1) / slot.fragmentCount;
    const std::uint32_t offset      = static_cast<std::uint32_t>(header.fragmentIndex) * normalChunk;
    const std::uint32_t expectedSize =
        (header.fragmentIndex + 1u == header.fragmentCount)
            ? (slot.totalSize - offset)
            : normalChunk;

    if (bodySize != expectedSize || offset + bodySize > slot.totalSize)
    {
        LOG_WARN("Packet",
                 "FragmentAssembler: bad fragment size (got=%zu expect=%u) mid=%u",
                 bodySize,
                 static_cast<unsigned>(expectedSize),
                 static_cast<unsigned>(header.messageId));
        // Leave the slot in place — an attacker shouldn't be able to kill
        // an in-flight assembly by injecting one bad fragment id.
        return FeedResult::Invalid;
    }

    if (slot.received[header.fragmentIndex])
    {
        slot.lastUpdateMs = nowMs; // Refresh liveness on valid duplicates.
        return FeedResult::Duplicate;
    }

    std::memcpy(slot.buffer.data() + offset, bytes + kFragmentHeaderSize, bodySize);
    slot.received[header.fragmentIndex] = true;
    slot.fragmentsSeen += 1;
    slot.lastUpdateMs = nowMs;

    if (slot.fragmentsSeen == slot.fragmentCount)
    {
        outPayload = std::move(slot.buffer);
        assemblies_.erase(it);
        return FeedResult::Complete;
    }

    return FeedResult::Stored;
}

// ─── EvictExpired ────────────────────────────────────────────────────────────

void FragmentAssembler::EvictExpired(std::uint64_t nowMs)
{
    // Linear scan — the table is bounded to kMaxConcurrentAssemblies (64),
    // so this is cheap relative to the memcpy work in Feed().
    for (auto it = assemblies_.begin(); it != assemblies_.end(); )
    {
        if (nowMs - it->second.lastUpdateMs >= kAssemblyTimeoutMs)
        {
            LOG_WARN("Packet",
                     "FragmentAssembler: evicted stalled message id=%u (%u/%u)",
                     static_cast<unsigned>(it->first),
                     static_cast<unsigned>(it->second.fragmentsSeen),
                     static_cast<unsigned>(it->second.fragmentCount));
            it = assemblies_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// ─── Fragmentize ─────────────────────────────────────────────────────────────

std::vector<Fragment> FragmentAssembler::Fragmentize(
    std::uint32_t       messageId,
    const std::uint8_t* payload,
    std::size_t         payloadSize,
    std::size_t         maxFragmentBody)
{
    std::vector<Fragment> out;

    if (payload == nullptr || payloadSize == 0 || maxFragmentBody == 0)
        return out;

    // ceil(payloadSize / maxFragmentBody) without overflow.
    const std::size_t count = (payloadSize + maxFragmentBody - 1) / maxFragmentBody;
    if (count > kMaxFragmentCount)
    {
        LOG_ERROR("Packet",
                  "FragmentAssembler: payload %zu B would exceed kMaxFragmentCount",
                  payloadSize);
        return out;
    }

    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t offset   = i * maxFragmentBody;
        const std::size_t chunkLen = std::min(maxFragmentBody, payloadSize - offset);

        Fragment frag;
        frag.messageId     = messageId;
        frag.fragmentIndex = static_cast<std::uint16_t>(i);
        frag.fragmentCount = static_cast<std::uint16_t>(count);
        frag.data.resize(kFragmentHeaderSize + chunkLen);

        FragmentHeader header{};
        header.messageId     = messageId;
        header.fragmentIndex = static_cast<std::uint16_t>(i);
        header.fragmentCount = static_cast<std::uint16_t>(count);
        header.totalSize     = static_cast<std::uint32_t>(payloadSize);
        std::memcpy(frag.data.data(), &header, kFragmentHeaderSize);
        std::memcpy(frag.data.data() + kFragmentHeaderSize, payload + offset, chunkLen);

        out.emplace_back(std::move(frag));
    }

    return out;
}

} // namespace SagaEngine::Networking
