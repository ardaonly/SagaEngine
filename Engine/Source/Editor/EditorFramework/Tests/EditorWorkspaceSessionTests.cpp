// SPDX-License-Identifier: MPL-2.0
/// @file EditorWorkspaceSessionTests.cpp
/// @brief Unit tests for the mountable SagaEditor mode facade.

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorWorkspaceSession.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <gtest/gtest.h>

#include <memory>
#include <filesystem>
#include <fstream>
#include <iterator>
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
        if (onClose && !onClose()) closed = false;
    }

    void SetTitle(const std::string& value) override { title = value; }
    void SetSize(int w, int h) override
    {
        width = w;
        height = h;
    }

    [[nodiscard]] int GetWidth() const noexcept override { return width; }
    [[nodiscard]] int GetHeight() const noexcept override { return height; }

    void ApplyShellLayout(const ShellLayout&) override { ++layoutApplyCount; }
    void SetStatusMessage(const std::string& value) override { status = value; }

    void DockPanel(QWidget*,
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

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::filesystem::path TestSourceRoot()
{
    const auto configured = std::filesystem::path(SAGA_SOURCE_ROOT);
    if (std::filesystem::is_regular_file(
            configured / "Samples/StarterArena/StarterArena.sagaproj"))
        return configured;

    auto cursor = std::filesystem::current_path();
    for (;;)
    {
        if (std::filesystem::is_regular_file(
                cursor / "Samples/StarterArena/StarterArena.sagaproj"))
            return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor)
            break;
        cursor = parent;
    }
    return configured;
}

TEST(EditorWorkspaceSessionTest, MountsEditorShellWithoutCreatingApplication)
{
    EditorWorkspaceSession mode;
    auto window = std::make_unique<FakeMainWindow>();
    FakeMainWindow* windowRaw = window.get();

    EditorWorkspaceSessionConfig config;
    config.windowTitle = "Saga Embedded Editor";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.registerDefaultPanels = false;
    config.showOnMount = false;

    ASSERT_TRUE(mode.Mount(std::move(window), MakeSettingsStore(), config));

    EXPECT_TRUE(mode.IsMounted());
    ASSERT_NE(mode.GetHost(), nullptr);
    ASSERT_NE(mode.GetShell(), nullptr);
    EXPECT_EQ(windowRaw->title, "Saga Embedded Editor");
    EXPECT_EQ(windowRaw->width, 1280);
    EXPECT_EQ(windowRaw->height, 720);
    EXPECT_FALSE(windowRaw->shown);
    EXPECT_GE(windowRaw->layoutApplyCount, 1);
    EXPECT_TRUE(mode.GetShell()->GetRegisteredPanelTitles().empty());

    mode.Unmount();
    EXPECT_FALSE(mode.IsMounted());
}

TEST(EditorWorkspaceSessionTest, RejectsInvalidInputsAndDoubleMount)
{
    EditorWorkspaceSession mode;
    EditorWorkspaceSessionConfig config;
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

TEST(EditorWorkspaceSessionTest, AuthoringModeSwitchPreservesCanonicalProjectSource)
{
    const std::filesystem::path root = TestSourceRoot() / "Samples/StarterArena";
    const std::vector<std::filesystem::path> canonical = {
        root / "StarterArena.sagaproj",
        root / "Scenes/StarterArena.sagascene",
        root / "Input/Input.saga.json",
        root / "Scripts/DoorController.cs",
        root / "SagaPackage.lock",
    };
    std::vector<std::string> before;
    for (const auto& path : canonical)
        before.push_back(ReadFile(path));

    EditorWorkspaceSession session;
    EditorWorkspaceSessionConfig config;
    config.projectPath = (root / "StarterArena.sagaproj").string();
    config.workspacePath = root.string();
    config.registerDefaultPanels = false;
    config.showOnMount = false;
    ASSERT_TRUE(session.Mount(std::make_unique<FakeMainWindow>(),
                              MakeSettingsStore(), config));
    ASSERT_TRUE(session.GetHost()->GetAuthoringModeService().SetActive("level2"));
    ASSERT_TRUE(session.GetHost()->GetAuthoringModeService().SetActive("level3"));
    ASSERT_TRUE(session.GetHost()->GetAuthoringModeService().SetActive("level1"));
    session.Unmount();

    for (std::size_t index = 0; index < canonical.size(); ++index)
        EXPECT_EQ(ReadFile(canonical[index]), before[index]) << canonical[index];
}

} // namespace
