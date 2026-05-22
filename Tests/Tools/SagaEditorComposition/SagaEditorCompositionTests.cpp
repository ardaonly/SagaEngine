/// @file SagaEditorCompositionTests.cpp
/// @brief Tests for the SDE-adjacent saga.editor composition adapter.

#include "SagaEditorComposition/CompositionCompiler.h"

#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIApplication.h"
#include "SagaEditor/UI/IUIFactory.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include "SDE/Core/StableHash.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace
{

namespace fs = std::filesystem;

class FakeMainWindow final : public SagaEditor::IUIMainWindow
{
public:
    void Show() override { shown = true; }
    void ShowMaximized() override { maximized = true; shown = true; }
    void Hide() override { shown = false; }
    void Close() override
    {
        closed = true;
        if (onClose)
        {
            onClose();
        }
    }

    void SetTitle(const std::string& value) override { title = value; }
    void SetSize(int w, int h) override { width = w; height = h; }

    [[nodiscard]] int GetWidth() const noexcept override { return width; }
    [[nodiscard]] int GetHeight() const noexcept override { return height; }
    [[nodiscard]] void* GetNativeHandle() const noexcept override { return nullptr; }

    void ApplyShellLayout(const SagaEditor::ShellLayout& value) override
    {
        layout = value;
        ++layoutApplyCount;
    }

    void SetStatusMessage(const std::string& value) override { status = value; }

    void DockPanel(void*,
                   const std::string& panelId,
                   const std::string&,
                   SagaEditor::UIDockArea area) override
    {
        visiblePanels[panelId] = true;
        dockAreas[panelId] = area;
    }

    void UndockPanel(const std::string& panelId) override
    {
        visiblePanels.erase(panelId);
    }

    void FocusPanel(const std::string& panelId) override
    {
        focusedPanels.push_back(panelId);
    }

    void SetPanelVisible(const std::string& panelId, bool visible) override
    {
        visiblePanels[panelId] = visible;
    }

    [[nodiscard]] std::vector<uint8_t> SaveState() const override { return {}; }
    [[nodiscard]] bool RestoreState(const std::vector<uint8_t>&) override { return true; }

    void SetOnClose(CloseCallback cb) override { onClose = std::move(cb); }
    void SetOnCommand(CommandCallback cb) override { onCommand = std::move(cb); }

    int width = 1600;
    int height = 900;
    bool shown = false;
    bool maximized = false;
    bool closed = false;
    std::string title;
    std::string status;
    SagaEditor::ShellLayout layout;
    int layoutApplyCount = 0;
    std::unordered_map<std::string, bool> visiblePanels;
    std::unordered_map<std::string, SagaEditor::UIDockArea> dockAreas;
    std::vector<std::string> focusedPanels;
    CloseCallback onClose;
    CommandCallback onCommand;
};

class FakeUIFactory final : public SagaEditor::IUIFactory
{
public:
    [[nodiscard]] std::unique_ptr<SagaEditor::IUIApplication>
    CreateApplication(int&, char**) override
    {
        return nullptr;
    }

    [[nodiscard]] std::unique_ptr<SagaEditor::IUIMainWindow>
    CreateMainWindow(const std::string&, int, int) override
    {
        return std::make_unique<FakeMainWindow>();
    }

    [[nodiscard]] std::unique_ptr<SagaEditor::IEditorSettingsStore>
    CreateSettingsStore() override
    {
        return std::make_unique<SagaEditor::MemoryEditorSettingsStore>();
    }
};

class FakePanel final : public SagaEditor::IPanel
{
public:
    FakePanel(std::string panelId, std::string titleText)
        : id(std::move(panelId)), title(std::move(titleText))
    {}

    [[nodiscard]] SagaEditor::PanelId GetPanelId() const override { return id; }
    [[nodiscard]] std::string GetTitle() const override { return title; }
    [[nodiscard]] void* GetNativeWidget() const noexcept override { return nullptr; }

    std::string id;
    std::string title;
};

[[nodiscard]] fs::path MakeWorkspace(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root / "source");
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << text;
}

