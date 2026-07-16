/// @file UiActionBindingTests.cpp
/// @brief Focused tests for backend-neutral UI action binding.

#include <gtest/gtest.h>

#include "SagaEngine/UI/IUiActionDispatcher.h"
#include "SagaEngine/UI/IUiScreenManager.h"
#include "SagaEngine/UI/RuntimeUiBootstrapper.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

namespace SagaUI = ::SagaEngine::UI;

class ActionTestBackend final : public SagaUI::IUiBackend
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
        ++showCount;
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
        ++hideCount;
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
    int showCount = 0;
    int hideCount = 0;
    std::unordered_map<SagaUI::UiDocumentHandle, bool> documents;
    std::vector<SagaUI::UiDiagnostic> diagnostics;
};

[[nodiscard]] SagaUI::UiScreenDescriptor MakeScreen(
    const char* screenId,
    bool showOnLoad = false)
{
    SagaUI::UiScreenDescriptor descriptor;
    descriptor.screenId = SagaUI::UiScreenId::FromString(screenId);
    descriptor.documentId = SagaUI::UiDocumentId::FromContent(screenId);
    descriptor.showOnLoad = showOnLoad;
    return descriptor;
}

[[nodiscard]] SagaUI::UiEvent MakeClickEvent()
{
    SagaUI::UiEvent event;
    event.screenId = SagaUI::UiScreenId::FromString("connect_screen");
    event.documentId = SagaUI::UiDocumentId::FromContent("connect_screen");
    event.elementId = SagaUI::UiElementId::FromString("connect_button");
    event.type = SagaUI::UiEventType::Click;
    return event;
}

[[nodiscard]] SagaUI::UiActionBinding MakeBinding(
    SagaUI::UiActionType type,
    const char* actionId,
    const char* targetScreenId = "")
{
    SagaUI::UiActionBinding binding;
    binding.screenId = SagaUI::UiScreenId::FromString("connect_screen");
    binding.elementId = SagaUI::UiElementId::FromString("connect_button");
    binding.eventType = SagaUI::UiEventType::Click;
    binding.actionId = SagaUI::UiActionId::FromString(actionId);
    binding.actionType = type;
    if (targetScreenId[0] != '\0')
    {
        binding.targetScreenId =
            SagaUI::UiScreenId::FromString(targetScreenId);
    }
    return binding;
}

class ActionDispatcherFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(backend.Initialize({}));
        manager = SagaUI::CreateUiScreenManager(backend);
        ASSERT_TRUE(manager->RegisterScreen(MakeScreen("connect_screen")).Succeeded());
        dispatcher = SagaUI::CreateUiActionDispatcher(*manager);
    }

    [[nodiscard]] SagaUI::UiScreenSnapshotEntry SnapshotFor(
        const char* screenId) const
    {
        const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
        for (const SagaUI::UiScreenSnapshotEntry& entry : snapshot.screens)
        {
            if (entry.screenId.value == screenId)
            {
                return entry;
            }
        }
        return {};
    }

    ActionTestBackend backend;
    std::unique_ptr<SagaUI::IUiScreenManager> manager;
    std::unique_ptr<SagaUI::IUiActionDispatcher> dispatcher;
};

[[nodiscard]] std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() /
           "SagaUiActionBindingRuntimeTest";
}

void WriteConnectScreen(const std::filesystem::path& root)
{
    std::filesystem::create_directories(root / "UI");
    std::ofstream(root / "UI" / "connect_screen.rml");
}

} // namespace

TEST_F(ActionDispatcherFixture, RegisterBindingSucceeds)
{
    const SagaUI::UiActionDispatchResult result =
        dispatcher->RegisterBinding(MakeBinding(
            SagaUI::UiActionType::EmitNamedAction,
            "connect_to_server"));

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.code, SagaUI::UiActionDiagnosticCode::None);
}

TEST_F(ActionDispatcherFixture, DuplicateBindingReturnsDiagnostic)
{
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeBinding(
        SagaUI::UiActionType::EmitNamedAction,
        "connect_to_server")).Succeeded());

    const SagaUI::UiActionDispatchResult duplicate =
        dispatcher->RegisterBinding(MakeBinding(
            SagaUI::UiActionType::EmitNamedAction,
            "replacement_action"));

    EXPECT_EQ(duplicate.code, SagaUI::UiActionDiagnosticCode::DuplicateBinding);
    ASSERT_EQ(dispatcher->Diagnostics().size(), 1U);

    ASSERT_TRUE(dispatcher->DispatchEvent(MakeClickEvent()).Succeeded());
    const std::vector<SagaUI::UiNamedAction> namedActions =
        dispatcher->DrainNamedActions();
    ASSERT_EQ(namedActions.size(), 1U);
    EXPECT_EQ(namedActions.front().actionId.value, "connect_to_server");
}

