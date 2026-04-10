/// @file PacketRegistry.cpp
/// @brief PacketRegistry implementation — thread-safe dispatch and statistics.

#include "SagaServer/Networking/Core/PacketRegistry.h"
#include "SagaEngine/Core/Log/Log.h"

#include <chrono>
#include <mutex>
#include <utility>

namespace SagaEngine::Networking
{

static constexpr const char* kTag = "PacketRegistry";

namespace
{

[[nodiscard]] std::uint64_t NowUnixMs() noexcept
{
    using clock = std::chrono::system_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count());
}

} // anonymous namespace

// ─── Construction ─────────────────────────────────────────────────────────────

PacketRegistry::PacketRegistry(PacketRegistryConfig config)
    : m_Config(config)
{
    m_Handlers.reserve(32);
    m_PerTypeStats.reserve(32);
}

// ─── Handler Management ───────────────────────────────────────────────────────

void PacketRegistry::RegisterHandler(PacketType type, PacketHandlerFn handler)
{
    if (!handler)
    {
        LOG_WARN(kTag, "RegisterHandler: null handler for type %u",
                 static_cast<unsigned>(type));
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    auto& entry   = m_Handlers[type];
    entry.handler = std::move(handler);

    LOG_INFO(kTag, "Handler registered for %s",
             PacketTypeToString(type));
}

void PacketRegistry::UnregisterHandler(PacketType type)
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_Handlers.erase(type);
}

void PacketRegistry::AddValidator(PacketType type, PacketValidatorFn validator)
{
    if (!validator)
        return;

    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_Handlers[type].validators.push_back(std::move(validator));
}

void PacketRegistry::ClearValidators(PacketType type)
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    auto it = m_Handlers.find(type);
    if (it != m_Handlers.end())
        it->second.validators.clear();
}

void PacketRegistry::SetFallbackHandler(PacketHandlerFn handler)
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_FallbackHandler = std::move(handler);
}

bool PacketRegistry::HasHandler(PacketType type) const
{
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    return m_Handlers.find(type) != m_Handlers.end();
}

// ─── Envelope Validation ──────────────────────────────────────────────────────

bool PacketRegistry::ValidateEnvelope(const PacketContext& ctx,
                                       PacketHandleResult&  earlyResult) const
{
    if (ctx.type == PacketType::Invalid)
    {
        earlyResult = PacketHandleResult::MalformedIn;
        return false;
    }

    if (ctx.payloadSize > m_Config.maxPayloadBytes)
    {
        LOG_WARN(kTag,
                 "Oversized packet from client %llu — %zu bytes (cap %zu)",
                 static_cast<unsigned long long>(ctx.clientId),
                 ctx.payloadSize,
                 m_Config.maxPayloadBytes);
        earlyResult = PacketHandleResult::Rejected;
        return false;
    }

    if (ctx.payloadSize > 0 && ctx.payload == nullptr)
    {
        earlyResult = PacketHandleResult::MalformedIn;
        return false;
    }

    return true;
}

// ─── Dispatch ─────────────────────────────────────────────────────────────────

