/// @file IPubSubBus.h
/// @brief Publish / subscribe event bus for cross-process notifications.
///
/// Layer  : Backends / Services / Persistence / Database
/// Purpose: The shard mesh (section 6) handles point-to-point
///          gameplay handoffs, but a lot of operational events are
///          not addressed to a specific peer — they are broadcast
///          "whoever cares, here is what happened".  Examples: a GM
///          posted a cluster-wide announcement, a player's account
///          just got banned, a new content patch rolled out, a
///          guild's name changed.  Routing those through the shard
///          mesh would couple unrelated zones to the mesh topology;
///          routing them through the database directly would turn
///          PostgreSQL into a polling target.  A lightweight pub/sub
///          bus (Redis pub/sub in production) is the natural fit.
///
/// Design rules:
///   - Topics are strings.  No upfront registry; subscribers and
///     publishers agree on the topic names by convention.  The
///     engine ships a small header of canonical topics
///     (`account.banned`, `cluster.announce`, etc.) so name drift is
///     caught in code review instead of at runtime.
///   - Delivery is at-most-once.  Pub/sub backends typically lose
///     messages on reconnect and we do not pretend otherwise; any
///     event that must not be missed has to ride a durable channel
///     (database row, shard-mesh ack'd message).
///   - Subscribers run on a dedicated thread per bus instance so
///     their handlers never contend with the simulation thread.
///     Handlers are expected to be fast and to push heavy work back
///     onto the subscriber's own work queue.
///   - Messages carry an opaque payload.  The bus never tries to
///     parse them; schema is the publisher's and subscriber's
///     problem.
///
/// What this header is NOT:
///   - Not a message queue.  Persistent queues with replay semantics
///     are a separate concern.
///   - Not a request / response channel.  The bus is one-way; the
///     shard mesh is where you go for bidirectional traffic.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Persistence::Database {

// ─── Event envelope ────────────────────────────────────────────────────────

/// One pub/sub message seen by a subscriber.  The `payload` buffer is
/// owned by the bus for the duration of the handler call; handlers
/// that need to retain bytes must copy out of the view.
struct PubSubMessage
{
    std::string_view          topic;
    std::string_view          publisherTag;  ///< Optional debug tag from the publisher.
    std::uint64_t             publishMonoMs = 0;
    std::vector<std::uint8_t> payloadCopy;   ///< Lazily populated if the consumer asks for a copy.
    const std::uint8_t*       payload  = nullptr;
    std::size_t               payloadSize = 0;
};

/// Subscriber callback signature.  Runs on the bus's internal
/// dispatch thread.
using PubSubHandler = std::function<void(const PubSubMessage&)>;

/// Opaque handle returned by `Subscribe`.  Used later to tear down
/// the subscription without keeping the callback around forever.
using SubscriptionHandle = std::uint64_t;
inline constexpr SubscriptionHandle kInvalidSubscription = 0;

// ─── Publication status ────────────────────────────────────────────────────

enum class PubSubStatus : std::uint8_t
{
    Ok = 0,
    NotConnected,
    PayloadTooLarge,
    BackendError,
    ShuttingDown,
    InternalError,
};

// ─── Interface ─────────────────────────────────────────────────────────────

/// Abstract bus.  Typically one instance per process, shared by every
/// subsystem that wants to publish or listen.
class IPubSubBus
{
public:
    virtual ~IPubSubBus() = default;

    /// Connect to the backing bus.  Non-blocking — the implementation
    /// may still be in the middle of its first handshake when Start
    /// returns.  Subscribers added before the handshake finishes are
    /// queued and installed the moment the link comes up.
    [[nodiscard]] virtual bool Start(const std::string& endpoint) = 0;

    /// Drain subscribers, close the connection, stop the dispatch
    /// thread.  In-flight Publish calls settle to `ShuttingDown`.
    virtual void Stop() = 0;

    /// Publish a message.  Returns synchronously with the enqueue
    /// status; actual on-the-wire send happens on the bus's I/O
    /// thread.  Callers that need acknowledgement should use a
    /// durable channel instead.
    [[nodiscard]] virtual PubSubStatus Publish(
        std::string_view    topic,
        const std::uint8_t* payload,
        std::size_t         payloadSize,
        std::string_view    publisherTag = {}) = 0;

    /// Subscribe to a topic.  Returns a non-zero handle on success.
    /// Topic patterns follow the backend's rules; Redis uses glob-
    /// style wildcards.  The handler MUST be fast — anything over
    /// a millisecond starves other subscribers.
    [[nodiscard]] virtual SubscriptionHandle Subscribe(
        std::string_view topicPattern,
        PubSubHandler    handler) = 0;

    /// Cancel a subscription.  Idempotent — unsubscribing an unknown
    /// handle is a no-op.  Guaranteed not to invoke the handler
    /// after the call returns, even if a message was in flight.
    virtual void Unsubscribe(SubscriptionHandle handle) = 0;

    [[nodiscard]] virtual bool IsConnected() const noexcept = 0;
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

using PubSubBusPtr = std::shared_ptr<IPubSubBus>;

// ─── Canonical topics ──────────────────────────────────────────────────────

/// Compiled-in topic names used by the engine itself.  Subsystems may
/// publish their own topics in addition to these; these exist so
/// operator tools and cross-subsystem wiring have stable identifiers.
namespace topics {
inline constexpr const char* kClusterAnnounce = "cluster.announce";
inline constexpr const char* kAccountBanned   = "account.banned";
inline constexpr const char* kAccountUnbanned = "account.unbanned";
inline constexpr const char* kGmBroadcast     = "gm.broadcast";
inline constexpr const char* kPatchRollout    = "patch.rollout";
inline constexpr const char* kConfigReload    = "config.reload";
} // namespace topics

/// Maximum bytes in a published message.  Above this the bus
/// refuses the publish with `PayloadTooLarge`.  Matches the common
/// Redis pub/sub recommendation and keeps one large announcement
/// from stalling a slow subscriber.
inline constexpr std::size_t kMaxPubSubPayloadBytes = 64 * 1024;

} // namespace SagaEngine::Persistence::Database
