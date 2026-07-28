// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Extensions/ExtensionHost.h"
#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/IEditorExtension.h"
#include "SagaEditor/Extensions/IExtensionContext.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SagaEditor {
namespace {

class TestExtension final : public IEditorExtension
{
public:
    TestExtension(std::string id, std::string name)
        : m_id(std::move(id)), m_name(std::move(name)) {}

    std::string GetExtensionId() const override { return m_id; }
    std::string GetDisplayName() const override { return m_name; }
    std::string GetVersion() const override { return "1.0.0"; }
    void OnLoad(IExtensionContext&) override {}
    void OnUnload(IExtensionContext&) override {}

private:
    std::string m_id;
    std::string m_name;
};

class FakeMainWindow final : public IUIMainWindow
{
public:
    void Show() override {}
    void ShowMaximized() override {}
    void Hide() override {}
    void Close() override {}
    void SetTitle(const std::string&) override {}
    void SetSize(int width, int height) override
    {
        m_width = width;
        m_height = height;
    }
    [[nodiscard]] int GetWidth() const noexcept override { return m_width; }
    [[nodiscard]] int GetHeight() const noexcept override { return m_height; }
    void ApplyShellLayout(const ShellLayout&) override {}
    void SetStatusMessage(const std::string&) override {}
    void DockPanel(QWidget*,
                   const std::string& panelId,
                   const std::string&,
                   UIDockArea) override
    {
        docked.push_back(panelId);
    }
    void UndockPanel(const std::string& panelId) override
    {
        undocked.push_back(panelId);
    }
    void FocusPanel(const std::string&) override {}
    void SetPanelVisible(const std::string&, bool) override {}
    [[nodiscard]] std::vector<uint8_t> SaveState() const override { return {}; }
    [[nodiscard]] bool RestoreState(const std::vector<uint8_t>&) override
    {
        return true;
    }
    void SetOnClose(CloseCallback callback) override
    {
        onClose = std::move(callback);
    }
    void SetOnCommand(CommandCallback callback) override
    {
        onCommand = std::move(callback);
    }

    int m_width = 0;
    int m_height = 0;
    std::vector<std::string> docked;
    std::vector<std::string> undocked;
    CloseCallback onClose;
    CommandCallback onCommand;
};

struct PanelLifecycle
{
    int initCount = 0;
    int shutdownCount = 0;
};

class TestPanel final : public IPanel
{
public:
    TestPanel(std::string id, PanelLifecycle& lifecycle)
        : m_id(std::move(id)), m_lifecycle(lifecycle)
    {}

    [[nodiscard]] PanelId GetPanelId() const override { return m_id; }
    [[nodiscard]] std::string GetTitle() const override { return m_id; }
    [[nodiscard]] QWidget* Widget() const noexcept override { return nullptr; }
    void OnInit() override { ++m_lifecycle.initCount; }
    void OnShutdown() override { ++m_lifecycle.shutdownCount; }

private:
    std::string m_id;
    PanelLifecycle& m_lifecycle;
};

class LifecycleExtension final : public IEditorExtension
{
public:
    using Callback = std::function<void(IExtensionContext&)>;

    LifecycleExtension(std::string id, Callback load, Callback unload)
        : m_id(std::move(id))
        , m_load(std::move(load))
        , m_unload(std::move(unload))
    {}

    [[nodiscard]] std::string GetExtensionId() const override { return m_id; }
    [[nodiscard]] std::string GetDisplayName() const override { return m_id; }
    [[nodiscard]] std::string GetVersion() const override { return "1.0.0"; }
    void OnLoad(IExtensionContext& context) override
    {
        ++loadCount;
        retainedContext = &context;
        if (m_load) m_load(context);
    }
    void OnUnload(IExtensionContext& context) override
    {
        ++unloadCount;
        if (m_unload) m_unload(context);
    }

    int loadCount = 0;
    int unloadCount = 0;
    IExtensionContext* retainedContext = nullptr;

private:
    std::string m_id;
    Callback m_load;
    Callback m_unload;
};

class ExtensionHostTests : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));
        auto ownedWindow = std::make_unique<FakeMainWindow>();
        window = ownedWindow.get();
        EditorShellConfig config;
        config.registerDefaultPanels = false;
        config.showOnInit = false;
        ASSERT_TRUE(shell.Init(host, std::move(ownedWindow), config));
    }

    void TearDown() override
    {
        host.GetExtensionHost().ShutdownAll();
        shell.Shutdown();
        host.Shutdown();
    }

    EditorHost host;
    EditorShell shell;
    FakeMainWindow* window = nullptr;
};

