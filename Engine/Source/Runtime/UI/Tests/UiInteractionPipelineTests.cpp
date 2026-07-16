/// @file UiInteractionPipelineTests.cpp
/// @brief End-to-end smoke tests for UI event to action dispatch.

#include <gtest/gtest.h>

#include "SagaEngine/UI/IUiActionDispatcher.h"
#include "SagaEngine/UI/IUiScreenManager.h"
#include "SagaEngine/UI/RuntimeUiBootstrapper.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

namespace SagaUI = ::SagaEngine::UI;

class PipelineTestBackend final : public SagaUI::IUiBackend
{
public:
    [[nodiscard]] bool Initialize(const SagaUI::UiBackendConfig& config) override
    {
        (void)config;
        initialized = true;
        return true;
    }

    void Shutdown() override
    {
        documents.clear();
        initialized = false;
        nextHandle = 1;
    }

    void SetViewport(SagaUI::UiViewport viewport) override
    {
        (void)viewport;
    }

    [[nodiscard]] SagaUI::UiDocumentHandle LoadDocument(
        const SagaUI::UiDocumentRequest& request) override
    {
        (void)request;
        if (!initialized)
        {
            return SagaUI::UiDocumentHandle::kInvalid;
        }

        const SagaUI::UiDocumentHandle handle =
            static_cast<SagaUI::UiDocumentHandle>(nextHandle++);
        documents.emplace(handle, request.showImmediately);
        return handle;
    }

    [[nodiscard]] bool UnloadDocument(SagaUI::UiDocumentHandle handle) override
    {
        return documents.erase(handle) > 0;
    }

    [[nodiscard]] bool ShowDocument(SagaUI::UiDocumentHandle handle) override
    {
        const auto it = documents.find(handle);
        if (it == documents.end())
        {
            return false;
        }

        it->second = true;
        return true;
    }

    [[nodiscard]] bool HideDocument(SagaUI::UiDocumentHandle handle) override
    {
        const auto it = documents.find(handle);
        if (it == documents.end())
        {
            return false;
        }

        it->second = false;
        return true;
    }

    void Update(double deltaSeconds) override
    {
        (void)deltaSeconds;
    }

    [[nodiscard]] bool Render() override
    {
        return initialized;
    }

    [[nodiscard]] bool SubmitInput(const SagaUI::UiInputEvent& event) override
    {
        (void)event;
        return initialized;
    }

    [[nodiscard]] const std::vector<SagaUI::UiDiagnostic>& Diagnostics()
        const noexcept override
    {
        return diagnostics;
    }

    void ClearDiagnostics() noexcept override
    {
        diagnostics.clear();
    }

    [[nodiscard]] SagaUI::UiRenderStats LastRenderStats()
        const noexcept override
    {
        return {};
    }

    bool initialized = false;
    std::uint32_t nextHandle = 1;
    std::unordered_map<SagaUI::UiDocumentHandle, bool> documents;
    std::vector<SagaUI::UiDiagnostic> diagnostics;
};

[[nodiscard]] std::filesystem::path TestRoot(const char* name)
{
    return std::filesystem::temp_directory_path() /
           "SagaUiInteractionPipelineTests" / name;
}

void WriteConnectScreen(const std::filesystem::path& root)
{
    std::filesystem::create_directories(root / "UI");
    std::ofstream document(root / "UI" / "connect_screen.rml");
    document << "<rml/>";
}

[[nodiscard]] SagaUI::UiRuntimeBootstrapRequest MakeRuntimeRequest(
    std::filesystem::path contentRoot)
{
    SagaUI::UiRuntimeBootstrapRequest request;
    request.backend = std::make_unique<SagaUI::NullUiBackend>();
    request.viewport = SagaUI::UiViewport{640, 360, 1.0f};
    request.contextName = "UiInteractionPipelineTests";
    request.contentRoot = std::move(contentRoot);
    return request;
}

[[nodiscard]] SagaUI::UiEvent MakeConnectClickEvent()
{
    SagaUI::UiEvent event;
    event.screenId = SagaUI::UiScreenId::FromString("connect_screen");
    event.documentId = SagaUI::UiDocumentId::FromContent("connect_screen");
    event.elementId = SagaUI::UiElementId::FromString("connect_button");
    event.type = SagaUI::UiEventType::Click;
    return event;
}

[[nodiscard]] SagaUI::UiActionBinding MakeConnectBinding(
    SagaUI::UiActionType actionType,
    const char* actionId,
    const char* targetScreenId = "")
{
    SagaUI::UiActionBinding binding;
    binding.screenId = SagaUI::UiScreenId::FromString("connect_screen");
    binding.elementId = SagaUI::UiElementId::FromString("connect_button");
    binding.eventType = SagaUI::UiEventType::Click;
    binding.actionId = SagaUI::UiActionId::FromString(actionId);
    binding.actionType = actionType;
    if (targetScreenId[0] != '\0')
    {
        binding.targetScreenId =
            SagaUI::UiScreenId::FromString(targetScreenId);
    }
    return binding;
}

