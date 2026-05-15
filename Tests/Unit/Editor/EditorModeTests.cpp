/// @file EditorModeTests.cpp
/// @brief Unit tests for the mountable SagaEditor mode facade.

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Profile/EditorProfile.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorMode.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace SagaEditor;

class FakeMainWindow final : public IUIMainWindow
{
public:
    void Show() override { shown = true; }
    void ShowMaximized() override
    {
        shown = true;
        maximized = true;
    }
    void Hide() override { shown = false; }
    void Close() override
    {
        closed = true;
        if (onClose)
            onClose();
    }

    void SetTitle(const std::string& value) override { title = value; }
    void SetSize(int w, int h) override
    {
        width = w;
        height = h;
    }

    [[nodiscard]] int GetWidth() const noexcept override { return width; }
    [[nodiscard]] int GetHeight() const noexcept override { return height; }
    [[nodiscard]] void* GetNativeHandle() const noexcept override { return nullptr; }

    void ApplyShellLayout(const ShellLayout&) override { ++layoutApplyCount; }
    void SetStatusMessage(const std::string& value) override { status = value; }

    void DockPanel(void*,
                   const std::string& panelId,
                   const std::string&,
                   UIDockArea) override
    {
        dockedPanels.push_back(panelId);
    }

    void UndockPanel(const std::string&) override {}
    void FocusPanel(const std::string& panelId) override
    {
        focusedPanels.push_back(panelId);
    }
    void SetPanelVisible(const std::string&, bool) override {}

    [[nodiscard]] std::vector<uint8_t> SaveState() const override { return {}; }
    [[nodiscard]] bool RestoreState(const std::vector<uint8_t>&) override { return true; }

    void SetOnClose(CloseCallback callback) override { onClose = std::move(callback); }
    void SetOnCommand(CommandCallback callback) override
    {
        onCommand = std::move(callback);
    }

    int width = 0;
    int height = 0;
    bool shown = false;
    bool maximized = false;
    bool closed = false;
    int layoutApplyCount = 0;
    std::string title;
    std::string status;
    std::vector<std::string> dockedPanels;
    std::vector<std::string> focusedPanels;
    CloseCallback onClose;
    CommandCallback onCommand;
};

[[nodiscard]] std::unique_ptr<MemoryEditorSettingsStore> MakeSettingsStore()
{
    return std::make_unique<MemoryEditorSettingsStore>();
}

TEST(EditorModeTest, MountsEditorShellWithoutCreatingApplication)
{
    EditorMode mode;
    auto window = std::make_unique<FakeMainWindow>();
    FakeMainWindow* windowRaw = window.get();

    EditorModeConfig config;
    config.windowTitle = "Saga Embedded Editor";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.registerDefaultPanels = false;
    config.showOnMount = false;

    ASSERT_TRUE(mode.Mount(std::move(window), MakeSettingsStore(), config));

    EXPECT_TRUE(mode.IsMounted());
    ASSERT_NE(mode.GetHost(), nullptr);
    ASSERT_NE(mode.GetShell(), nullptr);
    EXPECT_EQ(windowRaw->title, "Saga Embedded Editor - Basic");
    EXPECT_EQ(windowRaw->width, 1280);
    EXPECT_EQ(windowRaw->height, 720);
    EXPECT_FALSE(windowRaw->shown);
    EXPECT_GE(windowRaw->layoutApplyCount, 1);
    EXPECT_TRUE(mode.GetShell()->GetRegisteredPanelTitles().empty());

    mode.Unmount();
    EXPECT_FALSE(mode.IsMounted());
}

TEST(EditorModeTest, AppliesInitialProfileBeforeShellMount)
{
    EditorMode mode;
    auto window = std::make_unique<FakeMainWindow>();

    EditorModeConfig config;
    config.initialProfileId = "saga.profile.standard_pipeline";
    config.registerDefaultPanels = false;
    config.showOnMount = false;

    ASSERT_TRUE(mode.Mount(std::move(window), MakeSettingsStore(), config));

    const EditorProfile* activeProfile =
        mode.GetHost()->GetEditorProfileRegistry().GetActive();
    ASSERT_NE(activeProfile, nullptr);
    EXPECT_EQ(activeProfile->id, "saga.profile.standard_pipeline");
}

TEST(EditorModeTest, RejectsInvalidInputsAndDoubleMount)
{
    EditorMode mode;
    EditorModeConfig config;
    config.registerDefaultPanels = false;
    config.showOnMount = false;

    EXPECT_FALSE(mode.Mount({}, MakeSettingsStore(), config));
    EXPECT_FALSE(mode.Mount(std::make_unique<FakeMainWindow>(), {}, config));

    ASSERT_TRUE(mode.Mount(std::make_unique<FakeMainWindow>(),
                           MakeSettingsStore(),
                           config));
    EXPECT_FALSE(mode.Mount(std::make_unique<FakeMainWindow>(),
                            MakeSettingsStore(),
                            config));
}

} // namespace
