/// @file FragmentAssembler.h
/// @brief MTU-safe packet fragmentation and reassembly for oversize payloads.
///
/// Layer  : SagaServer / Networking / Core
/// Purpose: The wire-level `Packet` class caps a single datagram at
///          `MAX_PACKET_SIZE` (1400 B) so we stay under typical Ethernet MTU
///          minus IPv4/IPv6 + UDP overhead.  Most game traffic fits — small
///          movement updates, input commands, pings.  But WorldSnapshots,
///          large RPC responses, and compressed delta chunks routinely run
///          4–32 KiB.  Rather than raising the per-packet cap (which would
///          fragment at the IP layer with no reassembly control) we split
///          the payload ourselves and expose an explicit assembler.
///
/// Contract:
///   - Fragments share a single 32-bit `messageId` and monotonically
///     increasing `fragmentIndex` from 0 to `fragmentCount - 1`.
///   - Fragments may arrive out of order; duplicates are idempotent.
///   - An assembly is abandoned after `kAssemblyTimeoutMs` if any fragment
///     is still missing — this prevents stale senders from occupying slots
///     forever.
///   - Maximum in-flight assemblies are capped (`kMaxConcurrentAssemblies`)
///     to bound memory usage under malformed-input pressure.
///
/// Threading:
///   Not internally locked.  One `FragmentAssembler` per connection; the
///   connection owns the receive thread, so calls are naturally serial.
///   If you ever share it across threads wrap it in your own mutex.

#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Networking {

// ─── Wire format ─────────────────────────────────────────────────────────────

/// Header prepended to every fragment payload.  12 bytes, packed so the
/// layout is identical on every target.  Sits inside the `Packet` payload —
/// it is NOT a replacement for `PacketHeader`, it rides on top of it.
#pragma pack(push, 1)
struct FragmentHeader
{
    std::uint32_t messageId;      ///< Unique id shared by all fragments of one logical message.
    std::uint16_t fragmentIndex;  ///< 0-based index of this fragment.
    std::uint16_t fragmentCount;  ///< Total number of fragments that make up the message.
    std::uint32_t totalSize;      ///< Size of the fully assembled payload (sanity check).
};
#pragma pack(pop)

static_assert(sizeof(FragmentHeader) == 12, "FragmentHeader must be 12 bytes on the wire");

/// Fragment-capable packets have `kFragmentPayloadBudget` bytes available for
/// actual user data after the `FragmentHeader` subtracts its overhead.
/// Expressed as a constant so `Fragment()` can size its chunks consistently.
inline constexpr std::size_t kFragmentHeaderSize   = sizeof(FragmentHeader);

// ─── Fragmentation helper ────────────────────────────────────────────────────

/// Plain chunk struct returned by the splitter.  Owns its bytes — `data`
/// already includes the `FragmentHeader` prepended.
struct Fragment
{
    std::uint32_t     messageId     = 0;
    std::uint16_t     fragmentIndex = 0;
    std::uint16_t     fragmentCount = 0;
    std::vector<std::uint8_t> data; ///< Wire payload: header + chunk bytes.
};

// ─── Assembler ───────────────────────────────────────────────────────────────

/// Reassembles fragments into their original payload.  Holds partial state
/// indexed by `messageId` until either the message is complete or the
/// assembly slot ages out.
class FragmentAssembler
{
public:
    /// Status returned by `Feed()`.  Using an enum instead of a bool lets
    /// the caller distinguish "just stored, waiting for more" from
    /// "complete, take the buffer" from "rejected, don't count this against
    /// bandwidth".
    enum class FeedResult : std::uint8_t
    {
        Stored,     ///< Fragment accepted, message still incomplete.
        Complete,   ///< Last fragment arrived; `outPayload` now holds the full message.
        Duplicate,  ///< Fragment index was already seen for this messageId.
        Invalid     ///< Malformed header or assembly table full.
    };

    /// Limits — see class docstring for why these exist.
    static constexpr std::size_t   kMaxConcurrentAssemblies = 64;
    static constexpr std::size_t   kMaxFragmentCount        = 256;
    static constexpr std::size_t   kMaxAssembledSize        = 256 * 1024; ///< 256 KiB hard cap.
    static constexpr std::uint64_t kAssemblyTimeoutMs       = 3000;       ///< 3 s reassembly window.

    FragmentAssembler() = default;

    FragmentAssembler(const FragmentAssembler&)            = delete;
    FragmentAssembler& operator=(const FragmentAssembler&) = delete;
    FragmentAssembler(FragmentAssembler&&)                 = default;
    FragmentAssembler& operator=(FragmentAssembler&&)      = default;

    /// Feed a single fragment body (header + chunk bytes as they arrived on
    /// the wire).  `nowMs` is the current monotonic clock in milliseconds and
    /// is used both to stamp the assembly and to age out old entries.
    ///
    /// On `Complete`, `outPayload` is filled with the reassembled buffer and
    /// the internal slot is released.  On any other result `outPayload` is
    /// left untouched.
    [[nodiscard]] FeedResult Feed(const std::uint8_t* bytes,
                                  std::size_t         size,
                                  std::uint64_t       nowMs,
                                  std::vector<std::uint8_t>& outPayload);

    /// Drop assemblies whose last-seen timestamp is older than
    /// `kAssemblyTimeoutMs`.  Called opportunistically from `Feed()`, but
    /// transport code may call it directly on an idle tick.
    void EvictExpired(std::uint64_t nowMs);

    /// Diagnostic: how many partial assemblies are currently outstanding.
    [[nodiscard]] std::size_t ActiveAssemblyCount() const noexcept
    {
        return assemblies_.size();
    }

    // ─── Sender-side helper ─────────────────────────────────────────────────

    /// Split `payload` into fragments of `maxFragmentBody` bytes each
    /// (which should be `MAX_PAYLOAD_SIZE - kFragmentHeaderSize`).  Each
    /// returned Fragment already has its `FragmentHeader` prepended in
    /// `data` and can be dropped straight into a `Packet::WriteBytes()`.
    ///
    /// Returns an empty vector if `payload` is empty, if it would exceed
    /// `kMaxFragmentCount`, or if `maxFragmentBody` is zero.
    [[nodiscard]] static std::vector<Fragment> Fragmentize(
        std::uint32_t               messageId,
        const std::uint8_t*         payload,
        std::size_t                 payloadSize,
        std::size_t                 maxFragmentBody);

private:
    /// Partial assembly slot.  Stores the final buffer pre-allocated at the
    /// declared totalSize so out-of-order fragments can memcpy directly into
    /// their final offset — no intermediate queue.
    struct Assembly
    {
        std::uint16_t             fragmentCount = 0;
        std::uint16_t             fragmentsSeen = 0;
        std::uint32_t             totalSize     = 0;
        std::uint64_t             lastUpdateMs  = 0;
        std::vector<bool>         received;                  ///< Bitset of fragmentIndex seen.
        std::vector<std::uint8_t> buffer;                    ///< Pre-sized output buffer.
    };

    std::unordered_map<std::uint32_t, Assembly> assemblies_;
};

} // namespace SagaEngine::Networking
