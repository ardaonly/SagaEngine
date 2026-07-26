// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>
#include <string>

namespace SagaEditor
{
class CommandDispatcher;
class EditorDocumentWorkspace;
class UndoRedoStack;
class ScriptAnalysisCoordinator;
class CSharpSourceDocument;

/// Editor-owned seam for intentional source-buffer replacement.
class IScriptSourceMutationPort
{
public:
    virtual ~IScriptSourceMutationPort() = default;
    [[nodiscard]] virtual bool ReplaceText(CSharpSourceDocument& document,
        UndoRedoStack& journal, std::string text) = 0;
};

class ScriptingPanel final : public IPanel
{
public:
    ScriptingPanel();
    ~ScriptingPanel() override;
    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;
    void OnInit() override;
    void OnShutdown() override;

    void SetDocumentWorkspace(EditorDocumentWorkspace* workspace);
    void SetUndoRedoStack(UndoRedoStack* journal) noexcept;
    void SetCommandDispatcher(CommandDispatcher* dispatcher) noexcept;
    void SetAnalysisCoordinator(ScriptAnalysisCoordinator* coordinator) noexcept;
    void SetScriptSourceMutationPort(IScriptSourceMutationPort* port) noexcept;
    void Analyze();
    void CancelAnalysis();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