[[nodiscard]] std::string ReadFile(const fs::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::string SchemaSource()
{
    return ReadFile(fs::path(SAGA_SOURCE_ROOT) / "Schemas" / "Saga" /
                    "Editor" / "saga.editor.schema.sde");
}

[[nodiscard]] fs::path ProductCompositionWorkspace()
{
    return fs::path(SAGA_SOURCE_ROOT) / "Editor" / "CompositionSources";
}

[[nodiscard]] SagaEditorComposition::CompositionOutputPaths
ProductOutputContract(const fs::path& projectRoot)
{
    return SagaEditorComposition::MakeBuildOutputPaths(projectRoot / "Build");
}

[[nodiscard]] SagaEditorComposition::CompositionCompileResult
CompileCheckedInProductComposition(const fs::path& projectRoot)
{
    fs::remove_all(projectRoot);

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = ProductCompositionWorkspace();
    request.compositionId = "saga.editor.default";
    request.buildRoot = projectRoot / "Build";
    return compiler.Compile(request);
}

[[nodiscard]] std::string ValidCompositionSource()
{
    return R"(sde version 1

package saga.editor.test
import saga.editor

instance EditorComposition saga.editor.default {
  displayName = "Saga Default Editor"
  description = "Default composition emitted by the adapter test."
}

instance EditorPanel saga.panel.hierarchy {
  composition = saga.editor.default
  displayName = "Hierarchy"
  kind = "builtin"
  defaultVisible = true
  internalOnly = false
  allowedProfiles = ["default"]
}

instance EditorPanel saga.panel.internal.serviceGraph {
  composition = saga.editor.default
  displayName = "Service Graph"
  kind = "builtin"
  defaultVisible = false
  internalOnly = true
}

instance EditorAction saga.action.workspace.reset {
  composition = saga.editor.default
  displayName = "Reset Workspace"
  category = "Workspace"
  safeInProduct = true
  internalOnly = false
}

instance EditorMenu saga.menu.workspace {
  composition = saga.editor.default
  displayName = "Workspace"
}

instance EditorMenuItem saga.menu.workspace.reset {
  menu = saga.menu.workspace
  kind = "action"
  actionId = saga.action.workspace.reset
}

instance EditorToolbar saga.toolbar.main {
  composition = saga.editor.default
  displayName = "Main Toolbar"
  actions = [saga.action.workspace.reset]
}

instance EditorShortcut saga.shortcut.workspace.reset {
  composition = saga.editor.default
  actionId = saga.action.workspace.reset
  chord = "Ctrl+Shift+R"
}

instance EditorWorkspaceLayout saga.layout.default {
  composition = saga.editor.default
  displayName = "Default Layout"
}

instance EditorWorkspace saga.workspace.default {
  composition = saga.editor.default
  displayName = "Default"
  layoutId = saga.layout.default
  visiblePanels = [saga.panel.hierarchy]
}

instance EditorMode saga.mode.scene {
  composition = saga.editor.default
  displayName = "Scene"
  workspaceId = saga.workspace.default
  requiredPanels = [saga.panel.hierarchy]
}
)";
}

void WriteValidWorkspace(const fs::path& root)
{
    WriteFile(root / "source" / "saga.editor.sde", SchemaSource());
    WriteFile(root / "source" / "composition.sde", ValidCompositionSource());
}

