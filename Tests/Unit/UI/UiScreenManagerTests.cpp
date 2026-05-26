/// @file UiScreenManagerTests.cpp
/// @brief Focused tests for backend-neutral runtime UI screen management.

#include <gtest/gtest.h>

#include "SagaEngine/UI/IUiScreenManager.h"

#include <memory>
#include <unordered_map>

namespace
{

namespace SagaUI = ::SagaEngine::UI;

class CountingUiBackend final : public SagaUI::IUiBackend
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
    }

    void SetViewport(SagaUI::UiViewport viewport) override
    {
        (void)viewport;
    }

    [[nodiscard]] SagaUI::UiDocumentHandle LoadDocument(
        const SagaUI::UiDocumentRequest& request) override
    {
        ++loadCount;
        lastRequest = request;
        if (!initialized || failLoad)
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
        ++unloadCount;
        return documents.erase(handle) > 0;
    }

    [[nodiscard]] bool ShowDocument(SagaUI::UiDocumentHandle handle) override
    {
        ++showCount;
        const auto it = documents.find(handle);
        if (it == documents.end() || failShow)
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
        if (it == documents.end() || failHide)
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
    bool failLoad = false;
    bool failShow = false;
    bool failHide = false;
    std::uint32_t nextHandle = 1;
    int loadCount = 0;
    int showCount = 0;
    int hideCount = 0;
    int unloadCount = 0;
    SagaUI::UiDocumentRequest lastRequest;
    std::unordered_map<SagaUI::UiDocumentHandle, bool> documents;
    std::vector<SagaUI::UiDiagnostic> diagnostics;
};

[[nodiscard]] SagaUI::UiScreenDescriptor MakeDescriptor(
    const char* screenId = "connect_screen")
{
    SagaUI::UiScreenDescriptor descriptor;
    descriptor.screenId = SagaUI::UiScreenId::FromString(screenId);
    descriptor.documentId =
        SagaUI::UiDocumentId::FromContent("connect_screen");
    return descriptor;
}

} // namespace

TEST(UiScreenManagerTests, RegistersScreenWithLogicalDocumentId)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);

    const SagaUI::UiScreenOperationResult result =
        manager->RegisterScreen(MakeDescriptor());

    EXPECT_TRUE(result.Succeeded());
    const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 1u);
    EXPECT_EQ(snapshot.screens.front().screenId.value, "connect_screen");
    EXPECT_EQ(snapshot.screens.front().state, SagaUI::UiScreenState::Registered);
    EXPECT_FALSE(snapshot.screens.front().documentHandleValid);
}

TEST(UiScreenManagerTests, DuplicateRegistrationDoesNotReplaceDescriptor)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);

    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor()).Succeeded());

    SagaUI::UiScreenDescriptor replacement = MakeDescriptor();
    replacement.documentId = SagaUI::UiDocumentId::FromContent("replacement");
    const SagaUI::UiScreenOperationResult duplicate =
        manager->RegisterScreen(replacement);

    EXPECT_EQ(
        duplicate.code,
        SagaUI::UiScreenDiagnosticCode::DuplicateRegistration);
    const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 1u);
    EXPECT_EQ(snapshot.screens.front().documentId.name, "connect_screen");
}

TEST(UiScreenManagerTests, LoadStoresOneBackendHandle)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor()).Succeeded());

    const SagaUI::UiScreenOperationResult result =
        manager->LoadScreen(SagaUI::UiScreenId::FromString("connect_screen"));

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(backend.loadCount, 1);
    EXPECT_TRUE(result.documentId.IsSet());
    EXPECT_EQ(backend.lastRequest.documentId.name, "connect_screen");
    EXPECT_FALSE(backend.lastRequest.showImmediately);

    const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 1u);
    EXPECT_EQ(snapshot.screens.front().state, SagaUI::UiScreenState::LoadedHidden);
    EXPECT_TRUE(snapshot.screens.front().documentHandleValid);
}

TEST(UiScreenManagerTests, PreloadPolicyLoadsDuringRegistration)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);

    SagaUI::UiScreenDescriptor descriptor = MakeDescriptor();
    descriptor.loadPolicy = SagaUI::UiScreenLoadPolicy::Preload;
    descriptor.showOnLoad = true;

    const SagaUI::UiScreenOperationResult result =
        manager->RegisterScreen(descriptor);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(backend.loadCount, 1);
    EXPECT_TRUE(backend.lastRequest.showImmediately);
    const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 1u);
    EXPECT_EQ(snapshot.screens.front().state, SagaUI::UiScreenState::Visible);
}

