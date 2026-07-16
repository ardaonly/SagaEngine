/// @file UiBackendTests.cpp
/// @brief Focused tests for the minimal runtime UI backend contract.

#include <gtest/gtest.h>

#include "Backends/RmlUi/RmlUiUiBackend.h"
#include "SagaEngine/UI/IUiBackend.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace
{

namespace SagaUI = ::SagaEngine::UI;
namespace SagaBackendsUI = ::SagaEngine::UI::Backends;

[[nodiscard]] SagaUI::UiBackendConfig MakeConfig(const char* contextName)
{
    SagaUI::UiBackendConfig config;
    config.viewport = SagaUI::UiViewport{640, 360, 1.0f};
    config.assetRoot = std::filesystem::temp_directory_path();
    config.contextName = contextName;
    return config;
}

} // namespace

TEST(UiBackendTests, NullBackendCanInitializeUpdateRenderAndShutdown)
{
    SagaUI::NullUiBackend backend;

    EXPECT_TRUE(backend.Initialize(MakeConfig("NullUiContractTest")));

    SagaUI::UiInputEvent event;
    event.type = SagaUI::UiInputEventType::Key;
    event.key = ::SagaEngine::Input::KeyCode::Enter;
    event.pressed = true;
    EXPECT_TRUE(backend.SubmitInput(event));

    backend.Update(1.0 / 60.0);
    EXPECT_TRUE(backend.Render());
    EXPECT_EQ(backend.LastRenderStats().frameIndex, 1u);
    EXPECT_TRUE(backend.Diagnostics().empty());

    backend.Shutdown();
}

TEST(UiBackendTests, NullBackendReportsMissingDocument)
{
    SagaUI::NullUiBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeConfig("NullUiMissingDocumentTest")));

    SagaUI::UiDocumentRequest request;
    request.path = "saga-ui-missing-document.rml";
    const SagaUI::UiDocumentHandle handle = backend.LoadDocument(request);

    EXPECT_FALSE(SagaUI::IsValid(handle));
    ASSERT_FALSE(backend.Diagnostics().empty());
    EXPECT_EQ(
        backend.Diagnostics().back().code,
        SagaUI::UiDiagnosticCode::DocumentLoadFailed);
}

TEST(UiBackendTests, NullBackendLoadsDocumentThroughResourceProvider)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "SagaNullUiResourceProviderTest";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "UI");
    {
        std::ofstream document(root / "UI" / "connect_screen.rml");
        document << "<rml/>";
    }

    std::shared_ptr<SagaUI::IUiResourceProvider> provider =
        SagaUI::CreateDefaultUiResourceProvider();
    ASSERT_NE(provider, nullptr);
    ASSERT_TRUE(provider->MountContentRoot(root).Succeeded());

    SagaUI::UiBackendConfig config = MakeConfig("NullUiLogicalDocumentTest");
    config.resourceProvider = provider;

    SagaUI::NullUiBackend backend;
    ASSERT_TRUE(backend.Initialize(config));

    const SagaUI::UiDocumentHandle handle = backend.LoadDocument(
        SagaUI::UiDocumentRequest::FromDocumentId(
            SagaUI::UiDocumentId::FromContent("connect_screen")));

    EXPECT_TRUE(SagaUI::IsValid(handle));
    EXPECT_TRUE(backend.Diagnostics().empty());

    backend.Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiBackendTests, RmlUiBackendReportsMissingDocumentWithoutLeakingTypes)
{
    const std::unique_ptr<SagaUI::IUiBackend> backend =
        SagaBackendsUI::CreateRmlUiUiBackend();
    ASSERT_NE(backend, nullptr);
    ASSERT_TRUE(backend->Initialize(MakeConfig("RmlUiMissingDocumentTest")));

    SagaUI::UiDocumentRequest request;
    request.path = "saga-rmlui-missing-document.rml";
    const SagaUI::UiDocumentHandle handle = backend->LoadDocument(request);

    EXPECT_FALSE(SagaUI::IsValid(handle));
    ASSERT_FALSE(backend->Diagnostics().empty());

    bool sawLoadFailure = false;
    for (const SagaUI::UiDiagnostic& diagnostic : backend->Diagnostics())
    {
        if (diagnostic.code == SagaUI::UiDiagnosticCode::DocumentLoadFailed)
        {
            sawLoadFailure = true;
            break;
        }
    }
    EXPECT_TRUE(sawLoadFailure);

    backend->Shutdown();
}
