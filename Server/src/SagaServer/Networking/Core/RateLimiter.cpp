/// @file RateLimiter.cpp
/// @brief RateLimiter implementation — token bucket with quarantine cooldown.

#include "SagaServer/Networking/Core/RateLimiter.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <chrono>

namespace SagaEngine::Networking
{

static constexpr const char* kTag = "RateLimiter";

using clock   = std::chrono::steady_clock;
using seconds = std::chrono::duration<float>;

namespace
{

[[nodiscard]] std::uint64_t NowUnixMs() noexcept
{
    using sysclock = std::chrono::system_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            sysclock::now().time_since_epoch()).count());
}

} // anonymous namespace

// ─── Construction ─────────────────────────────────────────────────────────────

RateLimiter::RateLimiter(RateLimiterConfig config)
    : m_Config(config)
{
    if (m_Config.packetsPerSecond <= 0.0f) m_Config.packetsPerSecond = 1.0f;
    if (m_Config.bytesPerSecond   <= 0.0f) m_Config.bytesPerSecond   = 1.0f;
    if (m_Config.packetBurst      <= 0.0f) m_Config.packetBurst      = m_Config.packetsPerSecond;
    if (m_Config.bytesBurst       <= 0.0f) m_Config.bytesBurst       = m_Config.bytesPerSecond;
}

// ─── Bucket Acquisition ───────────────────────────────────────────────────────

RateLimiter::Bucket* RateLimiter::FetchBucket(ClientId clientId)
{
    {
        std::shared_lock<std::shared_mutex> lock(m_Mutex);
        auto it = m_Buckets.find(clientId);
        if (it != m_Buckets.end())
            return it->second.get();
    }

    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    auto it = m_Buckets.find(clientId);
    if (it != m_Buckets.end())
        return it->second.get();

    auto bucket = std::make_unique<Bucket>();
    bucket->packetTokens       = m_Config.packetBurst;
    bucket->byteTokens         = m_Config.bytesBurst;
    bucket->lastRefill         = clock::now();
    bucket->lastViolationDecay = bucket->lastRefill;
    bucket->quarantineUntil    = clock::time_point{};

    Bucket* raw = bucket.get();
    m_Buckets.emplace(clientId, std::move(bucket));
    return raw;
}

// ─── Refill / Decay ───────────────────────────────────────────────────────────

void RateLimiter::RefillLocked(Bucket& bucket, clock::time_point now) const
{
    const float elapsed = std::chrono::duration_cast<seconds>(now - bucket.lastRefill).count();
    if (elapsed <= 0.0f)
        return;

    bucket.packetTokens = std::min(
        m_Config.packetBurst,
        bucket.packetTokens + elapsed * m_Config.packetsPerSecond);

    bucket.byteTokens = std::min(
        m_Config.bytesBurst,
        bucket.byteTokens + elapsed * m_Config.bytesPerSecond);

    bucket.lastRefill = now;
}

void RateLimiter::DecayViolationsLocked(Bucket& bucket, clock::time_point now) const
{
    if (bucket.violations == 0)
    {
        bucket.lastViolationDecay = now;
        return;
    }

    const auto elapsed = now - bucket.lastViolationDecay;
    if (elapsed < m_Config.violationDecayInterval)
        return;

    const auto ticks = elapsed / m_Config.violationDecayInterval;
    const std::uint32_t decay = static_cast<std::uint32_t>(ticks);
    if (decay == 0)
        return;

    bucket.violations = (decay >= bucket.violations) ? 0u : (bucket.violations - decay);
    bucket.lastViolationDecay = now;
}

// ─── Allow ────────────────────────────────────────────────────────────────────

