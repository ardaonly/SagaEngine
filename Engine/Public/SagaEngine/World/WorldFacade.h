/// @file WorldFacade.h
/// @brief Transitional public shell for high-level world lifecycle access.

#pragma once

#include <cstdint>

namespace SagaEngine::World {

struct WorldFacadeConfig
{
    std::uint32_t tickRateHz = 60;
    bool          headless   = true;
};

struct WorldFacadeTickOptions
{
    double        deltaSeconds = 0.0;
    std::uint64_t tickHint     = 0;
};

struct WorldFacadeTickResult
{
    bool          initialized  = false;
    bool          accepted     = false;
    std::uint64_t tickIndex    = 0;
    double        deltaSeconds = 0.0;
};

struct WorldFacadeStatus
{
    bool          initialized = false;
    std::uint64_t tickIndex   = 0;
};

struct WorldFacadeStats
{
    std::uint64_t ticksSubmitted    = 0;
    std::uint64_t commandsSubmitted = 0;
};

enum class WorldFacadeCommandKind : std::uint8_t
{
    Noop,
};

struct WorldFacadeCommand
{
    WorldFacadeCommandKind kind      = WorldFacadeCommandKind::Noop;
    std::uint64_t          subjectId = 0;
    std::uint64_t          value     = 0;
};

class WorldFacade
{
public:
    WorldFacade() = default;
    ~WorldFacade() = default;

    WorldFacade(const WorldFacade&)            = delete;
    WorldFacade& operator=(const WorldFacade&) = delete;

    [[nodiscard]] bool Initialize(const WorldFacadeConfig& config = {}) noexcept
    {
        m_config = config;
        m_status = {};
        m_stats = {};
        m_initialized = true;
        m_status.initialized = true;
        return true;
    }

    void Shutdown() noexcept
    {
        m_initialized = false;
        m_status.initialized = false;
    }

    [[nodiscard]] WorldFacadeTickResult Tick(
        const WorldFacadeTickOptions& options = {}) noexcept
    {
        WorldFacadeTickResult result;
        result.initialized = m_initialized;
        result.deltaSeconds = options.deltaSeconds;

        if (!m_initialized)
        {
            return result;
        }

        ++m_status.tickIndex;
        ++m_stats.ticksSubmitted;

        result.accepted = true;
        result.tickIndex = options.tickHint != 0 ? options.tickHint : m_status.tickIndex;
        return result;
    }

    [[nodiscard]] WorldFacadeStatus GetStatus() const noexcept
    {
        return m_status;
    }

    [[nodiscard]] WorldFacadeStats GetStats() const noexcept
    {
        return m_stats;
    }

    [[nodiscard]] bool SubmitCommand(const WorldFacadeCommand& command) noexcept
    {
        if (!m_initialized)
        {
            return false;
        }

        ++m_stats.commandsSubmitted;
        return command.kind == WorldFacadeCommandKind::Noop;
    }

private:
    WorldFacadeConfig m_config{};
    WorldFacadeStatus m_status{};
    WorldFacadeStats  m_stats{};
    bool              m_initialized = false;
};

} // namespace SagaEngine::World