[[nodiscard]] SagaUI::UiScreenDescriptor MakeScreen(const char* screenId)
{
    SagaUI::UiScreenDescriptor descriptor;
    descriptor.screenId = SagaUI::UiScreenId::FromString(screenId);
    descriptor.documentId = SagaUI::UiDocumentId::FromContent(screenId);
    return descriptor;
}

[[nodiscard]] SagaUI::UiScreenSnapshotEntry SnapshotFor(
    const SagaUI::IUiScreenManager& manager,
    const char* screenId)
{
    const SagaUI::UiScreenSnapshot snapshot = manager.Snapshot();
    for (const SagaUI::UiScreenSnapshotEntry& entry : snapshot.screens)
    {
        if (entry.screenId.value == screenId)
        {
            return entry;
        }
    }
    return {};
}

} // namespace

TEST(UiInteractionPipelineTests, QueuedEventDispatchesToNamedActionOnce)
{
    const std::filesystem::path root = TestRoot("NamedActionOnce");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRuntimeRequest(root));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    ASSERT_TRUE(result.context->RegisterActionBinding(MakeConnectBinding(
        SagaUI::UiActionType::EmitNamedAction,
        "connect_to_server")).Succeeded());

    ASSERT_TRUE(result.context->QueueUiEventForTesting(
        MakeConnectClickEvent()));
    const std::vector<SagaUI::UiEvent> queuedEvents =
        result.context->PollEvents();
    ASSERT_EQ(queuedEvents.size(), 1U);
    EXPECT_EQ(queuedEvents.front().sequence, 1U);
    EXPECT_EQ(queuedEvents.front().elementId.value, "connect_button");

    const std::vector<SagaUI::UiActionDispatchResult> dispatchResults =
        result.context->DispatchUiEvents();
    ASSERT_EQ(dispatchResults.size(), 1U);
    EXPECT_EQ(
        dispatchResults.front().code,
        SagaUI::UiActionDiagnosticCode::NamedActionEmitted);
    EXPECT_TRUE(result.context->PollEvents().empty());
    EXPECT_TRUE(result.context->DispatchUiEvents().empty());

    const std::vector<SagaUI::UiNamedAction> namedActions =
        result.context->DrainNamedActions();
    ASSERT_EQ(namedActions.size(), 1U);
    EXPECT_EQ(namedActions.front().actionId.value, "connect_to_server");
    EXPECT_EQ(namedActions.front().sourceEvent.sequence, 1U);
    EXPECT_TRUE(result.context->DrainNamedActions().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiInteractionPipelineTests, MissingBindingConsumesEventWithoutAction)
{
    const std::filesystem::path root = TestRoot("MissingBinding");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRuntimeRequest(root));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    ASSERT_TRUE(result.context->QueueUiEventForTesting(
        MakeConnectClickEvent()));
    const std::vector<SagaUI::UiActionDispatchResult> dispatchResults =
        result.context->DispatchUiEvents();

    ASSERT_EQ(dispatchResults.size(), 1U);
    EXPECT_EQ(
        dispatchResults.front().code,
        SagaUI::UiActionDiagnosticCode::MissingBinding);
    EXPECT_TRUE(result.context->PollEvents().empty());
    EXPECT_TRUE(result.context->DrainNamedActions().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiInteractionPipelineTests, NullRuntimeStartsWithEmptyQueues)
{
    const std::filesystem::path root = TestRoot("EmptyQueues");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRuntimeRequest(root));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    EXPECT_TRUE(result.context->PollEvents().empty());
    EXPECT_TRUE(result.context->DispatchUiEvents().empty());
    EXPECT_TRUE(result.context->DrainEvents().empty());
    EXPECT_TRUE(result.context->DrainNamedActions().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiInteractionPipelineTests, OpenScreenActionUsesScreenManager)
{
    PipelineTestBackend backend;
    ASSERT_TRUE(backend.Initialize({}));

    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);
    ASSERT_TRUE(manager->RegisterScreen(
        MakeScreen("connect_screen")).Succeeded());
    ASSERT_TRUE(manager->RegisterScreen(
        MakeScreen("server_browser")).Succeeded());

    std::unique_ptr<SagaUI::IUiActionDispatcher> dispatcher =
        SagaUI::CreateUiActionDispatcher(*manager);
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeConnectBinding(
        SagaUI::UiActionType::OpenScreen,
        "open_server_browser",
        "server_browser")).Succeeded());

    const std::vector<SagaUI::UiActionDispatchResult> dispatchResults =
        dispatcher->DispatchEvents({MakeConnectClickEvent()});
    ASSERT_EQ(dispatchResults.size(), 1U);
    EXPECT_TRUE(dispatchResults.front().Succeeded());
    EXPECT_EQ(
        dispatchResults.front().code,
        SagaUI::UiActionDiagnosticCode::None);
    EXPECT_TRUE(SnapshotFor(*manager, "server_browser").visible);

    manager->UnloadAll();
    backend.Shutdown();
}
