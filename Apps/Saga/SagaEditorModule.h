/// @file SagaEditorModule.h
/// @brief Same-process editor mode facade mounted by the Saga product shell.

#pragma once

#include "SagaProjectSystem.h"
#include "SagaSdeCompiler.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor
{
class EditorHost;
class EditorShell;
class IPanel;
} // namespace SagaEditor

namespace SagaProduct
{

/// Immutable project/session descriptor consumed by editor mode.
struct SagaPreparedProjectSession
{
    SagaProjectManifest project;      ///< Product-selected project.
    std::string         sessionLabel; ///< Local session status or empty.
};

/// Native UI mount handles supplied by the product shell.
struct SagaEditorNativeMount
{
    void* mainWindow = nullptr;
    void* centralStack = nullptr;
};

/// Same-process editor facade. It never owns QApplication or product lifecycle.
class SagaEditorModule
{
public:
    using CloseProjectCallback = std::function<void()>;

    SagaEditorModule();
    ~SagaEditorModule();

    /// Mount editor panels into the Saga-owned main window and central stack.
    [[nodiscard]] bool Activate(SagaEditorNativeMount mount,
                                SagaPreparedProjectSession session,
                                CloseProjectCallback onCloseProject,
                                std::string& outError);

    /// Unmount editor widgets and destroy editor services.
    void Shutdown();

    /// Run the real SDE compiler path and update visible diagnostics.
    [[nodiscard]] SagaSdeCompileResult ValidateAndCompile();

    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] const SagaPreparedProjectSession& Session() const noexcept;
    [[nodiscard]] std::vector<std::string> VisiblePanelList() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

using EditorModePanelProvider = std::function<void(SagaEditor::EditorShell&)>;

/// Register a product-editor-mode panel provider without binding product core to the panel owner.
void RegisterEditorModePanelProvider(EditorModePanelProvider provider);

/// Clear registered editor-mode panel providers for deterministic tests or shutdown.
void ClearEditorModePanelProviders();

} // namespace SagaProduct
