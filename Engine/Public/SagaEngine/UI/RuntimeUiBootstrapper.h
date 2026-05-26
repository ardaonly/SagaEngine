/// @file RuntimeUiBootstrapper.h
/// @brief Backend-neutral runtime UI bootstrap and frame pump.

#pragma once

#include "SagaEngine/UI/IUiActionDispatcher.h"
#include "SagaEngine/UI/IUiBackend.h"
#include "SagaEngine/UI/IUiEventQueue.h"
#include "SagaEngine/UI/IUiInputRouter.h"
#include "SagaEngine/UI/IUiNamedActionHandler.h"
#include "SagaEngine/UI/IUiResourceProvider.h"
#include "SagaEngine/UI/IUiScreenManager.h"
#include "SagaEngine/UI/UiResourceIds.h"
#include "SagaEngine/UI/UiTypes.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::UI
{

struct UiRuntimeBootstrapResult;

/// Package UI mount supplied by runtime/product composition.
struct UiRuntimePackageMount
{
    std::string packageId;
    std::filesystem::path root;
};

/// Runtime UI bootstrap diagnostic codes.
enum class UiRuntimeBootstrapDiagnosticCode : std::uint8_t
{
    None = 0,
    Disabled,
    MissingBackend,
    ContentMountFailed,
    PackageMountFailed,
    BackendInitializeFailed,
    ScreenRegisterFailed,
    ScreenShowFailed,
};

/// One runtime UI bootstrap diagnostic.
struct UiRuntimeBootstrapDiagnostic
{
    UiRuntimeBootstrapDiagnosticCode code =
        UiRuntimeBootstrapDiagnosticCode::None;
    UiDiagnosticSeverity severity = UiDiagnosticSeverity::Info;
    std::string message;
    std::filesystem::path path;
    UiScreenId screenId;
    UiDocumentId documentId;
};

/// Runtime UI startup request.
struct UiRuntimeBootstrapRequest
{
    bool enabled = true;
    std::unique_ptr<IUiBackend> backend;
    UiViewport viewport;
    std::string contextName = "SagaRuntimeUi";
    std::filesystem::path contentRoot;
    std::vector<UiRuntimePackageMount> packageMounts;
    UiScreenId startupScreenId =
        UiScreenId::FromString("connect_screen");
    UiDocumentId startupDocumentId =
        UiDocumentId::FromContent("connect_screen");
    bool showStartupScreen = true;
};

/// Owns runtime UI backend, resource provider, and screen manager.
class UiRuntimeContext final
{
public:
    ~UiRuntimeContext();

    UiRuntimeContext(UiRuntimeContext&&) noexcept;
    UiRuntimeContext& operator=(UiRuntimeContext&&) noexcept;

    UiRuntimeContext(const UiRuntimeContext&) = delete;
    UiRuntimeContext& operator=(const UiRuntimeContext&) = delete;

    /// Advance UI animations, layout, and backend state.
    void Update(double deltaSeconds);

    /// Render the active runtime UI context.
    [[nodiscard]] bool Render();

    /// Release screen and backend state. Safe to call multiple times.
    void Shutdown();

    /// Return the screen manager snapshot.
    [[nodiscard]] UiScreenSnapshot Snapshot() const;

    /// Return the runtime UI input router.
    [[nodiscard]] IUiInputRouter& InputRouter() noexcept;

    /// Return the runtime UI input router.
    [[nodiscard]] const IUiInputRouter& InputRouter() const noexcept;

    /// Return the runtime UI action dispatcher.
    [[nodiscard]] IUiActionDispatcher& ActionDispatcher() noexcept;

    /// Return the runtime UI action dispatcher.
    [[nodiscard]] const IUiActionDispatcher& ActionDispatcher() const noexcept;

    /// Return the runtime UI named action handler registry.
    [[nodiscard]] IUiNamedActionHandlerRegistry&
    NamedActionHandlerRegistry() noexcept;

    /// Return the runtime UI named action handler registry.
    [[nodiscard]] const IUiNamedActionHandlerRegistry&
    NamedActionHandlerRegistry() const noexcept;

    /// Register one runtime UI action binding.
    [[nodiscard]] UiActionDispatchResult RegisterActionBinding(
        UiActionBinding binding);

    /// Register a runtime UI action map.
    [[nodiscard]] std::vector<UiActionDispatchResult> RegisterActionMap(
        const UiActionMap& actionMap);

    /// Queue one backend-neutral UI event for deterministic tests or demos.
    [[nodiscard]] bool QueueUiEventForTesting(UiEvent event);

    /// Return queued UI events without clearing them.
    [[nodiscard]] std::vector<UiEvent> PollEvents() const;

    /// Return queued UI events and clear the queue.
    [[nodiscard]] std::vector<UiEvent> DrainEvents();

    /// Drain queued UI events and dispatch them through action bindings.
    [[nodiscard]] std::vector<UiActionDispatchResult> DispatchUiEvents();

    /// Return and clear named actions emitted by action dispatch.
    [[nodiscard]] std::vector<UiNamedAction> DrainNamedActions();

    /// Register one runtime UI named action handler.
    [[nodiscard]] UiNamedActionHandlerResult RegisterNamedActionHandler(
        UiActionId actionId,
        std::shared_ptr<IUiNamedActionHandler> handler);

    /// Drain emitted named actions and invoke registered handlers.
    [[nodiscard]] std::vector<UiNamedActionHandlerResult>
    DispatchNamedActions();

    /// Return UI action diagnostics.
    [[nodiscard]] const std::vector<UiActionDiagnostic>& ActionDiagnostics()
        const noexcept;

    /// Clear UI action diagnostics.
    void ClearActionDiagnostics() noexcept;

    /// Return UI named action handler diagnostics.
    [[nodiscard]] const std::vector<UiNamedActionHandlerDiagnostic>&
    NamedActionHandlerDiagnostics() const noexcept;

    /// Clear UI named action handler diagnostics.
    void ClearNamedActionHandlerDiagnostics() noexcept;

    /// Return UI event queue diagnostics.
    [[nodiscard]] const std::vector<UiEventDiagnostic>& EventDiagnostics()
        const noexcept;

    /// Clear UI event queue diagnostics.
    void ClearEventDiagnostics() noexcept;

    /// Return bootstrap diagnostics retained by this context.
    [[nodiscard]] const std::vector<UiRuntimeBootstrapDiagnostic>&
    Diagnostics() const noexcept;

private:
    struct Impl;

    explicit UiRuntimeContext(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;

    friend UiRuntimeBootstrapResult BootstrapRuntimeUi(
        UiRuntimeBootstrapRequest request);
};

/// Runtime UI bootstrap result.
struct UiRuntimeBootstrapResult
{
    std::unique_ptr<UiRuntimeContext> context;
    std::vector<UiRuntimeBootstrapDiagnostic> diagnostics;
    bool disabled = false;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return disabled || context != nullptr;
    }
};

/// Build and start a runtime UI context.
[[nodiscard]] UiRuntimeBootstrapResult BootstrapRuntimeUi(
    UiRuntimeBootstrapRequest request);

} // namespace SagaEngine::UI
