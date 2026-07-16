/// @file UiEventBridgeTests.cpp
/// @brief Focused tests for backend-neutral UI event bridge contracts.

#include <gtest/gtest.h>

#include "SagaEngine/UI/IUiEventQueue.h"
#include "SagaEngine/UI/RuntimeUiBootstrapper.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

namespace SagaUI = ::SagaEngine::UI;

[[nodiscard]] SagaUI::UiEvent MakeEvent(
    SagaUI::UiEventType type,
    std::string elementId)
{
    SagaUI::UiEvent event;
    event.screenId = SagaUI::UiScreenId::FromString("connect_screen");
    event.documentId = SagaUI::UiDocumentId::FromContent("connect_screen");
    event.document = static_cast<SagaUI::UiDocumentHandle>(7);
    event.elementId = SagaUI::UiElementId::FromString(std::move(elementId));
    event.type = type;
    return event;
}

[[nodiscard]] std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() /
           "SagaUiEventBridgeRuntimeTest";
}

void WriteConnectScreen(const std::filesystem::path& root)
{
    std::filesystem::create_directories(root / "UI");
    std::ofstream document(root / "UI" / "connect_screen.rml");
    document << "<rml><body><button id=\"connect_button\"/></body></rml>";
}

} // namespace

TEST(UiEventBridgeTests, QueuePushPollDrainOrderIsDeterministic)
{
    std::shared_ptr<SagaUI::IUiEventQueue> queue =
        SagaUI::CreateUiEventQueue();

    EXPECT_TRUE(queue->PushEvent(
        MakeEvent(SagaUI::UiEventType::Click, "connect_button")));
    EXPECT_TRUE(queue->PushEvent(
        MakeEvent(SagaUI::UiEventType::Submit, "connect_form")));

    const std::vector<SagaUI::UiEvent> polled = queue->PollEvents();
    ASSERT_EQ(polled.size(), 2U);
    EXPECT_EQ(polled[0].sequence, 1U);
    EXPECT_EQ(polled[1].sequence, 2U);
    EXPECT_EQ(polled[0].elementId.value, "connect_button");
    EXPECT_EQ(polled[1].elementId.value, "connect_form");

    const std::vector<SagaUI::UiEvent> drained = queue->DrainEvents();
    ASSERT_EQ(drained.size(), 2U);
    EXPECT_EQ(drained[0].sequence, 1U);
    EXPECT_EQ(drained[1].sequence, 2U);
    EXPECT_TRUE(queue->PollEvents().empty());
}

TEST(UiEventBridgeTests, EmptyElementIdProducesDiagnosticAndIsNotQueued)
{
    std::shared_ptr<SagaUI::IUiEventQueue> queue =
        SagaUI::CreateUiEventQueue();

    EXPECT_FALSE(queue->PushEvent(MakeEvent(SagaUI::UiEventType::Click, "")));
    EXPECT_TRUE(queue->PollEvents().empty());
    ASSERT_EQ(queue->Diagnostics().size(), 1U);
    EXPECT_EQ(
        queue->Diagnostics()[0].code,
        SagaUI::UiEventDiagnosticCode::EmptyElementId);
}

TEST(UiEventBridgeTests, ClickEventPreservesScreenDocumentAndElementMetadata)
{
    std::shared_ptr<SagaUI::IUiEventQueue> queue =
        SagaUI::CreateUiEventQueue();

    EXPECT_TRUE(queue->PushEvent(
        MakeEvent(SagaUI::UiEventType::Click, "connect_button")));

    const std::vector<SagaUI::UiEvent> events = queue->DrainEvents();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].type, SagaUI::UiEventType::Click);
    EXPECT_EQ(events[0].screenId.value, "connect_screen");
    EXPECT_EQ(events[0].documentId.name, "connect_screen");
    EXPECT_EQ(
        events[0].document,
        static_cast<SagaUI::UiDocumentHandle>(7));
    EXPECT_EQ(events[0].elementId.value, "connect_button");
}

TEST(UiEventBridgeTests, TextChangedEventPreservesTextPayload)
{
    std::shared_ptr<SagaUI::IUiEventQueue> queue =
        SagaUI::CreateUiEventQueue();
    SagaUI::UiEvent event =
        MakeEvent(SagaUI::UiEventType::TextChanged, "server_address");
    event.text = "local-dev";

    EXPECT_TRUE(queue->PushEvent(std::move(event)));

    const std::vector<SagaUI::UiEvent> events = queue->DrainEvents();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].type, SagaUI::UiEventType::TextChanged);
    EXPECT_EQ(events[0].text, "local-dev");
}

TEST(UiEventBridgeTests, FocusEventsPreserveFocusedSurfaceMetadata)
{
    std::shared_ptr<SagaUI::IUiEventQueue> queue =
        SagaUI::CreateUiEventQueue();

    EXPECT_TRUE(queue->PushEvent(
        MakeEvent(SagaUI::UiEventType::FocusGained, "server_address")));
    EXPECT_TRUE(queue->PushEvent(
        MakeEvent(SagaUI::UiEventType::FocusLost, "server_address")));

    const std::vector<SagaUI::UiEvent> events = queue->DrainEvents();
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(events[0].type, SagaUI::UiEventType::FocusGained);
    EXPECT_EQ(events[1].type, SagaUI::UiEventType::FocusLost);
    EXPECT_EQ(events[0].screenId.value, "connect_screen");
    EXPECT_EQ(events[1].documentId.name, "connect_screen");
}

TEST(UiEventBridgeTests, RuntimeContextExposesEmptyEventQueueByDefault)
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

    EXPECT_TRUE(result.context->PollEvents().empty());
    EXPECT_TRUE(result.context->DrainEvents().empty());
    EXPECT_TRUE(result.context->EventDiagnostics().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiEventBridgeTests, NullBackendDoesNotEmitEventsByDefault)
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

    result.context->Update(0.016);
    EXPECT_TRUE(result.context->DrainEvents().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}