bool RateLimiter::Allow(ClientId clientId, std::size_t packetBytes)
{
    Bucket* bucketPtr = FetchBucket(clientId);
    if (!bucketPtr)
        return false;

    Bucket& bucket = *bucketPtr;
    std::lock_guard<std::mutex> lock(bucket.mutex);

    const auto now = clock::now();

    if (bucket.quarantineUntil > clock::time_point{} && now < bucket.quarantineUntil)
    {
        ++bucket.rejectedPackets;
        bucket.rejectedBytes += packetBytes;
        return false;
    }

    if (bucket.quarantineUntil > clock::time_point{} && now >= bucket.quarantineUntil)
    {
        bucket.quarantineUntil = clock::time_point{};
        bucket.packetTokens    = m_Config.packetBurst * 0.5f;
        bucket.byteTokens      = m_Config.bytesBurst  * 0.5f;
        bucket.lastRefill      = now;
    }

    RefillLocked(bucket, now);
    DecayViolationsLocked(bucket, now);

    const float bytesF = static_cast<float>(packetBytes);

    if (bucket.packetTokens < 1.0f || bucket.byteTokens < bytesF)
    {
        ++bucket.rejectedPackets;
        bucket.rejectedBytes += packetBytes;
        ++bucket.violations;

        if (bucket.violations >= m_Config.maxViolationsBeforeQuarantine)
        {
            bucket.quarantineUntil = now + m_Config.quarantineDuration;
            LOG_WARN(kTag,
                "Client %llu quarantined for %lld ms after %u violations",
                static_cast<unsigned long long>(clientId),
                static_cast<long long>(m_Config.quarantineDuration.count()),
                bucket.violations);
        }

        return false;
    }

    bucket.packetTokens -= 1.0f;
    bucket.byteTokens   -= bytesF;
    ++bucket.allowedPackets;
    bucket.allowedBytes += packetBytes;

    return true;
}

// ─── Management ───────────────────────────────────────────────────────────────

void RateLimiter::ReleaseClient(ClientId clientId)
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_Buckets.erase(clientId);
}

void RateLimiter::Clear()
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_Buckets.clear();
}

bool RateLimiter::IsQuarantined(ClientId clientId) const
{
    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    auto it = m_Buckets.find(clientId);
    if (it == m_Buckets.end())
        return false;

    std::lock_guard<std::mutex> bucketLock(it->second->mutex);
    const auto now = clock::now();
    return it->second->quarantineUntil > clock::time_point{} &&
           now < it->second->quarantineUntil;
}

void RateLimiter::ClearViolations(ClientId clientId)
{
    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    auto it = m_Buckets.find(clientId);
    if (it == m_Buckets.end())
        return;

    std::lock_guard<std::mutex> bucketLock(it->second->mutex);
    it->second->violations         = 0;
    it->second->quarantineUntil    = clock::time_point{};
    it->second->lastViolationDecay = clock::now();
}

// ─── Statistics ───────────────────────────────────────────────────────────────

RateLimiterStats RateLimiter::GetStatistics(ClientId clientId) const
{
    RateLimiterStats stats;

    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    auto it = m_Buckets.find(clientId);
    if (it == m_Buckets.end())
        return stats;

    std::lock_guard<std::mutex> bucketLock(it->second->mutex);

    stats.packetTokens      = it->second->packetTokens;
    stats.byteTokens        = it->second->byteTokens;
    stats.allowedPackets    = it->second->allowedPackets;
    stats.allowedBytes      = it->second->allowedBytes;
    stats.rejectedPackets   = it->second->rejectedPackets;
    stats.rejectedBytes     = it->second->rejectedBytes;
    stats.violations        = it->second->violations;
    stats.quarantined       = it->second->quarantineUntil > clock::time_point{} &&
                              clock::now() < it->second->quarantineUntil;
    stats.lastUpdateUnixMs  = NowUnixMs();
    return stats;
}

std::vector<ClientId> RateLimiter::ListTracked() const
{
    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    std::vector<ClientId> out;
    out.reserve(m_Buckets.size());
    for (const auto& [cid, _] : m_Buckets)
        out.push_back(cid);
    return out;
}

} // namespace SagaEngine::Networking