TEST(UiScreenManagerTests, DuplicateLoadDoesNotLoadBackendTwice)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor()).Succeeded());
    ASSERT_TRUE(
        manager->LoadScreen(SagaUI::UiScreenId::FromString("connect_screen"))
            .Succeeded());

    const SagaUI::UiScreenOperationResult duplicate =
        manager->LoadScreen(SagaUI::UiScreenId::FromString("connect_screen"));

    EXPECT_EQ(duplicate.code, SagaUI::UiScreenDiagnosticCode::DuplicateLoad);
    EXPECT_EQ(backend.loadCount, 1);
}

TEST(UiScreenManagerTests, ShowHideUnloadTransitionsAreDeterministic)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor()).Succeeded());

    EXPECT_TRUE(
        manager->ShowScreen(SagaUI::UiScreenId::FromString("connect_screen"))
            .Succeeded());
    EXPECT_EQ(backend.loadCount, 1);
    EXPECT_EQ(backend.showCount, 0);
    EXPECT_TRUE(manager->Snapshot().screens.front().visible);

    EXPECT_TRUE(
        manager->HideScreen(SagaUI::UiScreenId::FromString("connect_screen"))
            .Succeeded());
    EXPECT_EQ(backend.hideCount, 1);
    EXPECT_EQ(
        manager->Snapshot().screens.front().state,
        SagaUI::UiScreenState::LoadedHidden);

    EXPECT_TRUE(
        manager->ShowScreen(SagaUI::UiScreenId::FromString("connect_screen"))
            .Succeeded());
    EXPECT_EQ(backend.showCount, 1);

    EXPECT_TRUE(
        manager->UnloadScreen(SagaUI::UiScreenId::FromString("connect_screen"))
            .Succeeded());
    EXPECT_EQ(backend.unloadCount, 1);
    const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 1u);
    EXPECT_EQ(snapshot.screens.front().state, SagaUI::UiScreenState::Registered);
    EXPECT_FALSE(snapshot.screens.front().documentHandleValid);
}

TEST(UiScreenManagerTests, MissingScreenReturnsDiagnosticWithoutBackendCall)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);

    const SagaUI::UiScreenOperationResult result =
        manager->ShowScreen(SagaUI::UiScreenId::FromString("missing"));

    EXPECT_EQ(result.code, SagaUI::UiScreenDiagnosticCode::ScreenNotRegistered);
    EXPECT_EQ(backend.loadCount, 0);
    ASSERT_FALSE(manager->Diagnostics().empty());
}

TEST(UiScreenManagerTests, BackendLoadFailureLeavesScreenRegistered)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    backend.failLoad = true;
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor()).Succeeded());

    const SagaUI::UiScreenOperationResult result =
        manager->LoadScreen(SagaUI::UiScreenId::FromString("connect_screen"));

    EXPECT_EQ(result.code, SagaUI::UiScreenDiagnosticCode::BackendLoadFailed);
    const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 1u);
    EXPECT_EQ(snapshot.screens.front().state, SagaUI::UiScreenState::Registered);
    EXPECT_FALSE(snapshot.screens.front().documentHandleValid);
}

TEST(UiScreenManagerTests, SnapshotReportsRegisteredLoadedVisibleAndUnloadedStates)
{
    CountingUiBackend backend;
    ASSERT_TRUE(backend.Initialize({}));
    std::unique_ptr<SagaUI::IUiScreenManager> manager =
        SagaUI::CreateUiScreenManager(backend);
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor("registered")).Succeeded());
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor("loaded")).Succeeded());
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor("visible")).Succeeded());
    ASSERT_TRUE(manager->RegisterScreen(MakeDescriptor("unloaded")).Succeeded());

    ASSERT_TRUE(
        manager->LoadScreen(SagaUI::UiScreenId::FromString("loaded"))
            .Succeeded());
    ASSERT_TRUE(
        manager->ShowScreen(SagaUI::UiScreenId::FromString("visible"))
            .Succeeded());
    ASSERT_TRUE(
        manager->ShowScreen(SagaUI::UiScreenId::FromString("unloaded"))
            .Succeeded());
    ASSERT_TRUE(
        manager->UnloadScreen(SagaUI::UiScreenId::FromString("unloaded"))
            .Succeeded());

    const SagaUI::UiScreenSnapshot snapshot = manager->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 4u);
    EXPECT_EQ(snapshot.screens[0].screenId.value, "loaded");
    EXPECT_EQ(snapshot.screens[0].state, SagaUI::UiScreenState::LoadedHidden);
    EXPECT_EQ(snapshot.screens[1].screenId.value, "registered");
    EXPECT_EQ(snapshot.screens[1].state, SagaUI::UiScreenState::Registered);
    EXPECT_EQ(snapshot.screens[2].screenId.value, "unloaded");
    EXPECT_EQ(snapshot.screens[2].state, SagaUI::UiScreenState::Registered);
    EXPECT_EQ(snapshot.screens[3].screenId.value, "visible");
    EXPECT_EQ(snapshot.screens[3].state, SagaUI::UiScreenState::Visible);
}