TEST(ExtensionRegistryTests, PreservesInsertionOrderAndSupportsLookup)
{
    ExtensionRegistry registry;
    registry.Register(std::make_unique<TestExtension>("tools.first", "First"));
    registry.Register(std::make_unique<TestExtension>("tools.second", "Second"));

    ASSERT_NE(registry.Find("tools.first"), nullptr);
    EXPECT_EQ(registry.Find("tools.first")->GetDisplayName(), "First");
    EXPECT_EQ(registry.Find("missing"), nullptr);

    const auto all = registry.GetAll();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0]->GetExtensionId(), "tools.first");
    EXPECT_EQ(all[1]->GetExtensionId(), "tools.second");
}

TEST(ExtensionRegistryTests, ReplacementDoesNotDuplicateOrderEntry)
{
    ExtensionRegistry registry;
    registry.Register(std::make_unique<TestExtension>("tools.same", "Old"));
    registry.Register(std::make_unique<TestExtension>("tools.same", "New"));

    const auto all = registry.GetAll();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all.front()->GetDisplayName(), "New");

    registry.Unregister("tools.same");
    EXPECT_TRUE(registry.GetAll().empty());
    EXPECT_EQ(registry.Find("tools.same"), nullptr);
}

TEST_F(ExtensionHostTests, AutomaticallyCleansPanelsAndCommandsOnUnload)
{
    PanelLifecycle panel;
    auto extension = std::make_unique<LifecycleExtension>(
        "tools.lifecycle",
        [&](IExtensionContext& context)
        {
            context.RegisterPanel(
                std::make_unique<TestPanel>("tools.panel", panel),
                DockArea::Left);
            context.RegisterCommand({
                "tools.command", "Command", "Tools", [] {}, true});
        },
        LifecycleExtension::Callback{});
    auto* extensionRaw = extension.get();
    host.GetExtensionRegistry().Register(std::move(extension));

    host.GetExtensionHost().Load("tools.lifecycle");
    host.GetExtensionHost().Load("tools.lifecycle");
    EXPECT_EQ(extensionRaw->loadCount, 1);
    EXPECT_EQ(panel.initCount, 1);
    EXPECT_NE(shell.FindPanel("tools.panel"), nullptr);
    EXPECT_NE(host.GetCommandRegistry().Find("tools.command"), nullptr);
    EXPECT_EQ(window->docked, std::vector<std::string>{"tools.panel"});

    host.GetExtensionHost().Unload("tools.lifecycle");
    host.GetExtensionHost().Unload("tools.lifecycle");
    EXPECT_EQ(extensionRaw->unloadCount, 1);
    EXPECT_EQ(panel.shutdownCount, 1);
    EXPECT_EQ(shell.FindPanel("tools.panel"), nullptr);
    EXPECT_EQ(host.GetCommandRegistry().Find("tools.command"), nullptr);
    EXPECT_EQ(window->undocked, std::vector<std::string>{"tools.panel"});
}

TEST_F(ExtensionHostTests, ExplicitUnregisterMakesAutomaticCleanupIdempotent)
{
    PanelLifecycle panel;
    auto extension = std::make_unique<LifecycleExtension>(
        "tools.explicit",
        [&](IExtensionContext& context)
        {
            context.RegisterPanel(
                std::make_unique<TestPanel>("tools.explicit.panel", panel));
            context.RegisterCommand({
                "tools.explicit.command", "Command", "Tools", [] {}, true});
        },
        [](IExtensionContext& context)
        {
            context.UnregisterCommand("tools.explicit.command");
            context.UnregisterPanel("tools.explicit.panel");
        });
    host.GetExtensionRegistry().Register(std::move(extension));

    host.GetExtensionHost().Load("tools.explicit");
    host.GetExtensionHost().Unload("tools.explicit");

    EXPECT_EQ(panel.shutdownCount, 1);
    EXPECT_EQ(window->undocked,
              std::vector<std::string>{"tools.explicit.panel"});
    EXPECT_EQ(host.GetCommandRegistry().Find("tools.explicit.command"),
              nullptr);
}

TEST_F(ExtensionHostTests, ShellShutdownUnloadsExtensionsBeforeDestroyingPanels)
{
    PanelLifecycle panel;
    auto extension = std::make_unique<LifecycleExtension>(
        "tools.shell_shutdown",
        [&](IExtensionContext& context)
        {
            context.RegisterPanel(std::make_unique<TestPanel>(
                "tools.shell_shutdown.panel", panel));
            context.RegisterCommand({
                "tools.shell_shutdown.command",
                "Command",
                "Tools",
                [] {},
                true});
        },
        LifecycleExtension::Callback{});
    auto* extensionRaw = extension.get();
    host.GetExtensionRegistry().Register(std::move(extension));

    host.GetExtensionHost().Load("tools.shell_shutdown");
    shell.Shutdown();

    EXPECT_EQ(extensionRaw->unloadCount, 1);
    EXPECT_EQ(panel.shutdownCount, 1);
    EXPECT_EQ(
        host.GetCommandRegistry().Find("tools.shell_shutdown.command"),
        nullptr);
}

