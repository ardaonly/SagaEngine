/// @file EventLogPanel.cpp
/// @brief Engine log mirror panel implementation.
///
/// Architecture note on the log hook:
///   SagaEngine::Core::Log currently uses a global printf-style Logf() with
///   no external sink registration. The cleanest zero-engine-modification
///   approach here is to keep a global static pointer to this panel and
///   intercept at compile time — but that creates a coupling we don't want.
///
///   Short-term solution used here: PushLine() is public. The Sandbox can
///   override global operator new / set a custom sink in a thin shim that
///   wraps Logf. For now we leave the hook body as a no-op stub and document
///   the correct integration path below.
///
///   Proper integration (requires one engine addition):
///     Add to Log.h:
///       using LogSinkFn = std::function<void(Level, const char*, const char*)>;
///       void SetSink(LogSinkFn fn);
///     Then in EventLogPanel::EventLogPanel():
///       SagaEngine::Core::Log::SetSink(&EventLogPanel::LogHook);

#include "SagaSandbox/UI/Panels/EventLogPanel.h"
#include <imgui.h>
#include <cstdio>

namespace SagaSandbox
{

EventLogPanel* EventLogPanel::s_instance = nullptr;

// ─── Construction ─────────────────────────────────────────────────────────────

EventLogPanel::EventLogPanel()
{
    s_instance = this;
    SagaEngine::Core::Log::SetSink(&EventLogPanel::LogHook);
}

EventLogPanel::~EventLogPanel()
{
    SagaEngine::Core::Log::SetSink(nullptr);
    s_instance = nullptr;
}

// ─── Sink ─────────────────────────────────────────────────────────────────────

void EventLogPanel::PushLine(SagaEngine::Core::Log::Level level, std::string line)
{
    std::lock_guard lock(m_mutex);
    if (static_cast<int>(m_lines.size()) >= kMaxLines)
        m_lines.pop_front();
    m_lines.push_back({level, std::move(line)});
    m_scrollToEnd = m_autoScroll;
}

void EventLogPanel::LogHook(SagaEngine::Core::Log::Level level,
                             const char* tag, const char* message)
{
    if (!s_instance) return;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "[%s] %s", tag ? tag : "", message ? message : "");
    s_instance->PushLine(level, std::string(buf));
}

// ─── Render ───────────────────────────────────────────────────────────────────

void EventLogPanel::Render(float /*dt*/, uint64_t /*tick*/)
{
    if (!ImGui::Begin(GetTitle().data(), &m_open))
    {
        ImGui::End();
        return;
    }

    // ─── Controls ─────────────────────────────────────────────────────────────

    if (ImGui::Button("Clear"))
    {
        std::lock_guard lock(m_mutex);
        m_lines.clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);

    // Level filter combo
    static const char* kLevelNames[] =
        { "Trace", "Debug", "Info", "Warn", "Error", "Critical" };

    int filterIdx = static_cast<int>(m_filterLevel);
    if (ImGui::Combo("Min Level", &filterIdx, kLevelNames, 6))
        m_filterLevel = static_cast<SagaEngine::Core::Log::Level>(filterIdx);

    ImGui::Separator();

    // ─── Log lines ────────────────────────────────────────────────────────────

    ImGui::BeginChild("##log_scroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard lock(m_mutex);
        for (const auto& line : m_lines)
        {
            if (line.level < m_filterLevel) continue;

            ImVec4 col;
            switch (line.level)
            {
            using L = SagaEngine::Core::Log::Level;
            case L::Trace:    col = ImVec4(0.5f, 0.5f, 0.5f, 1.f); break;
            case L::Debug:    col = ImVec4(0.7f, 0.7f, 1.0f, 1.f); break;
            case L::Info:     col = ImVec4(1.0f, 1.0f, 1.0f, 1.f); break;
            case L::Warn:     col = ImVec4(1.0f, 0.8f, 0.0f, 1.f); break;
            case L::Error:    col = ImVec4(1.0f, 0.3f, 0.3f, 1.f); break;
            case L::Critical: col = ImVec4(1.0f, 0.0f, 0.5f, 1.f); break;
            default:          col = ImVec4(1.0f, 1.0f, 1.0f, 1.f); break;
            }

            ImGui::TextColored(col, "%s", line.text.c_str());
        }
    }

    if (m_scrollToEnd)
    {
        ImGui::SetScrollHereY(1.f);
        m_scrollToEnd = false;
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace SagaSandbox