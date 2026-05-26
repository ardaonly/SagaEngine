/// @file RuntimeUiBootstrapper.cpp
/// @brief Backend-neutral runtime UI bootstrap implementation.

#include "SagaEngine/UI/RuntimeUiBootstrapper.h"

#include <utility>

namespace SagaEngine::UI
{
namespace
{

void AddDiagnostic(
    std::vector<UiRuntimeBootstrapDiagnostic>& diagnostics,
    UiRuntimeBootstrapDiagnosticCode code,
    UiDiagnosticSeverity severity,
    std::string message,
    std::filesystem::path path = {},
    UiScreenId screenId = {},
    UiDocumentId documentId = {})
{
    diagnostics.push_back(UiRuntimeBootstrapDiagnostic{
        code,
        severity,
        std::move(message),
        std::move(path),
        std::move(screenId),
        std::move(documentId),
    });
}

void AddScreenDiagnostic(
    std::vector<UiRuntimeBootstrapDiagnostic>& diagnostics,
    UiRuntimeBootstrapDiagnosticCode code,
    const UiScreenOperationResult& screenResult)
{
    AddDiagnostic(
        diagnostics,
        code,
        screenResult.severity,
        screenResult.message,
        {},
        screenResult.screenId,
        screenResult.documentId);
}

[[nodiscard]] std::shared_ptr<IUiResourceProvider> MakeSharedProvider()
{
    std::unique_ptr<IUiResourceProvider> provider =
        CreateDefaultUiResourceProvider();
    return std::shared_ptr<IUiResourceProvider>(std::move(provider));
}

} // namespace

struct UiRuntimeContext::Impl
{
    std::unique_ptr<IUiBackend> backend;
    std::shared_ptr<IUiResourceProvider> resourceProvider;
    std::unique_ptr<IUiScreenManager> screenManager;
    std::unique_ptr<IUiInputRouter> inputRouter;
    std::unique_ptr<IUiActionDispatcher> actionDispatcher;
    std::unique_ptr<IUiNamedActionHandlerRegistry> namedActionHandlers;
    std::shared_ptr<IUiEventQueue> eventQueue;
    std::vector<UiRuntimeBootstrapDiagnostic> diagnostics;
    bool shutdown = false;
};

UiRuntimeContext::UiRuntimeContext(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

UiRuntimeContext::~UiRuntimeContext()
{
    Shutdown();
}

UiRuntimeContext::UiRuntimeContext(UiRuntimeContext&&) noexcept = default;

UiRuntimeContext& UiRuntimeContext::operator=(
    UiRuntimeContext&&) noexcept = default;

void UiRuntimeContext::Update(double deltaSeconds)
{
    if (impl_ && impl_->backend && !impl_->shutdown)
    {
        impl_->backend->Update(deltaSeconds);
    }
}

bool UiRuntimeContext::Render()
{
    if (!impl_ || !impl_->backend || impl_->shutdown)
    {
        return false;
    }

    return impl_->backend->Render();
}

void UiRuntimeContext::Shutdown()
{
    if (!impl_ || impl_->shutdown)
    {
        return;
    }

    if (impl_->screenManager)
    {
        impl_->screenManager->UnloadAll();
        impl_->namedActionHandlers.reset();
        impl_->actionDispatcher.reset();
        impl_->screenManager.reset();
    }

    if (impl_->backend)
    {
        impl_->backend->Shutdown();
        impl_->backend.reset();
    }

    impl_->resourceProvider.reset();
    impl_->inputRouter.reset();
    impl_->eventQueue.reset();
    impl_->shutdown = true;
}

UiScreenSnapshot UiRuntimeContext::Snapshot() const
{
    if (!impl_ || !impl_->screenManager)
    {
        return {};
    }

    return impl_->screenManager->Snapshot();
}

IUiInputRouter& UiRuntimeContext::InputRouter() noexcept
{
    return *impl_->inputRouter;
}

const IUiInputRouter& UiRuntimeContext::InputRouter() const noexcept
{
    return *impl_->inputRouter;
}

IUiActionDispatcher& UiRuntimeContext::ActionDispatcher() noexcept
{
    return *impl_->actionDispatcher;
}

const IUiActionDispatcher& UiRuntimeContext::ActionDispatcher() const noexcept
{
    return *impl_->actionDispatcher;
}

IUiNamedActionHandlerRegistry&
UiRuntimeContext::NamedActionHandlerRegistry() noexcept
{
    return *impl_->namedActionHandlers;
}

const IUiNamedActionHandlerRegistry&
UiRuntimeContext::NamedActionHandlerRegistry() const noexcept
{
    return *impl_->namedActionHandlers;
}

UiActionDispatchResult UiRuntimeContext::RegisterActionBinding(
    UiActionBinding binding)
{
    if (!impl_ || !impl_->actionDispatcher)
    {
        UiActionDispatchResult result;
        result.code = UiActionDiagnosticCode::InvalidBinding;
        result.severity = UiDiagnosticSeverity::Error;
        result.message = "Runtime UI action dispatcher is unavailable.";
        result.actionId = std::move(binding.actionId);
        result.actionType = binding.actionType;
        result.targetScreenId = std::move(binding.targetScreenId);
        return result;
    }

    return impl_->actionDispatcher->RegisterBinding(std::move(binding));
}

std::vector<UiActionDispatchResult> UiRuntimeContext::RegisterActionMap(
    const UiActionMap& actionMap)
{
    if (!impl_ || !impl_->actionDispatcher)
    {
        return {};
    }

    return impl_->actionDispatcher->RegisterActionMap(actionMap);
}

bool UiRuntimeContext::QueueUiEventForTesting(UiEvent event)
{
    if (!impl_ || !impl_->eventQueue)
    {
        return false;
    }

    return impl_->eventQueue->PushEvent(std::move(event));
}

std::vector<UiEvent> UiRuntimeContext::PollEvents() const
{
    if (!impl_ || !impl_->eventQueue)
    {
        return {};
    }

    return impl_->eventQueue->PollEvents();
}

std::vector<UiEvent> UiRuntimeContext::DrainEvents()
{
    if (!impl_ || !impl_->eventQueue)
    {
        return {};
    }

    return impl_->eventQueue->DrainEvents();
}

std::vector<UiActionDispatchResult> UiRuntimeContext::DispatchUiEvents()
{
    if (!impl_ || !impl_->eventQueue || !impl_->actionDispatcher)
    {
        return {};
    }

    return impl_->actionDispatcher->DispatchEvents(
        impl_->eventQueue->DrainEvents());
}

std::vector<UiNamedAction> UiRuntimeContext::DrainNamedActions()
{
    if (!impl_ || !impl_->actionDispatcher)
    {
        return {};
    }

    return impl_->actionDispatcher->DrainNamedActions();
}

UiNamedActionHandlerResult UiRuntimeContext::RegisterNamedActionHandler(
    UiActionId actionId,
    std::shared_ptr<IUiNamedActionHandler> handler)
{
    if (!impl_ || !impl_->namedActionHandlers)
    {
        UiNamedActionHandlerResult result;
        result.code = UiNamedActionHandlerDiagnosticCode::InvalidAction;
        result.severity = UiDiagnosticSeverity::Error;
        result.message =
            "Runtime UI named action handler registry is unavailable.";
        result.context.actionId = std::move(actionId);
        return result;
    }

    return impl_->namedActionHandlers->RegisterHandler(
        std::move(actionId),
        std::move(handler));
}

std::vector<UiNamedActionHandlerResult>
UiRuntimeContext::DispatchNamedActions()
{
    if (!impl_ || !impl_->actionDispatcher || !impl_->namedActionHandlers)
    {
        return {};
    }

    return impl_->namedActionHandlers->DispatchNamedActions(
        impl_->actionDispatcher->DrainNamedActions());
}

const std::vector<UiActionDiagnostic>&
UiRuntimeContext::ActionDiagnostics() const noexcept
{
    static const std::vector<UiActionDiagnostic> kEmptyDiagnostics;
    if (!impl_ || !impl_->actionDispatcher)
    {
        return kEmptyDiagnostics;
    }

    return impl_->actionDispatcher->Diagnostics();
}

void UiRuntimeContext::ClearActionDiagnostics() noexcept
{
    if (impl_ && impl_->actionDispatcher)
    {
        impl_->actionDispatcher->ClearDiagnostics();
    }
}

const std::vector<UiNamedActionHandlerDiagnostic>&
UiRuntimeContext::NamedActionHandlerDiagnostics() const noexcept
{
    static const std::vector<UiNamedActionHandlerDiagnostic> kEmptyDiagnostics;
    if (!impl_ || !impl_->namedActionHandlers)
    {
        return kEmptyDiagnostics;
    }

    return impl_->namedActionHandlers->Diagnostics();
}

void UiRuntimeContext::ClearNamedActionHandlerDiagnostics() noexcept
{
    if (impl_ && impl_->namedActionHandlers)
    {
        impl_->namedActionHandlers->ClearDiagnostics();
    }
}

const std::vector<UiEventDiagnostic>&
UiRuntimeContext::EventDiagnostics() const noexcept
{
    static const std::vector<UiEventDiagnostic> kEmptyDiagnostics;
    if (!impl_ || !impl_->eventQueue)
    {
        return kEmptyDiagnostics;
    }

    return impl_->eventQueue->Diagnostics();
}

void UiRuntimeContext::ClearEventDiagnostics() noexcept
{
    if (impl_ && impl_->eventQueue)
    {
        impl_->eventQueue->ClearDiagnostics();
    }
}

const std::vector<UiRuntimeBootstrapDiagnostic>&
UiRuntimeContext::Diagnostics() const noexcept
{
    return impl_->diagnostics;
}

UiRuntimeBootstrapResult BootstrapRuntimeUi(
    UiRuntimeBootstrapRequest request)
{
    UiRuntimeBootstrapResult result;
    if (!request.enabled)
    {
        result.disabled = true;
        AddDiagnostic(
            result.diagnostics,
            UiRuntimeBootstrapDiagnosticCode::Disabled,
            UiDiagnosticSeverity::Info,
            "Runtime UI bootstrap is disabled.");
        return result;
    }

    if (!request.backend)
    {
        AddDiagnostic(
            result.diagnostics,
            UiRuntimeBootstrapDiagnosticCode::MissingBackend,
            UiDiagnosticSeverity::Error,
            "Runtime UI bootstrap requires a UI backend.");
        return result;
    }

    std::unique_ptr<UiRuntimeContext::Impl> impl =
        std::make_unique<UiRuntimeContext::Impl>();
    impl->backend = std::move(request.backend);
    impl->resourceProvider = MakeSharedProvider();
    impl->inputRouter = CreateUiInputRouter();
    impl->eventQueue = CreateUiEventQueue();
    impl->namedActionHandlers = CreateUiNamedActionHandlerRegistry();

    if (!request.contentRoot.empty())
    {
        const UiResourceMountResult mountResult =
            impl->resourceProvider->MountContentRoot(request.contentRoot);
        if (!mountResult.Succeeded())
        {
            AddDiagnostic(
                result.diagnostics,
                UiRuntimeBootstrapDiagnosticCode::ContentMountFailed,
                UiDiagnosticSeverity::Error,
                mountResult.diagnostic,
                request.contentRoot,
                request.startupScreenId,
                request.startupDocumentId);
            return result;
        }
    }

    for (const UiRuntimePackageMount& mount : request.packageMounts)
    {
        const UiResourceMountResult mountResult =
            impl->resourceProvider->MountPackageRoot(mount.packageId, mount.root);
        if (!mountResult.Succeeded())
        {
            AddDiagnostic(
                result.diagnostics,
                UiRuntimeBootstrapDiagnosticCode::PackageMountFailed,
                UiDiagnosticSeverity::Error,
                mountResult.diagnostic,
                mount.root,
                request.startupScreenId,
                request.startupDocumentId);
            return result;
        }
    }

    UiBackendConfig backendConfig;
    backendConfig.viewport = request.viewport;
    backendConfig.assetRoot =
        request.contentRoot.empty()
            ? std::filesystem::path{}
            : request.contentRoot / "UI";
    backendConfig.resourceProvider = impl->resourceProvider;
    backendConfig.eventSink = impl->eventQueue;
    backendConfig.contextName = request.contextName;

    if (!impl->backend->Initialize(backendConfig))
    {
        AddDiagnostic(
            result.diagnostics,
            UiRuntimeBootstrapDiagnosticCode::BackendInitializeFailed,
            UiDiagnosticSeverity::Error,
            "Runtime UI backend failed to initialize.",
            {},
            request.startupScreenId,
            request.startupDocumentId);
        return result;
    }

    impl->screenManager = CreateUiScreenManager(*impl->backend);
    impl->actionDispatcher = CreateUiActionDispatcher(*impl->screenManager);

    UiScreenDescriptor startupScreen;
    startupScreen.screenId = request.startupScreenId;
    startupScreen.documentId = request.startupDocumentId;
    startupScreen.loadPolicy = UiScreenLoadPolicy::Manual;
    startupScreen.showOnLoad = false;

    const UiScreenOperationResult registerResult =
        impl->screenManager->RegisterScreen(std::move(startupScreen));
    if (!registerResult.Succeeded())
    {
        AddScreenDiagnostic(
            result.diagnostics,
            UiRuntimeBootstrapDiagnosticCode::ScreenRegisterFailed,
            registerResult);
        impl->backend->Shutdown();
        return result;
    }

    const UiScreenOperationResult startupResult =
        request.showStartupScreen
            ? impl->screenManager->ShowScreen(request.startupScreenId)
            : impl->screenManager->LoadScreen(request.startupScreenId);
    if (!startupResult.Succeeded())
    {
        AddScreenDiagnostic(
            result.diagnostics,
            UiRuntimeBootstrapDiagnosticCode::ScreenShowFailed,
            startupResult);
        impl->screenManager->UnloadAll();
        impl->backend->Shutdown();
        return result;
    }

    impl->diagnostics = result.diagnostics;
    result.context = std::unique_ptr<UiRuntimeContext>(
        new UiRuntimeContext(std::move(impl)));
    return result;
}

} // namespace SagaEngine::UI