TEST_F(ExtensionHostTests, ShutdownUsesReverseLoadOrderAndKeepsOwnersIsolated)
{
    PanelLifecycle firstPanel;
    PanelLifecycle secondPanel;
    std::vector<std::string> unloadOrder;
    const auto registerExtension =
        [&](std::string id, std::string panelId, std::string commandId,
            PanelLifecycle& lifecycle)
    {
        host.GetExtensionRegistry().Register(
            std::make_unique<LifecycleExtension>(
                id,
                [&, panelId, commandId](IExtensionContext& context)
                {
                    context.RegisterPanel(
                        std::make_unique<TestPanel>(panelId, lifecycle));
                    context.RegisterCommand({
                        commandId, commandId, "Tools", [] {}, true});
                },
                [&, id](IExtensionContext&) { unloadOrder.push_back(id); }));
    };
    registerExtension(
        "tools.first", "tools.first.panel", "tools.first.command", firstPanel);
    registerExtension(
        "tools.second", "tools.second.panel", "tools.second.command", secondPanel);

    host.GetExtensionHost().LoadAll();
    host.GetExtensionHost().Unload("tools.first");
    EXPECT_EQ(host.GetCommandRegistry().Find("tools.first.command"), nullptr);
    EXPECT_NE(host.GetCommandRegistry().Find("tools.second.command"), nullptr);
    EXPECT_EQ(shell.FindPanel("tools.first.panel"), nullptr);
    EXPECT_NE(shell.FindPanel("tools.second.panel"), nullptr);

    host.GetExtensionHost().ShutdownAll();
    EXPECT_EQ(unloadOrder,
              (std::vector<std::string>{"tools.first", "tools.second"}));
    EXPECT_EQ(firstPanel.shutdownCount, 1);
    EXPECT_EQ(secondPanel.shutdownCount, 1);
}

TEST_F(ExtensionHostTests, RejectsRetainedContextMutationOutsideLifecycle)
{
    auto extension = std::make_unique<LifecycleExtension>(
        "tools.retained",
        LifecycleExtension::Callback{},
        LifecycleExtension::Callback{});
    auto* extensionRaw = extension.get();
    host.GetExtensionRegistry().Register(std::move(extension));
    host.GetExtensionHost().Load("tools.retained");
    ASSERT_NE(extensionRaw->retainedContext, nullptr);

    extensionRaw->retainedContext->RegisterCommand({
        "tools.late.command", "Late", "Tools", [] {}, true});

    EXPECT_EQ(host.GetCommandRegistry().Find("tools.late.command"), nullptr);
    const auto& diagnostics = host.GetEditorDiagnosticsService().GetAll();
    const auto diagnostic = std::find_if(
        diagnostics.begin(), diagnostics.end(),
        [](const EditorDiagnostic& item)
        {
            return item.code ==
                "Editor.Extension.RegistrationOutsideLifecycle";
        });
    ASSERT_NE(diagnostic, diagnostics.end());
    EXPECT_EQ(diagnostic->source, "editor.extension.host");
}

TEST(ExtensionHostLifecycleTests, ReportsUnavailableShellWithoutLosingCommandCleanup)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));
    PanelLifecycle panel;
    host.GetExtensionRegistry().Register(
        std::make_unique<LifecycleExtension>(
            "tools.headless",
            [&](IExtensionContext& context)
            {
                context.RegisterPanel(
                    std::make_unique<TestPanel>("tools.headless.panel", panel));
                context.RegisterCommand({
                    "tools.headless.command", "Command", "Tools", [] {}, true});
            },
            LifecycleExtension::Callback{}));

    host.GetExtensionHost().Load("tools.headless");
    EXPECT_EQ(panel.initCount, 0);
    EXPECT_NE(host.GetCommandRegistry().Find("tools.headless.command"), nullptr);
    const auto& diagnostics = host.GetEditorDiagnosticsService().GetAll();
    EXPECT_NE(std::find_if(
                  diagnostics.begin(), diagnostics.end(),
                  [](const EditorDiagnostic& item)
                  {
                      return item.code == "Editor.Extension.ShellUnavailable";
                  }),
              diagnostics.end());

    host.GetExtensionHost().ShutdownAll();
    EXPECT_EQ(host.GetCommandRegistry().Find("tools.headless.command"), nullptr);
    host.Shutdown();
}

} // namespace
} // namespace SagaEditor