PacketHandleResult PacketRegistry::Dispatch(const PacketContext& ctx)
{
    PacketHandleResult early = PacketHandleResult::Ok;
    const std::uint64_t now  = NowUnixMs();

    if (!ValidateEnvelope(ctx, early))
    {
        BumpStatistics(ctx.type, ctx.payloadSize, early, now);
        return early;
    }

    PacketHandlerFn                 handler;
    std::vector<PacketValidatorFn>  validators;
    bool                            usedFallback = false;

    {
        std::shared_lock<std::shared_mutex> lock(m_Mutex);

        auto it = m_Handlers.find(ctx.type);
        if (it != m_Handlers.end() && it->second.handler)
        {
            handler    = it->second.handler;
            validators = it->second.validators;
        }
        else if (m_FallbackHandler)
        {
            handler      = m_FallbackHandler;
            usedFallback = true;
        }
    }

    if (!handler)
    {
        if (m_Config.logUnknownTypes)
        {
            LOG_WARN(kTag,
                     "No handler for %s from client %llu",
                     PacketTypeToString(ctx.type),
                     static_cast<unsigned long long>(ctx.clientId));
        }
        BumpStatistics(ctx.type, ctx.payloadSize, PacketHandleResult::Unknown, now);
        return PacketHandleResult::Unknown;
    }

    for (const auto& validator : validators)
    {
        if (!validator(ctx))
        {
            BumpStatistics(ctx.type, ctx.payloadSize, PacketHandleResult::Rejected, now);
            return PacketHandleResult::Rejected;
        }
    }

    PacketHandleResult result = PacketHandleResult::Rejected;

    try
    {
        result = handler(ctx);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(kTag,
                  "Handler exception for %s (client %llu): %s",
                  PacketTypeToString(ctx.type),
                  static_cast<unsigned long long>(ctx.clientId),
                  e.what());
        result = PacketHandleResult::MalformedIn;
    }
    catch (...)
    {
        LOG_ERROR(kTag,
                  "Handler exception (unknown) for %s (client %llu)",
                  PacketTypeToString(ctx.type),
                  static_cast<unsigned long long>(ctx.clientId));
        result = PacketHandleResult::MalformedIn;
    }

    if (usedFallback && result == PacketHandleResult::Ok)
        result = PacketHandleResult::Ok;

    BumpStatistics(ctx.type, ctx.payloadSize, result, now);
    return result;
}

std::size_t PacketRegistry::DispatchBatch(const PacketContext* contexts, std::size_t count)
{
    if (!contexts || count == 0)
        return 0;

    std::size_t okCount = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (Dispatch(contexts[i]) == PacketHandleResult::Ok)
            ++okCount;
    }
    return okCount;
}

// ─── Statistics ───────────────────────────────────────────────────────────────

void PacketRegistry::BumpStatistics(PacketType         type,
                                     std::size_t        bytes,
                                     PacketHandleResult result,
                                     std::uint64_t      nowUnixMs)
{
    if (!m_Config.trackPerTypeStats)
        return;

    std::lock_guard<std::mutex> lock(m_StatsMutex);

    auto& t = m_PerTypeStats[type];
    ++t.received;
    t.totalBytes     += bytes;
    t.lastSeenUnixMs  = nowUnixMs;

    ++m_AggregateStats.totalReceived;
    m_AggregateStats.totalBytes += bytes;

    switch (result)
    {
        case PacketHandleResult::Ok:
            ++t.handled;
            ++m_AggregateStats.totalHandled;
            break;
        case PacketHandleResult::Rejected:
            ++t.rejected;
            ++m_AggregateStats.totalRejected;
            break;
        case PacketHandleResult::MalformedIn:
            ++t.malformed;
            ++m_AggregateStats.totalMalformed;
            break;
        case PacketHandleResult::Throttled:
            ++t.throttled;
            ++m_AggregateStats.totalThrottled;
            break;
        case PacketHandleResult::Unknown:
            ++t.unknownHandler;
            ++m_AggregateStats.totalUnknown;
            break;
    }
}

PacketRegistryStats PacketRegistry::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(m_StatsMutex);

    PacketRegistryStats snapshot = m_AggregateStats;

    {
        std::shared_lock<std::shared_mutex> hlock(m_Mutex);
        snapshot.registeredTypes = m_Handlers.size();
    }

    return snapshot;
}

std::optional<PacketTypeStats> PacketRegistry::GetTypeStatistics(PacketType type) const
{
    std::lock_guard<std::mutex> lock(m_StatsMutex);
    auto it = m_PerTypeStats.find(type);
    if (it == m_PerTypeStats.end())
        return std::nullopt;
    return it->second;
}

void PacketRegistry::ResetStatistics()
{
    std::lock_guard<std::mutex> lock(m_StatsMutex);
    m_PerTypeStats.clear();
    m_AggregateStats = {};
}

} // namespace SagaEngine::Networking