[[nodiscard]] bool HasDiagnostic(
    const SagaEditorComposition::CompositionCompileResult& result,
    const std::string& code)
{
    for (const SDE::Diagnostic& diagnostic : result.diagnostics)
    {
        if (diagnostic.code == code)
        {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(SagaEditorCompositionAdapterTest, CompilesValidSourceIntoArtifactSet)
{
    const fs::path root = MakeWorkspace("sde_saga_editor_valid");
    const fs::path out = root / "artifacts";
    WriteValidWorkspace(root);

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = root;
    request.outputRoot = out;
    request.compositionId = "saga.editor.default";

    const SagaEditorComposition::CompositionCompileResult result =
        compiler.Compile(request);

    EXPECT_TRUE(SDE::IsUsable(result.state));
    EXPECT_TRUE(fs::exists(out / "saga.editor.composition.json"));
    EXPECT_TRUE(fs::exists(out / "saga.editor.composition.manifest.json"));
    EXPECT_TRUE(fs::exists(out / "saga.editor.composition.diagnostics.json"));
    EXPECT_TRUE(fs::exists(out / "saga.editor.composition.sourcemap.json"));
    EXPECT_TRUE(fs::exists(out / "saga.editor.composition.dependencies.json"));

    const std::string artifactText =
        ReadFile(out / "saga.editor.composition.json");
    EXPECT_NE(artifactText.find("\"artifactKind\": \"saga.editor.composition\""),
              std::string::npos);
    EXPECT_NE(artifactText.find("\"compositionId\": \"saga.editor.default\""),
              std::string::npos);

    const std::string manifestText =
        ReadFile(out / "saga.editor.composition.manifest.json");
    EXPECT_NE(manifestText.find(SDE::StableHashBytes(artifactText)),
              std::string::npos);
}

TEST(SagaEditorCompositionAdapterTest, ProducesDeterministicArtifactText)
{
    const fs::path root = MakeWorkspace("sde_saga_editor_deterministic");
    WriteValidWorkspace(root);

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = root;
    request.compositionId = "saga.editor.default";

    request.outputRoot = root / "out-a";
    const SagaEditorComposition::CompositionCompileResult first =
        compiler.Compile(request);
    request.outputRoot = root / "out-b";
    const SagaEditorComposition::CompositionCompileResult second =
        compiler.Compile(request);

    ASSERT_TRUE(SDE::IsUsable(first.state));
    ASSERT_TRUE(SDE::IsUsable(second.state));
    EXPECT_EQ(ReadFile(root / "out-a" / "saga.editor.composition.json"),
              ReadFile(root / "out-b" / "saga.editor.composition.json"));
    EXPECT_EQ(first.hashes.artifactHash, second.hashes.artifactHash);
}

TEST(SagaEditorCompositionAdapterTest, SupportsExplicitForgeOutputContract)
{
    const fs::path root = MakeWorkspace("sde_saga_editor_explicit_outputs");
    const fs::path buildRoot = root / "Build";
    WriteValidWorkspace(root);

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = root;
    request.compositionId = "saga.editor.default";
    request.explicitOutputs = SagaEditorComposition::MakeBuildOutputPaths(buildRoot);

    const SagaEditorComposition::CompositionCompileResult result =
        compiler.Compile(request);

    ASSERT_TRUE(SDE::IsUsable(result.state));
    EXPECT_TRUE(fs::exists(request.explicitOutputs.artifactPath));
    EXPECT_TRUE(fs::exists(request.explicitOutputs.manifestPath));
    EXPECT_TRUE(fs::exists(request.explicitOutputs.diagnosticsPath));
    EXPECT_TRUE(fs::exists(request.explicitOutputs.sourceMapPath));
    EXPECT_TRUE(fs::exists(request.explicitOutputs.dependencyManifestPath));

    const std::string manifestText = ReadFile(request.explicitOutputs.manifestPath);
    EXPECT_NE(manifestText.find(
                  "\"artifactPath\": \"../Artifacts/EditorComposition/saga.editor.composition.json\""),
              std::string::npos);
    EXPECT_NE(manifestText.find(
                  "\"diagnosticsPath\": \"../Reports/saga.editor.composition.diagnostics.json\""),
              std::string::npos);
    EXPECT_NE(manifestText.find(
                  "\"sourceMapPath\": \"saga.editor.composition.sourcemap.json\""),
              std::string::npos);
    EXPECT_NE(manifestText.find(
                  "\"dependencyManifestPath\": \"saga.editor.composition.dependencies.json\""),
              std::string::npos);
}

TEST(SagaEditorCompositionAdapterTest, SupportsBuildRootOutputContract)
{
    const fs::path root = MakeWorkspace("sde_saga_editor_build_root_outputs");
    const fs::path buildRoot = root / "Build";
    WriteValidWorkspace(root);

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = root;
    request.compositionId = "saga.editor.default";
    request.buildRoot = buildRoot;

    const SagaEditorComposition::CompositionCompileResult result =
        compiler.Compile(request);
    const SagaEditorComposition::CompositionOutputPaths outputs =
        SagaEditorComposition::MakeBuildOutputPaths(buildRoot);

    ASSERT_TRUE(SDE::IsUsable(result.state));
    EXPECT_EQ(result.outputs.artifactPath, outputs.artifactPath);
    EXPECT_EQ(result.outputs.manifestPath, outputs.manifestPath);
    EXPECT_EQ(result.outputs.diagnosticsPath, outputs.diagnosticsPath);
    EXPECT_EQ(result.outputs.sourceMapPath, outputs.sourceMapPath);
    EXPECT_EQ(result.outputs.dependencyManifestPath, outputs.dependencyManifestPath);
    EXPECT_TRUE(fs::exists(outputs.artifactPath));
    EXPECT_TRUE(fs::exists(outputs.manifestPath));
    EXPECT_TRUE(fs::exists(outputs.diagnosticsPath));
    EXPECT_TRUE(fs::exists(outputs.sourceMapPath));
    EXPECT_TRUE(fs::exists(outputs.dependencyManifestPath));
}

TEST(SagaEditorCompositionAdapterTest, CompilesCheckedInProductWorkspace)
{
    const fs::path projectRoot =
        fs::temp_directory_path() / "saga_editor_product_composition_compile";

    const SagaEditorComposition::CompositionCompileResult result =
        CompileCheckedInProductComposition(projectRoot);
    const SagaEditorComposition::CompositionOutputPaths outputs =
        ProductOutputContract(projectRoot);

    ASSERT_TRUE(SDE::IsUsable(result.state));
    EXPECT_TRUE(fs::exists(outputs.artifactPath));
    EXPECT_TRUE(fs::exists(outputs.manifestPath));
    EXPECT_TRUE(fs::exists(outputs.diagnosticsPath));
    EXPECT_TRUE(fs::exists(outputs.sourceMapPath));
    EXPECT_TRUE(fs::exists(outputs.dependencyManifestPath));

    const std::string artifactText = ReadFile(outputs.artifactPath);
    EXPECT_NE(artifactText.find("\"compositionId\": \"saga.editor.default\""),
              std::string::npos);
    EXPECT_NE(artifactText.find("\"id\": \"saga.panel.production_dashboard\""),
              std::string::npos);
    EXPECT_NE(artifactText.find("\"id\": \"saga.toolbar.main\""),
              std::string::npos);

    const std::string manifestText = ReadFile(outputs.manifestPath);
    EXPECT_NE(manifestText.find(
                  "\"artifactPath\": \"../Artifacts/EditorComposition/saga.editor.composition.json\""),
              std::string::npos);
}

TEST(SagaEditorCompositionAdapterTest, ProductManifestFeedsEditorHostAndShell)
{
    const fs::path projectRoot =
        fs::temp_directory_path() / "saga_editor_product_composition_shell";

    const SagaEditorComposition::CompositionCompileResult result =
        CompileCheckedInProductComposition(projectRoot);
    const SagaEditorComposition::CompositionOutputPaths outputs =
        ProductOutputContract(projectRoot);
    ASSERT_TRUE(SDE::IsUsable(result.state));

    SagaEditor::EditorCompositionStartupConfig composition;
    composition.manifestPath = outputs.manifestPath;

    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(
        std::make_unique<SagaEditor::MemoryEditorSettingsStore>(),
        fs::path{},
        composition));
    ASSERT_TRUE(host.GetCompositionSession().IsUsable());
    ASSERT_TRUE(host.GetCompositionSession().Snapshot().has_value());
    EXPECT_EQ(host.GetCompositionSession().Snapshot()->compositionId,
              "saga.editor.default");

    FakeUIFactory factory;
    SagaEditor::EditorShell shell;
    SagaEditor::EditorShellConfig config;
    config.registerDefaultPanels = false;
    config.applyActivePersona = false;
    config.showOnInit = false;
    ASSERT_TRUE(shell.Init(host, factory, config));

    auto& window = dynamic_cast<FakeMainWindow&>(shell.GetMainWindow());
    ASSERT_FALSE(window.layout.menus.empty());
    const bool hasFileMenu =
        std::any_of(window.layout.menus.begin(),
                    window.layout.menus.end(),
                    [](const SagaEditor::MenuDescriptor& menu)
                    {
                        return menu.title == "File";
                    });
    EXPECT_TRUE(hasFileMenu);
    ASSERT_FALSE(window.layout.mainToolbarItems.empty());
    EXPECT_EQ(window.layout.mainToolbarItems[0].commandId,
              "saga.command.file.save");
    EXPECT_EQ(host.GetShortcutManager().GetBoundCommand({1u, 'S'}),
              "saga.command.file.save");

    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.production_dashboard",
                            "Production Dashboard"),
                        SagaEditor::UIDockArea::Left);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.hierarchy",
                            "Hierarchy"),
                        SagaEditor::UIDockArea::Left);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.viewport",
                            "World Viewport"),
                        SagaEditor::UIDockArea::Center);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.inspector",
                            "Inspector"),
                        SagaEditor::UIDockArea::Right);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.assetbrowser",
                            "Asset Browser"),
                        SagaEditor::UIDockArea::Bottom);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.console",
                            "Console"),
                        SagaEditor::UIDockArea::Bottom);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.problems",
                            "Problems"),
                        SagaEditor::UIDockArea::Bottom);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.customize_workspace",
                            "Customize Workspace"),
                        SagaEditor::UIDockArea::Right);
    shell.RegisterPanel(std::make_unique<FakePanel>(
                            "saga.panel.shortcut_preferences",
                            "Shortcut Preferences"),
                        SagaEditor::UIDockArea::Right);

    EXPECT_TRUE(window.visiblePanels["saga.panel.production_dashboard"]);
    EXPECT_TRUE(window.visiblePanels["saga.panel.hierarchy"]);
    EXPECT_TRUE(window.visiblePanels["saga.panel.viewport"]);
    EXPECT_TRUE(window.visiblePanels["saga.panel.inspector"]);
    EXPECT_TRUE(window.visiblePanels["saga.panel.assetbrowser"]);
    EXPECT_FALSE(window.visiblePanels["saga.panel.console"]);
    EXPECT_TRUE(window.visiblePanels["saga.panel.problems"]);

    const std::vector<SagaEditor::EditorDiagnostic>& diagnostics =
        host.GetEditorDiagnosticsService().GetAll();
    const bool hasMissingPanelDiagnostic =
        std::any_of(diagnostics.begin(),
                    diagnostics.end(),
                    [](const SagaEditor::EditorDiagnostic& diagnostic)
                    {
                        return diagnostic.source ==
                                   "editor.composition.shell" &&
                               diagnostic.code ==
                                   "CompositionShell.MissingPanelImplementation";
                    });
    EXPECT_FALSE(hasMissingPanelDiagnostic);

    shell.Shutdown();
    host.Shutdown();
}

