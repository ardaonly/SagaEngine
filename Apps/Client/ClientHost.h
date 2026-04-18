/// @file ClientHost.h
/// @brief MMO client application host — orchestrates network, input, and render.
///
/// Layer  : Apps / Client
/// Purpose: Single-file client host.  Network session is defined inline
///          in ClientHost.cpp to avoid stale-header.
///
/// MMO client loop (every frame):
///   1. Recv → Decode → Apply → ECS
///   2. Send Input → Server
///   3. Prediction → Reconciliation → Interpolation
///   4. Render Prep → Render

#pragma once

#include <SagaEngine/Core/Application/Application.h>
#include <SagaEngine/Input/Core/InputManager.h>
#include <memory>
#include <string>
#include <cstdint>

namespace Saga {

// ─── Forward: defined inline in ClientHost.cpp ───────────────────────────

class ClientNetworkSession;
class IDebugRenderer2D;

// ─── Client config ────────────────────────────────────────────────────────────

struct ClientConfig
{
    std::string windowTitle    = "SagaEngine Client";
    std::string serverHost     = "127.0.0.1";
    uint16_t    serverPort     = 7777;
    bool        headless       = false;
    uint32_t    width          = 1280;
    uint32_t    height         = 720;
    bool        vsync          = true;
};

// ─── ClientHost ───────────────────────────────────────────────────────────────

/// MMO client application. One instance per process.
class ClientHost final : public Application
{
public:
    explicit ClientHost(ClientConfig config);
    ~ClientHost() override;

    ClientHost(const ClientHost&)            = delete;
    ClientHost& operator=(const ClientHost&) = delete;

    // ── Application interface ─────────────────────────────────────────────────

    void OnInit()     override;
    void OnUpdate()   override;
    void OnShutdown() override;

    // ── Public API ────────────────────────────────────────────────────────────

    void RequestExit();

    [[nodiscard]] const ClientConfig&    GetConfig()  const noexcept { return m_config; }
    [[nodiscard]] ClientNetworkSession&  GetSession() const noexcept { return *m_session; }

private:
    // ── Input ─────────────────────────────────────────────────────────────────

    /// Poll keyboard state via InputManager and produce input commands.
    void TickInput();

    // ── Debug render ──────────────────────────────────────────────────────────

    /// Draw interpolated entity positions as debug primitives.
    void RenderDebug();

    // ── State ─────────────────────────────────────────────────────────────────

    ClientConfig                              m_config;
    std::unique_ptr<ClientNetworkSession>     m_session;
    uint64_t                                  m_tickCounter = 0;
    uint32_t                                  m_inputSequence = 0;

    // ── Input ────────────────────────────────────────────────────────────────

    SagaEngine::Input::InputManager           m_inputManager;

    // ── Debug renderer (non-headless only) ───────────────────────────────────

    std::unique_ptr<IDebugRenderer2D>         m_debugRenderer;
};

} // namespace Saga
