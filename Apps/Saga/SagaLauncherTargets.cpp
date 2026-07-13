/// @file SagaLauncherTargets.cpp

#include "SagaLauncherTargets.h"

#include "SagaSessionModel.h"

#include <algorithm>
#include <chrono>

namespace SagaProduct
{
namespace
{

[[nodiscard]] std::filesystem::path WithWindowsExecutableSuffix(std::filesystem::path path)
{
#if defined(_WIN32)
    if (path.extension().empty() && !std::filesystem::exists(path))
        path += ".exe";
#endif
    return path;
}

[[nodiscard]] std::filesystem::path FindDeveloperTool(std::filesystem::path start,
                                                      const std::filesystem::path& relative)
{
    for (start = std::filesystem::absolute(start); !start.empty(); start = start.parent_path())
    {
        const auto candidate = WithWindowsExecutableSuffix(start / relative);
        if (std::filesystem::is_regular_file(candidate))
            return candidate;
        if (start == start.root_path())
            break;
    }
    return {};
}

[[nodiscard]] bool ContainsPrivateComponent(const std::filesystem::path& path)
{
    return std::any_of(path.begin(), path.end(), [](const auto& component) {
        return component == ".saga-private";
    });
}

[[nodiscard]] SagaLauncherDiagnostic Diagnostic(
    const char* id,
    SagaLauncherDiagnosticSeverity severity,
    std::string message,
    std::optional<SagaLauncherActionId> action = std::nullopt)
{
    SagaLauncherDiagnostic result;
    result.diagnosticId = id;
    result.severity = severity;
    result.message = std::move(message);
    result.actionId = action;
    return result;
}

struct TargetDefinition
{
    SagaLauncherTargetKind kind;
    SagaLauncherActionId action;
    const char* label;
    const char* description;
    const char* owner;
    std::optional<SagaProcessTargetId> process;
};

constexpr TargetDefinition kDefinitions[] = {
    {SagaLauncherTargetKind::EditorTarget,
     SagaLauncherActionId::OpenEditor,
     "Open SagaEditor",
     "Open the selected project in the external Editor host.",
     "SagaEditor",
     SagaProcessTargetId::Editor},
    {SagaLauncherTargetKind::RuntimeStarterArenaSmokeTarget,
     SagaLauncherActionId::RuntimeStarterArenaSmoke,
     "StarterArena Runtime smoke",
     "Run the bounded 30-frame headless Runtime smoke.",
     "SagaRuntime",
     SagaProcessTargetId::Runtime},
    {SagaLauncherTargetKind::RuntimeStarterArenaPlayableTarget,
     SagaLauncherActionId::RuntimeStarterArenaPlayable,
     "StarterArena visible playable",
     "Run the bounded 30-frame synthetic-input visible path.",
     "SagaRuntime",
     SagaProcessTargetId::Runtime},
    {SagaLauncherTargetKind::FirstPlayableCheckTarget,
     SagaLauncherActionId::FirstPlayableCheck,
     "First-playable check",
     "Run the existing bounded first-playable evidence workflow.",
     "Saga",
     std::nullopt},
    {SagaLauncherTargetKind::SagascriptEvidenceTarget,
     SagaLauncherActionId::ViewSagaScriptEvidence,
     "SagaScript evidence",
     "Read known SagaScript evidence without claiming generic authoring.",
     "sagascript",
     std::nullopt},
    {SagaLauncherTargetKind::VisualBlocksEvidenceTarget,
     SagaLauncherActionId::ViewVisualBlocksEvidence,
     "Visual Blocks evidence",
     "Read descriptor/projection evidence produced by CLI workflows.",
     "Saga/sagascript",
     std::nullopt},
    {SagaLauncherTargetKind::PackageStatusTarget,
     SagaLauncherActionId::ViewPackageStatus,
     "Package status",
     "Read known package/distribution reports without running package scripts.",
     "distribution reports",
     std::nullopt},
    {SagaLauncherTargetKind::UnsupportedGenericRuntimeTarget,
     SagaLauncherActionId::UnsupportedGenericRuntime,
     "Generic Runtime",
     "Generic project Runtime execution is not implemented.",
     "SagaRuntime",
     std::nullopt},
    {SagaLauncherTargetKind::UnsupportedServerTarget,
     SagaLauncherActionId::UnsupportedServer,
     "Server",
     "Product dedicated-server execution is not implemented.",
     "Saga",
     std::nullopt},
    {SagaLauncherTargetKind::UnsupportedWorldServerTarget,
     SagaLauncherActionId::UnsupportedWorldServer,
     "World server",
     "No Product world-server target exists.",
     "Saga",
     std::nullopt},
    {SagaLauncherTargetKind::UnsupportedCloudCollaborationTarget,
     SagaLauncherActionId::UnsupportedCloudCollaboration,
     "Cloud collaboration",
     "Only local report metadata exists; cloud collaboration is not implemented.",
     "Saga",
     std::nullopt},
};

[[nodiscard]] bool IsUnsupported(SagaLauncherTargetKind kind)
{
    return kind == SagaLauncherTargetKind::UnsupportedGenericRuntimeTarget ||
           kind == SagaLauncherTargetKind::UnsupportedServerTarget ||
           kind == SagaLauncherTargetKind::UnsupportedWorldServerTarget ||
           kind == SagaLauncherTargetKind::UnsupportedCloudCollaborationTarget;
}

[[nodiscard]] const char* UnsupportedDiagnostic(SagaLauncherTargetKind kind)
{
    switch (kind)
    {
    case SagaLauncherTargetKind::UnsupportedGenericRuntimeTarget:
        return SagaLauncherTargetDiagnostics::GenericRuntimeUnsupported;
    case SagaLauncherTargetKind::UnsupportedServerTarget:
        return SagaProductDiagnostics::ServerExecutionUnsupported;
    case SagaLauncherTargetKind::UnsupportedWorldServerTarget:
        return SagaLauncherTargetDiagnostics::WorldServerUnsupported;
    case SagaLauncherTargetKind::UnsupportedCloudCollaborationTarget:
        return SagaLauncherTargetDiagnostics::CloudCollaborationUnsupported;
    default:
        return "Saga.Launcher.Target.Unsupported";
    }
}

} // namespace

SagaExecutableCatalog::SagaExecutableCatalog(std::filesystem::path productExecutable)
    : m_productExecutable(std::move(productExecutable))
{}

std::optional<std::filesystem::path> SagaExecutableCatalog::Resolve(SagaProcessTargetId id) const
{
    const std::filesystem::path bin = m_productExecutable.parent_path();
    std::filesystem::path candidate;
    switch (id)
    {
    case SagaProcessTargetId::Editor:
        candidate = bin / "SagaEditor";
        break;
    case SagaProcessTargetId::Runtime:
        candidate = bin / "SagaRuntime";
        break;
    case SagaProcessTargetId::SagaProject:
        candidate = bin / "sagaproject";
        if (!std::filesystem::is_regular_file(WithWindowsExecutableSuffix(candidate)))
            candidate = FindDeveloperTool(bin, "Tools/SagaProjectKit/sagaproject");
        break;
    case SagaProcessTargetId::SagaScript:
        candidate = bin / "sagascript";
        if (!std::filesystem::is_regular_file(WithWindowsExecutableSuffix(candidate)))
            candidate = FindDeveloperTool(bin, "Tools/SagaScript/sagascript");
        break;
    case SagaProcessTargetId::Forge:
        candidate = bin / "forge";
        break;
    }
    candidate = WithWindowsExecutableSuffix(std::move(candidate));
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(candidate, error);
    if (error || !std::filesystem::is_regular_file(canonical))
        return std::nullopt;
    return canonical;
}

bool SagaLauncherPathPolicy::IsContainedPath(const std::filesystem::path& root,
                                             const std::filesystem::path& candidate) noexcept
{
    if (root.empty() || candidate.empty())
        return false;
    std::error_code error;
    const auto normalizedRoot = std::filesystem::weakly_canonical(root, error);
    if (error)
        return false;
    error.clear();
    auto normalizedCandidate = std::filesystem::weakly_canonical(candidate, error);
    if (error)
    {
        error.clear();
        normalizedCandidate = std::filesystem::absolute(candidate, error).lexically_normal();
        if (error)
            return false;
    }
    const auto mismatch = std::mismatch(normalizedRoot.begin(),
                                        normalizedRoot.end(),
                                        normalizedCandidate.begin(),
                                        normalizedCandidate.end());
    return mismatch.first == normalizedRoot.end();
}

std::optional<std::filesystem::path> SagaLauncherPathPolicy::ActionReportDirectory(
    const SagaLauncherProjectSummary& project,
    SagaLauncherActionId action,
    std::string& error) const
{
    if (!project.valid || project.projectId.empty() || reportsRoot.empty() ||
        project.projectId == "." || project.projectId == ".." ||
        project.projectId == ".saga-private" || ContainsPrivateComponent(reportsRoot) ||
        ContainsPrivateComponent(evidenceRoot) ||
        project.projectId.find('/') != std::string::npos ||
        project.projectId.find('\\') != std::string::npos)
    {
        error = "Invalid project id or launcher report root.";
        return std::nullopt;
    }
    std::filesystem::path root = reportsRoot;
    std::filesystem::path candidate;
    if (action == SagaLauncherActionId::FirstPlayableCheck && !evidenceRoot.empty())
    {
        root = evidenceRoot;
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
        candidate = root / project.projectId / ("run-" + std::to_string(stamp));
    }
    else
        candidate = root / project.projectId / ToString(action);
    if (!IsContainedPath(root, candidate))
    {
        error = "Launcher report path escapes the approved root.";
        return std::nullopt;
    }
    return candidate;
}

SagaTargetResolver::SagaTargetResolver(SagaExecutableCatalog catalog,
                                       std::filesystem::path runtimeBridgeAssembly)
    : m_catalog(std::move(catalog)), m_runtimeBridgeAssembly(std::move(runtimeBridgeAssembly))
{}

std::vector<SagaLauncherTargetSummary> SagaTargetResolver::ResolveTargets(
    const std::optional<SagaLauncherProjectSummary>& project) const
{
    std::vector<SagaLauncherTargetSummary> result;
    const bool hasProject = project.has_value() && project->valid;
    const bool starterArena = hasProject && project->projectId == "starter-arena";
    for (const auto& definition : kDefinitions)
    {
        SagaLauncherTargetSummary target;
        target.targetKind = definition.kind;
        target.displayName = definition.label;
        target.owner = definition.owner;
        if (IsUnsupported(definition.kind))
        {
            target.availability = SagaLauncherActionAvailability::Disabled;
            target.status = SagaLauncherActionStatus::Unsupported;
            target.diagnostics.push_back(Diagnostic(UnsupportedDiagnostic(definition.kind),
                                                    SagaLauncherDiagnosticSeverity::Info,
                                                    definition.description,
                                                    definition.action));
            target.nonClaims.push_back(definition.description);
        }
        else if (definition.kind == SagaLauncherTargetKind::PackageStatusTarget ||
                 definition.kind == SagaLauncherTargetKind::SagascriptEvidenceTarget ||
                 definition.kind == SagaLauncherTargetKind::VisualBlocksEvidenceTarget)
        {
            target.availability = SagaLauncherActionAvailability::Available;
            target.status = SagaLauncherActionStatus::NotConfigured;
        }
        else
        {
            bool available = hasProject;
            if (definition.kind == SagaLauncherTargetKind::RuntimeStarterArenaSmokeTarget ||
                definition.kind == SagaLauncherTargetKind::RuntimeStarterArenaPlayableTarget ||
                definition.kind == SagaLauncherTargetKind::FirstPlayableCheckTarget)
                available = starterArena;
            if (definition.kind == SagaLauncherTargetKind::RuntimeStarterArenaPlayableTarget)
                available = available &&
                            std::filesystem::is_regular_file(project->canonicalRoot / "Input" /
                                                             "playable.synthetic-input.json");
            if (definition.kind == SagaLauncherTargetKind::FirstPlayableCheckTarget)
                available = available &&
                            m_catalog.Resolve(SagaProcessTargetId::Runtime).has_value() &&
                            m_catalog.Resolve(SagaProcessTargetId::SagaScript).has_value() &&
                            std::filesystem::is_regular_file(m_runtimeBridgeAssembly);
            if (definition.process.has_value())
                available = available && m_catalog.Resolve(*definition.process).has_value();
            target.availability = available ? SagaLauncherActionAvailability::Available
                                            : SagaLauncherActionAvailability::Disabled;
            target.status = available ? SagaLauncherActionStatus::Available
                                      : SagaLauncherActionStatus::MissingInput;
            if (!available)
            {
                target.diagnostics.push_back(Diagnostic(
                    SagaLauncherTargetDiagnostics::ExecutableMissing,
                    SagaLauncherDiagnosticSeverity::Info,
                    hasProject ? "Required executable or StarterArena input is unavailable."
                               : "Select a valid project first.",
                    definition.action));
            }
        }
        result.push_back(std::move(target));
    }
    return result;
}

std::vector<SagaLauncherAction> SagaTargetResolver::ResolveActions(
    const std::optional<SagaLauncherProjectSummary>& project) const
{
    const auto targets = ResolveTargets(project);
    std::vector<SagaLauncherAction> result;
    SagaLauncherAction open;
    open.id = SagaLauncherActionId::OpenProject;
    open.label = "Open Project";
    open.description = "Select a canonical .sagaproj project.";
    open.owner = "Saga";
    open.availability = SagaLauncherActionAvailability::Available;
    open.status = SagaLauncherActionStatus::Available;
    open.canRun = true;
    result.push_back(std::move(open));

    SagaLauncherAction validate;
    validate.id = SagaLauncherActionId::ValidateProject;
    validate.label = "Validate Project";
    validate.description = "Run authoritative sagaproject validation.";
    validate.owner = "sagaproject";
    validate.processTargetId = SagaProcessTargetId::SagaProject;
    validate.requiredInputKinds = {"selected-project", "sagaproject"};
    const bool canValidate = project.has_value() && project->valid &&
                             m_catalog.Resolve(SagaProcessTargetId::SagaProject).has_value();
    validate.availability = canValidate ? SagaLauncherActionAvailability::Available
                                        : SagaLauncherActionAvailability::Disabled;
    validate.status = canValidate ? SagaLauncherActionStatus::Available
                                  : SagaLauncherActionStatus::MissingInput;
    validate.canRun = canValidate;
    result.push_back(std::move(validate));

    for (std::size_t i = 0; i < std::size(kDefinitions); ++i)
    {
        const auto& definition = kDefinitions[i];
        const auto& target = targets[i];
        SagaLauncherAction action;
        action.id = definition.action;
        action.label = definition.label;
        action.description = definition.description;
        action.owner = definition.owner;
        action.processTargetId = definition.process;
        action.availability = target.availability;
        action.status = target.status;
        action.diagnostics = target.diagnostics;
        action.nonClaims = target.nonClaims;
        action.canRun = target.availability == SagaLauncherActionAvailability::Available &&
                        (definition.process.has_value() ||
                         definition.action == SagaLauncherActionId::FirstPlayableCheck);
        result.push_back(std::move(action));
    }

    SagaLauncherAction refresh;
    refresh.id = SagaLauncherActionId::RefreshReports;
    refresh.label = "Refresh Reports";
    refresh.description = "Reload known report and distribution evidence.";
    refresh.owner = "Saga";
    refresh.availability = SagaLauncherActionAvailability::Available;
    refresh.status = SagaLauncherActionStatus::Available;
    refresh.canRun = true;
    result.push_back(std::move(refresh));

    SagaLauncherAction workspace;
    workspace.id = SagaLauncherActionId::ViewLocalWorkspaceEvidence;
    workspace.label = "Local workspace evidence";
    workspace.description = "Read local transaction/collaboration metadata only.";
    workspace.owner = "Saga";
    workspace.availability = SagaLauncherActionAvailability::Available;
    workspace.status = SagaLauncherActionStatus::NotConfigured;
    result.push_back(std::move(workspace));
    return result;
}

} // namespace SagaProduct
