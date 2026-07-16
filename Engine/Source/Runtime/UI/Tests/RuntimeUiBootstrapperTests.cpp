/// @file RuntimeUiBootstrapperTests.cpp
/// @brief Focused tests for runtime UI bootstrap.

#include <gtest/gtest.h>

#include "SagaEngine/UI/RuntimeUiBootstrapper.h"

#include <filesystem>
#include <fstream>

namespace
{

namespace SagaUI = ::SagaEngine::UI;

[[nodiscard]] std::filesystem::path TestRoot(const char* name)
{
    return std::filesystem::temp_directory_path() / "SagaRuntimeUiTests" / name;
}

void WriteConnectScreen(const std::filesystem::path& root)
{
    std::filesystem::create_directories(root / "UI");
    std::ofstream document(root / "UI" / "connect_screen.rml");
    document << "<rml/>";
}

[[nodiscard]] SagaUI::UiRuntimeBootstrapRequest MakeRequest(
    std::filesystem::path contentRoot)
{
    SagaUI::UiRuntimeBootstrapRequest request;
    request.backend = std::make_unique<SagaUI::NullUiBackend>();
    request.viewport = SagaUI::UiViewport{640, 360, 1.0f};
    request.contextName = "RuntimeUiBootstrapperTest";
    request.contentRoot = std::move(contentRoot);
    return request;
}

} // namespace

TEST(RuntimeUiBootstrapperTests, BootsConnectScreenThroughScreenManager)
{
    const std::filesystem::path root = TestRoot("BootsConnectScreen");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRequest(root));

    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);
    EXPECT_TRUE(result.diagnostics.empty());

    const SagaUI::UiScreenSnapshot snapshot = result.context->Snapshot();
    ASSERT_EQ(snapshot.screens.size(), 1u);
    EXPECT_EQ(snapshot.screens.front().screenId.value, "connect_screen");
    EXPECT_EQ(snapshot.screens.front().state, SagaUI::UiScreenState::Visible);
    EXPECT_TRUE(snapshot.screens.front().documentHandleValid);

    result.context->Update(1.0 / 60.0);
    EXPECT_TRUE(result.context->Render());
    result.context->Shutdown();

    std::filesystem::remove_all(root);
}

TEST(RuntimeUiBootstrapperTests, DisabledBootstrapSucceedsWithoutContext)
{
    SagaUI::UiRuntimeBootstrapRequest request;
    request.enabled = false;
    request.backend = std::make_unique<SagaUI::NullUiBackend>();

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(std::move(request));

    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.disabled);
    EXPECT_EQ(result.context, nullptr);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics.front().code,
        SagaUI::UiRuntimeBootstrapDiagnosticCode::Disabled);
}

TEST(RuntimeUiBootstrapperTests, MissingDocumentReportsDeterministicDiagnostic)
{
    const std::filesystem::path root = TestRoot("MissingDocument");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "UI");

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRequest(root));

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.context, nullptr);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(
        result.diagnostics.back().code,
        SagaUI::UiRuntimeBootstrapDiagnosticCode::ScreenShowFailed);
    EXPECT_EQ(result.diagnostics.back().screenId.value, "connect_screen");
    EXPECT_EQ(result.diagnostics.back().documentId.name, "connect_screen");

    std::filesystem::remove_all(root);
}

TEST(RuntimeUiBootstrapperTests, MissingBackendFailsBeforeMounting)
{
    SagaUI::UiRuntimeBootstrapRequest request;
    request.contentRoot = TestRoot("MissingBackend");

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(std::move(request));

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.context, nullptr);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(
        result.diagnostics.front().code,
        SagaUI::UiRuntimeBootstrapDiagnosticCode::MissingBackend);
}
