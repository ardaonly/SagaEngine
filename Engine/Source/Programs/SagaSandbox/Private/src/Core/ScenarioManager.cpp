/// @file ScenarioManager.cpp
/// @brief Scenario lifecycle management: registration, activation, hot-swap.

#include "SagaSandbox/Core/ScenarioManager.h"
#include <SagaEngine/Core/Log/Log.h>
#include <cassert>

namespace SagaSandbox
{

static constexpr const char* kTag = "ScenarioManager";

// ─── Destructor ───────────────────────────────────────────────────────────────

ScenarioManager::~ScenarioManager()
{
    if (m_activeScenario)
    {
        DeactivateCurrentScenario();
    }
}

// ─── Registration ─────────────────────────────────────────────────────────────

void ScenarioManager::RegisterScenario(std::string_view id, ScenarioFactory factory)
{
    assert(!id.empty() && "Scenario ID must not be empty");
    assert(factory     && "Scenario factory must be valid");

    std::string key{id};
    if (m_registry.count(key))
    {
        LOG_WARN(kTag, "Scenario '%s' already registered — skipping duplicate.", key.c_str());
        return;
    }

    // Probe the descriptor from a temporary instance.
    auto probe    = factory();
    auto& entry   = m_registry[key];
    entry.id             = key;
    entry.factory        = std::move(factory);
    entry.cachedDescriptor = probe->GetDescriptor();

    LOG_INFO(kTag, "Registered scenario '%s' (%s).", key.c_str(), entry.cachedDescriptor.category.data());
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool ScenarioManager::Init(std::string_view initialScenarioId)
{
    m_initialized = true;

    if (!initialScenarioId.empty())
    {
        return ActivateScenario(initialScenarioId);
    }

    LOG_INFO(kTag, "No initial scenario — waiting for user selection.");
    return true;
}

void ScenarioManager::Update(float deltaTime, uint64_t tick)
{
    if (!m_initialized) return;

    // ── Deferred switch ───────────────────────────────────────────────────────
    if (m_stopRequested)
    {
        m_stopRequested = false;
        DeactivateCurrentScenario();
    }
    else if (!m_pendingSwitchId.empty())
    {
        std::string targetId = std::move(m_pendingSwitchId);
        m_pendingSwitchId.clear();
        ActivateScenario(targetId);
    }

    // ── Tick active scenario ──────────────────────────────────────────────────
    if (m_activeScenario)
    {
        m_activeScenario->OnUpdate(deltaTime, tick);
    }
}

void ScenarioManager::RenderDebugUI()
{
    if (m_activeScenario)
    {
        m_activeScenario->OnRenderDebugUI();
    }
}

void ScenarioManager::PrepareRender(
    ::SagaEngine::Render::Scene::Camera&     outCam,
    ::SagaEngine::Render::Scene::RenderView& outView)
{
    if (m_activeScenario)
    {
        m_activeScenario->OnPrepareRender(outCam, outView);
    }
}

void ScenarioManager::Shutdown()
{
    DeactivateCurrentScenario();
    m_registry.clear();
    m_initialized = false;
}

// ─── Scenario Selection ───────────────────────────────────────────────────────

void ScenarioManager::RequestSwitch(std::string_view id)
{
    m_pendingSwitchId = std::string{id};
    LOG_DEBUG(kTag, "Switch to '%s' scheduled for next tick boundary.", m_pendingSwitchId.c_str());
}

void ScenarioManager::RequestStop()
{
    m_stopRequested = true;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool ScenarioManager::HasActiveScenario() const noexcept
{
    return m_activeScenario != nullptr;
}

std::string ScenarioManager::GetActiveScenarioId() const noexcept
{
    return m_activeScenarioId;
}

bool ScenarioManager::IsSwitchPending() const noexcept
{
    return !m_pendingSwitchId.empty() || m_stopRequested;
}

std::vector<const ScenarioDescriptor*> ScenarioManager::GetAllDescriptors() const
{
    std::vector<const ScenarioDescriptor*> out;
    out.reserve(m_registry.size());
    for (const auto& [key, entry] : m_registry)
    {
        out.push_back(&entry.cachedDescriptor);
    }
    return out;
}

// ─── Private ──────────────────────────────────────────────────────────────────

bool ScenarioManager::ActivateScenario(std::string_view id)
{
    auto it = m_registry.find(std::string{id});
    if (it == m_registry.end())
    {
        LOG_ERROR(kTag, "Unknown scenario '%.*s' — cannot activate.", (int)id.size(), id.data());
        return false;
    }

    DeactivateCurrentScenario();

    LOG_INFO(kTag, "Activating scenario '%.*s'…", (int)id.size(), id.data());

    m_activeScenario   = it->second.factory();
    m_activeScenarioId = it->first;

    // Give the scenario access to host services before OnInit.
    m_activeScenario->OnAttach(m_context);

    if (!m_activeScenario->OnInit())
    {
        LOG_ERROR(kTag, "Scenario '%s' failed OnInit — aborting.", m_activeScenarioId.c_str());
        m_activeScenario.reset();
        m_activeScenarioId.clear();
        return false;
    }

    LOG_INFO(kTag, "Scenario '%s' is now active.", m_activeScenarioId.c_str());
    return true;
}

void ScenarioManager::DeactivateCurrentScenario()
{
    if (!m_activeScenario) return;

    LOG_INFO(kTag, "Shutting down scenario '%s'…", m_activeScenarioId.c_str());
    m_activeScenario->OnShutdown();
    m_activeScenario.reset();
    m_activeScenarioId.clear();
    LOG_INFO(kTag, "Scenario shutdown complete.");
}

} // namespace SagaSandbox