TEST(SagaEditorCompositionAdapterTest, MissingToolbarActionFails)
{
    const fs::path root = MakeWorkspace("sde_saga_editor_missing_action");
    WriteFile(root / "source" / "saga.editor.sde", SchemaSource());
    WriteFile(root / "source" / "composition.sde",
              ValidCompositionSource() +
              R"(
instance EditorToolbar saga.toolbar.invalid {
  composition = saga.editor.default
  displayName = "Invalid Toolbar"
  actions = [saga.action.missing]
}
)");

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = root;
    request.outputRoot = root / "artifacts";
    request.compositionId = "saga.editor.default";

    const SagaEditorComposition::CompositionCompileResult result =
        compiler.Compile(request);

    EXPECT_FALSE(SDE::IsUsable(result.state));
    EXPECT_TRUE(HasDiagnostic(result, "SDE_EDITOR_COMPOSITION_UNKNOWN_ACTION"));
}

TEST(SagaEditorCompositionAdapterTest, MissingWorkspacePanelFails)
{
    const fs::path root = MakeWorkspace("sde_saga_editor_missing_panel");
    WriteFile(root / "source" / "saga.editor.sde", SchemaSource());
    WriteFile(root / "source" / "composition.sde",
              ValidCompositionSource() +
              R"(
instance EditorWorkspace saga.workspace.invalid {
  composition = saga.editor.default
  displayName = "Invalid"
  layoutId = saga.layout.default
  visiblePanels = [saga.panel.missing]
}
)");

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = root;
    request.outputRoot = root / "artifacts";
    request.compositionId = "saga.editor.default";

    const SagaEditorComposition::CompositionCompileResult result =
        compiler.Compile(request);

    EXPECT_FALSE(SDE::IsUsable(result.state));
    EXPECT_TRUE(HasDiagnostic(result, "SDE_EDITOR_COMPOSITION_UNKNOWN_PANEL"));
}

TEST(SagaEditorCompositionAdapterTest, MultipleRootsRequireCompositionId)
{
    const fs::path root = MakeWorkspace("sde_saga_editor_multiple_roots");
    WriteFile(root / "source" / "saga.editor.sde", SchemaSource());
    WriteFile(root / "source" / "composition.sde",
              ValidCompositionSource() +
              R"(
instance EditorComposition saga.editor.secondary {
  displayName = "Secondary"
}
)");

    SagaEditorComposition::CompositionCompiler compiler;
    SagaEditorComposition::CompositionCompileRequest request;
    request.workspaceRoot = root;
    request.outputRoot = root / "artifacts";

    const SagaEditorComposition::CompositionCompileResult result =
        compiler.Validate(request);

    EXPECT_FALSE(SDE::IsUsable(result.state));
    EXPECT_TRUE(HasDiagnostic(result, "SDE_EDITOR_COMPOSITION_DUPLICATE_ROOT"));
}
