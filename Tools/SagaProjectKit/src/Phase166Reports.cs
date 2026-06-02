using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class Phase166Reports
{
    private const string PlannedClientPreviewProfileId = "client-preview-local-no-network";
    private const string PlannedClientPreviewPackageId = "technical-preview-client-preview-local";

    private static readonly List<string> NonClaims =
    [
        "No Product Beta is claimed.",
        "No Release Candidate is claimed.",
        "No Production Readiness is claimed.",
        "No Playable Editor is claimed.",
        "No Client Preview implementation is claimed.",
        "No ClientHost implementation is claimed.",
        "No Runtime gameplay implementation is claimed.",
        "No Server gameplay implementation is claimed.",
        "No multiplayer proof is claimed.",
        "No network proof is claimed.",
        "ReadyForImplementationPlanning does not mean Client Preview is implemented.",
    ];

    private static readonly List<string> DeferredImplementationWork =
    [
        "Runtime read seam implementation",
        "Runtime read adapters",
        "ClientHost preview shell implementation",
        "Client Preview implementation",
        "Client Preview package/profile implementation",
        "Editor UI and Qt UI",
        "networking/session lifecycle",
        "asset import/cook",
    ];

    public static MinimalRuntimeReadSeamPlanReport BuildMinimalRuntimeReadSeamPlan(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var reports = ReadRequiredInputs(load, evidenceRoot, diagnostics, MinimalRuntimeReadSeamInputs());
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));

        return new MinimalRuntimeReadSeamPlanReport
        {
            Command = "minimal-runtime-read-seam-plan",
            Status = readiness,
            ReadinessStatus = readiness,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredReports = reports,
            FirstRuntimeConsumedSourceTruth =
            [
                new RuntimeReadInputItem { InputId = "validated-project-scene-source-truth", Kind = "SceneSourceTruth", Path = "Project-declared scene source truth", Classification = "FutureRuntimeConsumedSourceTruth", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "validated-entities", Kind = "EntitySourceTruth", Path = "Entities in project-declared scene source truth", Classification = "FutureRuntimeConsumedSourceTruth", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "component-metadata", Kind = "ComponentMetadata", Path = "Scene/entity component metadata", Classification = "FutureRuntimeConsumedSourceTruth", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "accepted-asset-references", Kind = "AssetReferenceSourceTruth", Path = "Accepted asset reference evidence", Classification = "FutureRuntimeConsumedSourceTruth", Status = "PlanOnly" },
            ],
            EvidenceOnlyGeneratedProjections =
            [
                new RuntimeReadInputItem { InputId = "non-canonical-generated-projection-metadata", Kind = "GeneratedProjectionEvidence", Path = "Generated freshness evidence", Classification = "EvidenceOnlyWhenFreshOrExplicitlyPartial", Status = "PlanOnly" },
            ],
            FutureAdapterBoundary =
            [
                Adapter("runtime-scene-source-truth-adapter", "runtime-readiness-v2", "validated project-declared scene source truth"),
                Adapter("runtime-entity-source-truth-adapter", "runtime-readiness-v2", "validated entities"),
                Adapter("runtime-component-metadata-adapter", "runtime-readiness-v2", "component metadata"),
                Adapter("runtime-accepted-asset-reference-adapter", "runtime-readiness-v2", "accepted asset references"),
                Adapter("runtime-generated-projection-evidence-adapter", "runtime-readiness-v2", "non-canonical generated projection evidence"),
                Adapter("runtime-freshness-evidence-adapter", "runtime-readiness-v2", "freshness evidence"),
            ],
            FutureTouchCandidates =
            [
                "Runtime read seam interface/model files",
                "Runtime read seam adapter files",
                "Focused Runtime read seam tests",
            ],
            ForbiddenTouchTargets =
            [
                "Server implementation",
                "Editor UI implementation",
                "Qt UI implementation",
                "Engine-wide behavior",
                "SagaScript patch behavior",
                "networking/session transport",
                "asset import/cook",
                "package staging",
                "launch execution",
            ],
            FutureProofTests =
            [
                "Runtime reads validated scene source truth through RuntimeReadSeamV1.",
                "Runtime reads validated entity/component metadata through RuntimeReadSeamV1.",
                "Runtime accepts only accepted asset references.",
                "Generated projections remain evidence-only and non-canonical.",
                "Missing or blocked evidence prevents the seam from being treated as ready.",
            ],
            BlockersBeforeImplementation =
            [
                "Runtime readiness v2 remains partial.",
                "Client Preview blocker matrix remains partial.",
                "Runtime read adapters are not implemented.",
                "ClientHost preview shell is not implemented.",
                "Client Preview is not implemented.",
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static MinimalClientHostPreviewShellPlanReport BuildMinimalClientHostPreviewShellPlan(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var reports = ReadRequiredInputs(load, evidenceRoot, diagnostics, MinimalClientHostShellInputs());
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));

        return new MinimalClientHostPreviewShellPlanReport
        {
            Command = "minimal-clienthost-preview-shell-plan",
            Status = readiness,
            ReadinessStatus = readiness,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredReports = reports,
            ClientHostFutureOwnership =
            [
                Ownership("ClientHost", "Local preview lifecycle shell", "ClientHostPreviewShellV1"),
                Ownership("ClientHost", "Diagnostics envelope", "ClientHostPreviewShellV1"),
                Ownership("ClientHost", "Runtime read seam handoff request", "ClientHostPreviewShellV1"),
                Ownership("ClientHost", "Local no-network preview state label", "ClientHostPreviewShellV1"),
            ],
            RuntimeFutureOwnership =
            [
                Ownership("Runtime", "Runtime read seam adapters", "RuntimeReadSeamV1"),
                Ownership("Runtime", "Scene/entity/component/asset reference reads", "RuntimeReadSeamV1"),
                Ownership("Runtime", "Validation of Runtime-readable inputs", "RuntimeReadSeamV1"),
            ],
            LaunchProfileFutureOwnership =
            [
                Ownership("LaunchProfile", "Select deferred client-preview-local-no-network profile", "ProfileContract"),
                Ownership("LaunchProfile", "Pass project and report paths only", "ProfileContract"),
            ],
            OutsideClientHost =
            [
                "scene/entity source truth ownership",
                "Runtime source truth",
                "server state",
                "sockets",
                "transport/session lifecycle",
                "rendering/UI",
                "asset import/cook outputs",
                "package/launch summaries as canonical state",
            ],
            StartupDiagnosticsWithoutGameplay =
            [
                "ClientHost shell constructed",
                "project manifest accepted",
                "Runtime read seam handoff prepared",
                "no gameplay loaded",
                "no network opened",
                "no server launched",
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static PackageLaunchPreviewAlignmentPlanReport BuildPackageLaunchPreviewAlignmentPlan(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var reports = ReadRequiredInputs(load, evidenceRoot, diagnostics, PackageLaunchInputs());
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));
        var launchProfiles = BuildLaunchProfileClassifications(load);
        var packageProfiles = BuildPackageProfileClassifications(load);

        return new PackageLaunchPreviewAlignmentPlanReport
        {
            Command = "package-launch-preview-alignment-plan",
            Status = readiness,
            ReadinessStatus = readiness,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredLaunchProfileId = PlannedClientPreviewProfileId,
            RequiredLaunchProfileStatus = launchProfiles.Any(item => item.ProfileId == PlannedClientPreviewProfileId) ? "DeferredProfileContractPresent" : "DeferredProfileNotImplemented",
            RequiredPackageProfileId = PlannedClientPreviewPackageId,
            RequiredPackageProfileStatus = packageProfiles.Any(item => item.ProfileId == PlannedClientPreviewPackageId) ? "DeferredProfileContractPresent" : "DeferredProfileNotImplemented",
            RequiredReports = reports,
            CurrentLaunchProfiles = launchProfiles,
            CurrentPackageProfiles = packageProfiles,
            RequiredReferencedEvidence =
            [
                Check("RuntimeReadinessV2", StatusFor(reports, "runtime-readiness-v2"), "Runtime readiness v2 must be referenced as planning evidence."),
                Check("ClientPreviewBlockerMatrix", StatusFor(reports, "client-preview-blocker-matrix"), "Client Preview blocker matrix must be referenced as planning evidence."),
                Check("MinimalRuntimeReadSeamPlan", StatusFor(reports, "minimal-runtime-read-seam-plan"), "Minimal Runtime read seam plan must be referenced before preview package/launch implementation."),
                Check("MinimalClientHostPreviewShellPlan", StatusFor(reports, "minimal-clienthost-preview-shell-plan"), "Minimal ClientHost preview shell plan must be referenced before preview package/launch implementation."),
                Check("PackageLaunchAlignmentReportOnly", "PlanOnly", "Package/source-truth and launch/source-truth alignment must remain report-only."),
            ],
            DeferredWork =
            [
                "package staging",
                "executable packaging",
                "LaunchLab execution",
                "Runtime process startup",
                "ClientHost process startup",
                "networking/session lifecycle",
                "asset import/cook",
            ],
            NotRuntimeProof =
            [
                "package summaries",
                "launch summaries",
                "server-headless profiles",
                "generated projections",
                "diagnostics shell reports",
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static EditorlessPreviewWorkflowPlanReport BuildEditorlessPreviewWorkflowPlan(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var reports = ReadRequiredInputs(load, evidenceRoot, diagnostics, EditorlessWorkflowInputs());
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));

        return new EditorlessPreviewWorkflowPlanReport
        {
            Command = "editorless-preview-workflow-plan",
            Status = readiness,
            ReadinessStatus = readiness,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredReports = reports,
            ConsumedInputs =
            [
                new RuntimeReadInputItem { InputId = "project-manifest", Kind = "SagaProjectManifest", Path = NormalizeRelative(load.ManifestPath), Classification = "FutureCliInput", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "project-declared-scene-source-truth", Kind = "SceneSourceTruth", Path = "Project-declared scene source truth", Classification = "FutureCliInput", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "asset-reference-evidence", Kind = "AssetReferenceEvidence", Path = "Build/Preview and Build/SourceTruth evidence", Classification = "FutureCliInput", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "generated-freshness-evidence", Kind = "GeneratedFreshnessEvidence", Path = "Build/Preview and Build/SourceTruth evidence", Classification = "FutureCliInput", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "preview-planning-reports", Kind = "PreviewPlanningReports", Path = NormalizeRelative(evidenceRoot), Classification = "FutureCliInput", Status = "PlanOnly" },
            ],
            FutureDiagnostics =
            [
                "manifest accepted",
                "preview profile selected",
                "no-network mode selected",
                "Runtime read seam handoff prepared",
                "ClientHost shell prepared",
                "no gameplay loaded",
                "no network opened",
                "no server launched",
            ],
            ImpossibleWithoutEditorUi =
            [
                "in-editor viewport",
                "interactive editing",
                "visual inspection UI",
                "Playable Editor workflow",
                "Qt UI panels",
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static PreviewEvidenceGateReport BuildPreviewEvidenceGate(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var reports = ReadRequiredInputs(load, evidenceRoot, diagnostics, PreviewGateInputs());
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));

        return new PreviewEvidenceGateReport
        {
            Command = "preview-evidence-gate",
            Status = readiness,
            ReadinessStatus = readiness,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredReports = reports,
            Checks =
            [
                Check("MissingRequiredEvidenceDoesNotPass", reports.Any(item => item.ReadinessStatus == "MissingEvidence") ? "MissingEvidence" : "Passed", "Missing required evidence must not pass."),
                Check("BlockedRequiredEvidenceDoesNotPass", reports.Any(item => item.ReadinessStatus == "Blocked") ? "Blocked" : "Passed", "Blocked required evidence must not pass."),
                Check("PartialEvidenceRemainsPartial", reports.Any(item => item.ReadinessStatus == "PartiallyProven") ? "PartiallyProven" : "Passed", "Partial planning evidence keeps the gate partial."),
                Check("ClientPreviewNotImplemented", "Passed", "Client Preview remains not implemented."),
                Check("ReportOnlyInvariants", "Passed", "The gate reads existing JSON reports and emits one report only."),
            ],
            DeferredImplementationWork = DeferredImplementationWork,
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static List<ReportSpec> MinimalRuntimeReadSeamInputs() =>
    [
        new("runtime-readiness-v2", "runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        new("client-preview-blocker-matrix", "client-preview-blocker-matrix", "client_preview_blocker_matrix_report.json"),
    ];

    private static List<ReportSpec> MinimalClientHostShellInputs() =>
    [
        new("runtime-readiness-v2", "runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        new("clienthost-preview-ownership-boundary", "clienthost-preview-ownership-boundary", "clienthost_preview_ownership_boundary_report.json"),
        new("client-preview-launch-profile-contract", "client-preview-launch-profile-contract", "client_preview_launch_profile_contract_report.json"),
        new("client-preview-no-network-plan", "client-preview-no-network-plan", "client_preview_no_network_plan_report.json"),
        new("client-preview-blocker-matrix", "client-preview-blocker-matrix", "client_preview_blocker_matrix_report.json"),
        new("minimal-runtime-read-seam-plan", "minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
    ];

    private static List<ReportSpec> PackageLaunchInputs() =>
    [
        new("runtime-readiness-v2", "runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        new("client-preview-launch-profile-contract", "client-preview-launch-profile-contract", "client_preview_launch_profile_contract_report.json"),
        new("client-preview-blocker-matrix", "client-preview-blocker-matrix", "client_preview_blocker_matrix_report.json"),
        new("minimal-runtime-read-seam-plan", "minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
        new("minimal-clienthost-preview-shell-plan", "minimal-clienthost-preview-shell-plan", "minimal_clienthost_preview_shell_plan_report.json"),
    ];

    private static List<ReportSpec> EditorlessWorkflowInputs() =>
    [
        new("runtime-readiness-v2", "runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        new("client-preview-no-network-plan", "client-preview-no-network-plan", "client_preview_no_network_plan_report.json"),
        new("client-preview-blocker-matrix", "client-preview-blocker-matrix", "client_preview_blocker_matrix_report.json"),
        new("minimal-runtime-read-seam-plan", "minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
        new("minimal-clienthost-preview-shell-plan", "minimal-clienthost-preview-shell-plan", "minimal_clienthost_preview_shell_plan_report.json"),
        new("package-launch-preview-alignment-plan", "package-launch-preview-alignment-plan", "package_launch_preview_alignment_plan_report.json"),
    ];

    private static List<ReportSpec> PreviewGateInputs() =>
    [
        new("runtime-readiness-v2", "runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        new("clienthost-preview-ownership-boundary", "clienthost-preview-ownership-boundary", "clienthost_preview_ownership_boundary_report.json"),
        new("client-preview-launch-profile-contract", "client-preview-launch-profile-contract", "client_preview_launch_profile_contract_report.json"),
        new("client-preview-no-network-plan", "client-preview-no-network-plan", "client_preview_no_network_plan_report.json"),
        new("client-preview-diagnostics-shell", "client-preview-diagnostics-shell", "client_preview_diagnostics_shell_report.json"),
        new("client-preview-blocker-matrix", "client-preview-blocker-matrix", "client_preview_blocker_matrix_report.json"),
        new("minimal-runtime-read-seam-plan", "minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
        new("minimal-clienthost-preview-shell-plan", "minimal-clienthost-preview-shell-plan", "minimal_clienthost_preview_shell_plan_report.json"),
        new("package-launch-preview-alignment-plan", "package-launch-preview-alignment-plan", "package_launch_preview_alignment_plan_report.json"),
        new("editorless-preview-workflow-plan", "editorless-preview-workflow-plan", "editorless_preview_workflow_plan_report.json"),
    ];

    private static List<LaunchProfileContractItem> BuildLaunchProfileClassifications(ManifestLoadResult load) =>
        (load.Descriptor?.LaunchProfiles ?? [])
            .OrderBy(item => item.Id, StringComparer.Ordinal)
            .Select(item =>
            {
                var isServerOnly = item.Id == "local-server-headless";
                var isPlannedClientPreview = item.Id == PlannedClientPreviewProfileId;
                return new LaunchProfileContractItem
                {
                    ProfileId = item.Id,
                    Kind = "launch",
                    Path = NormalizeRelative(item.Path),
                    Classification = isServerOnly ? "ServerHeadlessEvidenceOnly" :
                        isPlannedClientPreview ? "ClientPreviewNoNetworkContract" :
                        "UnknownLaunchEvidence",
                    Status = isServerOnly ? "NotClientPreview" :
                        isPlannedClientPreview ? "DeferredProfileContractPresent" :
                        "Unclassified",
                };
            })
            .ToList();

    private static List<LaunchProfileContractItem> BuildPackageProfileClassifications(ManifestLoadResult load) =>
        (load.Descriptor?.PackageProfiles ?? [])
            .OrderBy(item => item.Id, StringComparer.Ordinal)
            .Select(item =>
            {
                var isServerOnly = item.Id == "technical-preview-server-headless";
                var isPlannedClientPreview = item.Id == PlannedClientPreviewPackageId;
                return new LaunchProfileContractItem
                {
                    ProfileId = item.Id,
                    Kind = "package",
                    Path = NormalizeRelative(item.Path),
                    Classification = isServerOnly ? "ServerHeadlessPackageEvidenceOnly" :
                        isPlannedClientPreview ? "ClientPreviewPackageContract" :
                        "UnknownPackageEvidence",
                    Status = isServerOnly ? "NotClientPreview" :
                        isPlannedClientPreview ? "DeferredProfileContractPresent" :
                        "Unclassified",
                };
            })
            .ToList();

    private static List<ReportEvidenceStatusItem> ReadRequiredInputs(
        ManifestLoadResult load,
        string evidenceRoot,
        List<ProjectDiagnostic> diagnostics,
        IEnumerable<ReportSpec> specs)
    {
        var fullRoot = FullEvidenceRoot(load, evidenceRoot);
        var result = new List<ReportEvidenceStatusItem>();
        foreach (var spec in specs.OrderBy(item => item.Id, StringComparer.Ordinal))
        {
            var path = Path.Combine(fullRoot, spec.FileName);
            var report = ReadReport(load, path, spec.Command, diagnostics);
            result.Add(new ReportEvidenceStatusItem
            {
                EvidenceId = spec.Id,
                Command = spec.Command,
                Path = NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)),
                SourceStatus = report.SourceStatus,
                ReadinessStatus = report.ReadinessStatus,
                Required = true,
            });
        }

        return result
            .OrderBy(item => item.EvidenceId, StringComparer.Ordinal)
            .ToList();
    }

    private static ReportStatus ReadReport(
        ManifestLoadResult load,
        string reportPath,
        string expectedCommand,
        List<ProjectDiagnostic> diagnostics)
    {
        var fullPath = FullReportPath(load, reportPath);
        try
        {
            if (!File.Exists(fullPath))
            {
                diagnostics.Add(Error(
                    $"Project.Phase166_170.{expectedCommand}.Missing",
                    "ClientPreviewImplementationPathPlanning",
                    NormalizeRelative(reportPath),
                    $"Required report is missing: {expectedCommand}."));
                return new ReportStatus("Missing", "MissingEvidence");
            }

            var root = JsonNode.Parse(File.ReadAllText(fullPath)) as JsonObject;
            if (root is null)
            {
                diagnostics.Add(Error(
                    $"Project.Phase166_170.{expectedCommand}.Malformed",
                    "ClientPreviewImplementationPathPlanning",
                    NormalizeRelative(reportPath),
                    $"Required report JSON root must be an object: {expectedCommand}."));
                return new ReportStatus("Malformed", "MissingEvidence");
            }

            var command = ReadString(root, "command");
            if (!string.IsNullOrWhiteSpace(command) && command != expectedCommand)
            {
                diagnostics.Add(Error(
                    $"Project.Phase166_170.{expectedCommand}.UnexpectedCommand",
                    "ClientPreviewImplementationPathPlanning",
                    NormalizeRelative(reportPath),
                    $"Expected command {expectedCommand}, found {command}."));
                return new ReportStatus(command, "Blocked");
            }

            var status = FirstNonEmpty(
                ReadString(root, "readinessStatus"),
                FirstNonEmpty(ReadString(root, "status"), ReadString(root, "outcome")));
            if (string.IsNullOrWhiteSpace(status))
            {
                return new ReportStatus("Missing", "MissingEvidence");
            }

            return new ReportStatus(status, MapReadiness(status));
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Error(
                $"Project.Phase166_170.{expectedCommand}.Malformed",
                "ClientPreviewImplementationPathPlanning",
                NormalizeRelative(reportPath),
                $"Required report JSON could not be read: {e.Message}"));
            return new ReportStatus("Malformed", "MissingEvidence");
        }
    }

    private static RuntimeReadAdapterAuditItem Adapter(
        string adapterId,
        string sourceEvidenceId,
        string expectedInput) =>
        new()
        {
            AdapterId = adapterId,
            SourceEvidenceId = sourceEvidenceId,
            ExpectedInput = expectedInput,
            Status = "FutureAdapterBoundary",
        };

    private static PreviewOwnershipItem Ownership(string owner, string responsibility, string boundary) =>
        new()
        {
            Owner = owner,
            Responsibility = responsibility,
            Boundary = boundary,
            Status = "FuturePlan",
        };

    private static string AggregateReadiness(IEnumerable<string> statuses)
    {
        var values = statuses.ToArray();
        if (values.Any(item => item == "MissingEvidence"))
        {
            return "MissingEvidence";
        }
        if (values.Any(item => item == "Blocked"))
        {
            return "Blocked";
        }
        if (values.Any(item => item == "PartiallyProven"))
        {
            return "PartiallyProven";
        }
        return "ReadyForImplementationPlanning";
    }

    private static string MapReadiness(string status) =>
        status switch
        {
            "ReadyForImplementationPlanning" or "Passed" or "SourceTruthFoundationEstablished" or "ReadyForPlanning" => "ReadyForImplementationPlanning",
            "PartiallyProven" or "Deferred" or "Unclaimed" or "PlanOnly" => "PartiallyProven",
            "Blocked" or "Failed" => "Blocked",
            _ => "MissingEvidence",
        };

    private static string StatusFor(IEnumerable<ReportEvidenceStatusItem> evidence, string id) =>
        evidence.FirstOrDefault(item => item.EvidenceId == id)?.ReadinessStatus ?? "MissingEvidence";

    private static string FullReportPath(ManifestLoadResult load, string reportPath)
    {
        if (Path.IsPathRooted(reportPath))
        {
            return Path.GetFullPath(reportPath);
        }

        var projectRelative = Path.GetFullPath(Path.Combine(load.ProjectRoot, reportPath));
        if (File.Exists(projectRelative))
        {
            return projectRelative;
        }

        return Path.GetFullPath(reportPath);
    }

    private static string FullEvidenceRoot(ManifestLoadResult load, string evidenceRoot)
    {
        if (Path.IsPathRooted(evidenceRoot))
        {
            return Path.GetFullPath(evidenceRoot);
        }

        var projectRelative = Path.GetFullPath(Path.Combine(load.ProjectRoot, evidenceRoot));
        if (Directory.Exists(projectRelative))
        {
            return projectRelative;
        }

        return Path.GetFullPath(evidenceRoot);
    }

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";

    private static string FirstNonEmpty(string first, string second) =>
        string.IsNullOrWhiteSpace(first) ? second : first;

    private static SourceTruthGateCheck Check(string checkId, string status, string summary) =>
        new() { CheckId = checkId, Status = status, Summary = summary };

    private static ProjectSummary BuildProjectSummary(ManifestLoadResult load) =>
        new()
        {
            ProjectId = load.Descriptor?.ProjectId ?? "",
            DisplayName = load.Descriptor?.DisplayName ?? "",
            ProjectRoot = load.ProjectRoot,
            ManifestPath = load.ManifestPath,
            SchemaVersion = load.Descriptor?.SchemaVersion ?? 0,
        };

    private static ProjectDiagnostic Error(string code, string category, string path, string message) =>
        new() { Severity = "Error", Code = code, Category = category, Path = NormalizeRelative(path), Message = message };

    private static List<ProjectDiagnostic> SortDiagnostics(IEnumerable<ProjectDiagnostic> diagnostics) =>
        diagnostics
            .OrderByDescending(item => item.Severity == "Error")
            .ThenByDescending(item => item.Severity == "Warning")
            .ThenBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal)
            .ToList();

    private static string NormalizeRelative(string path) =>
        path.Replace('\\', '/').Trim();

    private sealed record ReportSpec(string Id, string Command, string FileName);
    private sealed record ReportStatus(string SourceStatus, string ReadinessStatus);
}