TEST_F(ActionDispatcherFixture, ClickEventResolvesToExpectedNamedAction)
{
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeBinding(
        SagaUI::UiActionType::EmitNamedAction,
        "connect_to_server")).Succeeded());

    const SagaUI::UiActionDispatchResult result =
        dispatcher->DispatchEvent(MakeClickEvent());
    EXPECT_EQ(result.code, SagaUI::UiActionDiagnosticCode::NamedActionEmitted);
    EXPECT_EQ(result.actionId.value, "connect_to_server");

    const std::vector<SagaUI::UiNamedAction> namedActions =
        dispatcher->DrainNamedActions();
    ASSERT_EQ(namedActions.size(), 1U);
    EXPECT_EQ(namedActions.front().actionId.value, "connect_to_server");
    EXPECT_EQ(namedActions.front().sourceEvent.elementId.value, "connect_button");
}

TEST_F(ActionDispatcherFixture, MissingBindingReturnsDeterministicNoOp)
{
    const SagaUI::UiActionDispatchResult result =
        dispatcher->DispatchEvent(MakeClickEvent());

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.code, SagaUI::UiActionDiagnosticCode::MissingBinding);
    EXPECT_TRUE(dispatcher->Diagnostics().empty());
}

TEST_F(ActionDispatcherFixture, OpenScreenShowsTargetScreen)
{
    ASSERT_TRUE(manager->RegisterScreen(MakeScreen("server_browser")).Succeeded());
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeBinding(
        SagaUI::UiActionType::OpenScreen,
        "open_server_browser",
        "server_browser")).Succeeded());

    const SagaUI::UiActionDispatchResult result =
        dispatcher->DispatchEvent(MakeClickEvent());

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.code, SagaUI::UiActionDiagnosticCode::None);
    EXPECT_TRUE(SnapshotFor("server_browser").visible);
}

TEST_F(ActionDispatcherFixture, CloseScreenHidesVisibleTargetScreen)
{
    ASSERT_TRUE(manager->RegisterScreen(MakeScreen("server_browser")).Succeeded());
    ASSERT_TRUE(manager->ShowScreen(
        SagaUI::UiScreenId::FromString("server_browser")).Succeeded());
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeBinding(
        SagaUI::UiActionType::CloseScreen,
        "close_server_browser",
        "server_browser")).Succeeded());

    const SagaUI::UiActionDispatchResult result =
        dispatcher->DispatchEvent(MakeClickEvent());

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.code, SagaUI::UiActionDiagnosticCode::None);
    EXPECT_FALSE(SnapshotFor("server_browser").visible);
}

TEST_F(ActionDispatcherFixture, ToggleScreenTogglesVisibleState)
{
    ASSERT_TRUE(manager->RegisterScreen(MakeScreen("server_browser")).Succeeded());
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeBinding(
        SagaUI::UiActionType::ToggleScreen,
        "toggle_server_browser",
        "server_browser")).Succeeded());

    EXPECT_TRUE(dispatcher->DispatchEvent(MakeClickEvent()).Succeeded());
    EXPECT_TRUE(SnapshotFor("server_browser").visible);

    EXPECT_TRUE(dispatcher->DispatchEvent(MakeClickEvent()).Succeeded());
    EXPECT_FALSE(SnapshotFor("server_browser").visible);
}

TEST_F(ActionDispatcherFixture, InvalidTargetScreenReturnsDiagnostic)
{
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeBinding(
        SagaUI::UiActionType::OpenScreen,
        "open_missing",
        "missing_screen")).Succeeded());

    const SagaUI::UiActionDispatchResult result =
        dispatcher->DispatchEvent(MakeClickEvent());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.code, SagaUI::UiActionDiagnosticCode::InvalidTargetScreen);
    ASSERT_EQ(dispatcher->Diagnostics().size(), 1U);
}

TEST_F(ActionDispatcherFixture, EmitNamedActionRecordsWithoutScriptExecution)
{
    ASSERT_TRUE(dispatcher->RegisterBinding(MakeBinding(
        SagaUI::UiActionType::EmitNamedAction,
        "connect_to_server")).Succeeded());

    ASSERT_TRUE(dispatcher->DispatchEvent(MakeClickEvent()).Succeeded());
    const std::vector<SagaUI::UiNamedAction> namedActions =
        dispatcher->DrainNamedActions();

    ASSERT_EQ(namedActions.size(), 1U);
    EXPECT_EQ(namedActions.front().actionId.value, "connect_to_server");
    EXPECT_TRUE(dispatcher->DrainNamedActions().empty());
}

TEST(UiActionBindingRuntimeTests, RuntimeContextExposesActionDispatcher)
{
    const std::filesystem::path root = TestRoot();
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapRequest request;
    request.backend = std::make_unique<SagaUI::NullUiBackend>();
    request.viewport = SagaUI::UiViewport{640, 360, 1.0f};
    request.contentRoot = root;

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(std::move(request));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    ASSERT_TRUE(result.context->ActionDispatcher().RegisterBinding(MakeBinding(
        SagaUI::UiActionType::EmitNamedAction,
        "connect_to_server")).Succeeded());
    const SagaUI::UiActionDispatchResult dispatchResult =
        result.context->ActionDispatcher().DispatchEvent(MakeClickEvent());
    EXPECT_EQ(
        dispatchResult.code,
        SagaUI::UiActionDiagnosticCode::NamedActionEmitted);

    const std::vector<SagaUI::UiNamedAction> namedActions =
        result.context->DrainNamedActions();
    ASSERT_EQ(namedActions.size(), 1U);
    EXPECT_EQ(namedActions.front().actionId.value, "connect_to_server");

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}
