#pragma once

#include "SagaEditor/Authoring/ProjectBrowserWorkflowView.h"
#include "SagaEditor/Authoring/ScriptBehaviorInspectorView.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

enum class DiagnosticsPanelSeverity
{
    Critical,
    Warning,
    Info,
};

struct DiagnosticsPanelEntryView
{
    DiagnosticsPanelSeverity severity = DiagnosticsPanelSeverity::Info;
    std::string code;
    std::string message;
    std::filesystem::path path;
    std::string source;
};

struct DiagnosticsPanelGroupView
{
    DiagnosticsPanelSeverity severity = DiagnosticsPanelSeverity::Info;
    std::vector<DiagnosticsPanelEntryView> entries;
};

struct DiagnosticsPanelRefreshState
{
    bool recomputedFromFiles = false;
    bool writesFiles = false;
    std::string status;
};

struct DiagnosticsPanelView
{
    bool ok = false;
    std::filesystem::path diagnosticsSummaryPath;
    DiagnosticsPanelRefreshState refresh;
    DiagnosticsPanelGroupView critical;
    DiagnosticsPanelGroupView warning;
    DiagnosticsPanelGroupView info;
};

[[nodiscard]] const char* ToString(DiagnosticsPanelSeverity severity) noexcept;

[[nodiscard]] DiagnosticsPanelView LoadDiagnosticsPanelView(
    const ProjectReadinessView& project);

[[nodiscard]] DiagnosticsPanelView LoadDiagnosticsPanelView(
    const ProjectBrowserWorkflowView& projectBrowser,
    const ScriptBehaviorInspectorLoadResult& inspector);

} // namespace SagaEditor::Authoring
