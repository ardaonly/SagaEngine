/// @file UiInputRouterTests.cpp
/// @brief Focused tests for backend-neutral UI input routing.

#include <gtest/gtest.h>

#include "SagaEngine/UI/IUiInputRouter.h"
#include "SagaEngine/UI/RuntimeUiBootstrapper.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace
{

namespace SagaUI = ::SagaEngine::UI;
namespace SagaInput = ::SagaEngine::Input;

[[nodiscard]] SagaUI::UiInputEvent MakeMouseEvent()
{
    SagaUI::UiInputEvent event;
    event.type = SagaUI::UiInputEventType::MouseButton;
    event.mouseButton = SagaInput::MouseButton::Left;
    event.pressed = true;
    return event;
}

[[nodiscard]] SagaUI::UiInputEvent MakeKeyboardEvent()
{
    SagaUI::UiInputEvent event;
    event.type = SagaUI::UiInputEventType::Key;
    event.key = SagaInput::KeyCode::W;
    event.pressed = true;
    return event;
}

[[nodiscard]] SagaUI::UiInputEvent MakeTextEvent()
{
    SagaUI::UiInputEvent event;
    event.type = SagaUI::UiInputEventType::Text;
    event.textUtf8[0] = 'a';
    return event;
}

[[nodiscard]] std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() /
           "SagaUiInputRouterRuntimeTest";
}

void WriteConnectScreen(const std::filesystem::path& root)
{
    std::filesystem::create_directories(root / "UI");
    std::ofstream document(root / "UI" / "connect_screen.rml");
    document << "<rml/>";
}

} // namespace

TEST(UiInputRouterTests, NoCaptureAllowsGameplayInput)
{
    std::unique_ptr<SagaUI::IUiInputRouter> router =
        SagaUI::CreateUiInputRouter();

    const SagaUI::UiInputRoutingResult mouse =
        router->RouteInput(MakeMouseEvent());
    EXPECT_EQ(mouse.device, SagaUI::UiInputDevice::Mouse);
    EXPECT_FALSE(mouse.uiReceivesInput);
    EXPECT_FALSE(mouse.gameplayBlocked);

    const SagaUI::UiInputRoutingResult keyboard =
        router->RouteInput(MakeKeyboardEvent());
    EXPECT_EQ(keyboard.device, SagaUI::UiInputDevice::Keyboard);
    EXPECT_FALSE(keyboard.uiReceivesInput);
    EXPECT_FALSE(keyboard.gameplayBlocked);

    const SagaUI::UiInputRoutingResult text =
        router->RouteInput(MakeTextEvent());
    EXPECT_EQ(text.device, SagaUI::UiInputDevice::Text);
    EXPECT_FALSE(text.uiReceivesInput);
    EXPECT_FALSE(text.gameplayBlocked);
}

TEST(UiInputRouterTests, MouseCaptureBlocksMouseGameplayInputOnly)
{
    std::unique_ptr<SagaUI::IUiInputRouter> router =
        SagaUI::CreateUiInputRouter();
    SagaUI::UiInputCaptureState state;
    state.mouseCaptured = true;
    router->SetCaptureState(state);

    const SagaUI::UiInputRoutingResult mouse =
        router->RouteInput(MakeMouseEvent());
    EXPECT_TRUE(mouse.uiReceivesInput);
    EXPECT_TRUE(mouse.gameplayBlocked);
    EXPECT_EQ(
        mouse.decision,
        SagaUI::UiInputRoutingDecision::BlockedByMouseCapture);

    const SagaUI::UiInputRoutingResult keyboard =
        router->RouteInput(MakeKeyboardEvent());
    EXPECT_FALSE(keyboard.gameplayBlocked);
}

TEST(UiInputRouterTests, KeyboardCaptureBlocksKeyboardGameplayInput)
{
    std::unique_ptr<SagaUI::IUiInputRouter> router =
        SagaUI::CreateUiInputRouter();
    SagaUI::UiInputCaptureState state;
    state.keyboardCaptured = true;
    router->SetCaptureState(state);

    const SagaUI::UiInputRoutingResult result =
        router->RouteInput(MakeKeyboardEvent());

    EXPECT_TRUE(result.uiReceivesInput);
    EXPECT_TRUE(result.gameplayBlocked);
    EXPECT_EQ(
        result.decision,
        SagaUI::UiInputRoutingDecision::BlockedByKeyboardCapture);
}

TEST(UiInputRouterTests, TextInputBlocksTextAndKeyboardGameplayInput)
{
    std::unique_ptr<SagaUI::IUiInputRouter> router =
        SagaUI::CreateUiInputRouter();
    SagaUI::UiInputCaptureState state;
    state.textInputActive = true;
    router->SetCaptureState(state);

    const SagaUI::UiInputRoutingResult text =
        router->RouteInput(MakeTextEvent());
    EXPECT_TRUE(text.uiReceivesInput);
    EXPECT_TRUE(text.gameplayBlocked);
    EXPECT_EQ(
        text.decision,
        SagaUI::UiInputRoutingDecision::BlockedByTextInput);

    const SagaUI::UiInputRoutingResult keyboard =
        router->RouteInput(MakeKeyboardEvent());
    EXPECT_TRUE(keyboard.uiReceivesInput);
    EXPECT_TRUE(keyboard.gameplayBlocked);
    EXPECT_EQ(
        keyboard.decision,
        SagaUI::UiInputRoutingDecision::BlockedByTextInput);
}

TEST(UiInputRouterTests, FocusedSurfaceMetadataIsPreserved)
{
    std::unique_ptr<SagaUI::IUiInputRouter> router =
        SagaUI::CreateUiInputRouter();
    SagaUI::UiInputCaptureState state;
    state.keyboardCaptured = true;
    state.focusedSurface.screenId =
        SagaUI::UiScreenId::FromString("connect_screen");
    state.focusedSurface.document =
        static_cast<SagaUI::UiDocumentHandle>(42);
    router->SetCaptureState(state);

    const SagaUI::UiInputRoutingResult result =
        router->RouteInput(MakeKeyboardEvent());

    EXPECT_TRUE(result.captureState.focusedSurface.IsSet());
    EXPECT_EQ(
        result.captureState.focusedSurface.screenId.value,
        "connect_screen");
    EXPECT_EQ(
        result.captureState.focusedSurface.document,
        static_cast<SagaUI::UiDocumentHandle>(42));
}

TEST(UiInputRouterTests, ResetClearsCaptureAndFocus)
{
    std::unique_ptr<SagaUI::IUiInputRouter> router =
        SagaUI::CreateUiInputRouter();
    SagaUI::UiInputCaptureState state;
    state.mouseCaptured = true;
    state.keyboardCaptured = true;
    state.textInputActive = true;
    state.focusedSurface.screenId =
        SagaUI::UiScreenId::FromString("connect_screen");
    router->SetCaptureState(state);

    router->ResetFrameCapture();

    EXPECT_FALSE(router->CaptureState().HasAnyCapture());
    EXPECT_FALSE(router->CaptureState().focusedSurface.IsSet());
    EXPECT_FALSE(router->RouteInput(MakeMouseEvent()).gameplayBlocked);
}

TEST(UiInputRouterTests, RuntimeContextExposesNoCaptureRouterByDefault)
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

    const SagaUI::UiInputRoutingResult routing =
        result.context->InputRouter().RouteInput(MakeKeyboardEvent());
    EXPECT_FALSE(routing.uiReceivesInput);
    EXPECT_FALSE(routing.gameplayBlocked);
    EXPECT_FALSE(result.context->InputRouter().CaptureState().HasAnyCapture());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}
