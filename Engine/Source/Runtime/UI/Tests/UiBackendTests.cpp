// SPDX-License-Identifier: MPL-2.0
/// @file UiBackendTests.cpp
/// @brief Focused tests for the minimal runtime UI backend contract.

#include <gtest/gtest.h>

#include "Backends/RmlUi/RmlUiUiBackend.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"
#include "SagaEngine/UI/IUiBackend.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>

namespace
{

namespace SagaUI = ::SagaEngine::UI;
namespace SagaBackendsUI = ::SagaEngine::UI::Backends;
namespace Render = ::SagaEngine::Render::Backend;

class RecordingSubmission final : public Render::IRenderOverlaySubmission
{
public:
    Render::RenderOverlaySubmissionResult InitializeOverlay() override
    { return {Render::RenderOverlaySubmissionStatus::Success, {}}; }
    Render::RenderOverlayTextureHandle CreateOverlayTextureResource(
        const Render::RenderOverlayTextureDesc&, const std::uint8_t* pixels) override
    { return pixels ? Render::RenderOverlayTextureHandle{nextTexture_++, 1} :
        Render::RenderOverlayTextureHandle{}; }
    void DestroyOverlayTextureResource(Render::RenderOverlayTextureHandle) override {}
    Render::RenderOverlaySubmissionResult SubmitOverlayFrame(
        const Render::RenderOverlayFrame& frame) override
    { lastFrame = frame; return {Render::RenderOverlaySubmissionStatus::Success, {}}; }
    void ShutdownOverlay() override {}

    Render::RenderOverlayFrame lastFrame;
private:
    std::uint32_t nextTexture_ = 1;
};

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
    EXPECT_FALSE(backend.GetDocumentElementBounds(
        SagaUI::UiDocumentHandle::kInvalid, "missing"));
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

TEST(UiBackendTests, RmlUiBackendProducesStarterPromptGeometry)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Samples/StarterArena/Content/UI";
    RecordingSubmission submission;
    auto config = MakeConfig("RmlUiStarterPromptGeometryTest");
    config.assetRoot = root / "HUD";
    config.fontPaths = {root / "ThirdParty/DejaVu/DejaVuSans.ttf"};
    config.overlaySubmission = &submission;
    const std::unique_ptr<SagaUI::IUiBackend> backend =
        SagaBackendsUI::CreateRmlUiUiBackend();
    ASSERT_TRUE(backend->Initialize(config));
    const auto document = backend->LoadDocument(SagaUI::UiDocumentRequest::FromDebugPath(
        root / "HUD/interaction-prompt.rml", true,
        SagaUI::UiScreenId::FromString("starter-arena.hud")));
    ASSERT_TRUE(SagaUI::IsValid(document));
    ASSERT_TRUE(backend->SetDocumentElementText(document, "interaction-prompt",
                                                "[E] Open Door"));
    backend->Update(1.0 / 60.0);
    const auto bounds = backend->GetDocumentElementBounds(
        document, "interaction-prompt");
    ASSERT_TRUE(bounds);
    EXPECT_NEAR(bounds->x + bounds->width * 0.5F, 320.0F, 2.0F);
    EXPECT_NEAR(360.0F - (bounds->y + bounds->height), 28.8F, 2.0F);
    EXPECT_FALSE(backend->GetDocumentElementBounds(document, "missing"));
    ASSERT_TRUE(backend->HideDocument(document));
    backend->Update(1.0 / 60.0);
    EXPECT_FALSE(backend->GetDocumentElementBounds(document, "interaction-prompt"));
    ASSERT_TRUE(backend->ShowDocument(document));
    backend->SetViewport({800, 500, 1.5F});
    backend->Update(1.0 / 60.0);
    const auto resizedBounds = backend->GetDocumentElementBounds(
        document, "interaction-prompt");
    ASSERT_TRUE(resizedBounds);
    EXPECT_NEAR(resizedBounds->x + resizedBounds->width * 0.5F, 400.0F, 2.0F);
    EXPECT_NEAR(500.0F - (resizedBounds->y + resizedBounds->height), 40.0F, 2.0F);
    ASSERT_TRUE(backend->Render());
    EXPECT_GT(backend->LastRenderStats().drawCallCount, 0U);
    EXPECT_FALSE(submission.lastFrame.drawLists.empty());
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    std::uint8_t maxAlpha = 0;
    std::size_t commandCount = 0;
    for (const auto& list : submission.lastFrame.drawLists)
    {
        EXPECT_FALSE(list.indices.empty());
        for (const auto& vertex : list.vertices)
        {
            minX = std::min(minX, vertex.pos[0]);
            minY = std::min(minY, vertex.pos[1]);
            maxX = std::max(maxX, vertex.pos[0]);
            maxY = std::max(maxY, vertex.pos[1]);
            maxAlpha = std::max(maxAlpha,
                static_cast<std::uint8_t>(vertex.colorRgba8 >> 24u));
        }
        for (const auto& command : list.commands)
        {
            EXPECT_TRUE(command.texture);
            EXPECT_GT(command.elementCount, 0U);
            EXPECT_GT(command.clipRect.right, command.clipRect.left);
            EXPECT_GT(command.clipRect.bottom, command.clipRect.top);
            ++commandCount;
        }
    }
    EXPECT_GT(commandCount, 0U);
    EXPECT_GT(maxAlpha, 0U);
    EXPECT_LT(minX, 800.0f);
    EXPECT_LT(minY, 500.0f);
    EXPECT_GT(maxX, 0.0f);
    EXPECT_GT(maxY, 0.0f);
    backend->Shutdown();
}
