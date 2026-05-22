/// @file EditorNotificationTests.cpp
/// @brief Unit tests for transient editor notification feedback.

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace SagaEditor;

class NotificationFakeMainWindow final : public IUIMainWindow
{
public:
    void Show() override {}
    void ShowMaximized() override {}
    void Hide() override {}
    void Close() override {}

    void SetTitle(const std::string& value) override { title = value; }
    void SetSize(int w, int h) override { width = w; height = h; }

    [[nodiscard]] int GetWidth() const noexcept override { return width; }
    [[nodiscard]] int GetHeight() const noexcept override { return height; }
    [[nodiscard]] void* GetNativeHandle() const noexcept override { return nullptr; }

    void ApplyShellLayout(const ShellLayout&) override {}
    void SetStatusMessage(const std::string& value) override { status = value; }

    void DockPanel(void*,
                   const std::string&,
                   const std::string&,
                   UIDockArea) override
    {}
    void UndockPanel(const std::string&) override {}
    void FocusPanel(const std::string&) override {}
    void SetPanelVisible(const std::string&, bool) override {}

    [[nodiscard]] std::vector<std::uint8_t> SaveState() const override
    {
        return {};
    }
    [[nodiscard]] bool RestoreState(const std::vector<std::uint8_t>&) override
    {
        return true;
    }

    void SetOnClose(CloseCallback cb) override { onClose = std::move(cb); }
    void SetOnCommand(CommandCallback cb) override { onCommand = std::move(cb); }

    int width = 1600;
    int height = 900;
    std::string title;
    std::string status;
    CloseCallback onClose;
    CommandCallback onCommand;
};

} // namespace

TEST(EditorNotificationCenterTests, PublishesAndStoresRecentNotifications)
{
    EditorNotificationCenter center;
    std::vector<EditorNotification> received;
    const EditorNotificationSubscription subscription =
        center.Subscribe([&received](const EditorNotification& notification)
                         {
                             received.push_back(notification);
                         });

    EditorNotification notification;
    notification.severity = EditorNotificationSeverity::Success;
    notification.source = "test.source";
    notification.message = "Saved.";
    center.Publish(notification);

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].source, "test.source");
    EXPECT_EQ(received[0].message, "Saved.");
    ASSERT_EQ(center.Recent().size(), 1u);
    EXPECT_EQ(center.Recent()[0].severity, EditorNotificationSeverity::Success);

    center.Unsubscribe(subscription);
    center.Publish({ EditorNotificationSeverity::Info,
                     "test.source",
                     "Ignored.",
                     {} });
    EXPECT_EQ(received.size(), 1u);
    EXPECT_EQ(center.Recent().size(), 2u);
}

TEST(EditorNotificationCenterTests, EmptySubscriptionIsRejected)
{
    EditorNotificationCenter center;
    EXPECT_EQ(center.Subscribe({}), kInvalidEditorNotificationSubscription);
}

TEST(EditorNotificationHostTests, HostExposesNotificationCenter)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    host.GetEditorNotificationCenter().Publish({
        EditorNotificationSeverity::Info,
        "test.host",
        "Host notification.",
        {}
    });

    ASSERT_EQ(host.GetEditorNotificationCenter().Recent().size(), 1u);
    EXPECT_EQ(host.GetEditorNotificationCenter().Recent()[0].message,
              "Host notification.");
    host.Shutdown();
}

TEST(EditorNotificationShellTests, ShellForwardsNotificationsToStatusBar)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    auto window = std::make_unique<NotificationFakeMainWindow>();
    NotificationFakeMainWindow* rawWindow = window.get();

    EditorShellConfig config;
    config.registerDefaultPanels = false;
    config.applyActivePersona = false;
    config.showOnInit = false;

    EditorShell shell;
    ASSERT_TRUE(shell.Init(host, std::move(window), config));

    host.GetEditorNotificationCenter().Publish({
        EditorNotificationSeverity::Success,
        "editor.customization.workspace",
        "Workspace customization saved. Restart editor to apply workspace changes.",
        {}
    });

    EXPECT_EQ(rawWindow->status,
              "Workspace customization saved. Restart editor to apply workspace changes.");

    shell.Shutdown();
    host.Shutdown();
}
