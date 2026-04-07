/// @file EventLogPanel.h
/// @brief HUD panel that mirrors the engine log in real time.
///
/// Layer  : Sandbox / UI / Panels
/// Purpose: Captures log output from SagaEngine::Core::Log via a custom
///          sink and displays the last N lines with level-based colouring.
///          No engine modification needed — we intercept at the macro level
///          by registering a callback sink during Init().

#pragma once

#include "IDebugPanel.h"
#include <deque>
#include <mutex>
#include <string>
#include <SagaEngine/Core/Log/Log.h>

namespace SagaSandbox
{

class EventLogPanel final : public IDebugPanel
{
public:
    EventLogPanel();
    ~EventLogPanel() override;

    [[nodiscard]] std::string_view GetTitle() const noexcept override
    {
        return "Engine Log";
    }

    void Render(float dt, uint64_t tick) override;

    // ── Sink API (called from log hook) ───────────────────────────────────────

    void PushLine(SagaEngine::Core::Log::Level level, std::string line);

private:
    static constexpr int kMaxLines = 500;

    struct LogLine
    {
        SagaEngine::Core::Log::Level level;
        std::string                  text;
    };

    std::deque<LogLine> m_lines;
    mutable std::mutex  m_mutex;

    bool m_autoScroll   = true;
    bool m_scrollToEnd  = false;

    SagaEngine::Core::Log::Level m_filterLevel = SagaEngine::Core::Log::Level::Trace;

    // Unique instance pointer for the static log hook
    static EventLogPanel* s_instance;
    static void LogHook(SagaEngine::Core::Log::Level level,
                        const char* tag, const char* message);
};

} // namespace SagaSandbox