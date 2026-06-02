using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class Phase161Reports
{
    private const string PlannedClientPreviewProfileId = "client-preview-local-no-network";

    private static readonly List<string> ClientPreviewNonClaims =
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

    private static readonly List<string> DeferredSystems =
    [
        "Client Preview implementation",
        "ClientHost construction changes",
        "Runtime loading/gameplay",
        "networking/session lifecycle",
        "rendering/UI",
        "asset import/cook",
    ];

    public static ClientHostPreviewOwnershipBoundaryReport BuildClientHostPreviewOwnershipBoundary(
        ManifestLoadResult load,
        string runtimeReadinessV2Path)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var runtime = ReadReport(load, runtimeReadinessV2Path, "runtime-readiness-v2", diagnostics);
        var readiness = runtime.ReadinessStatus switch
        {
            "Blocked" => "Blocked",
            "PartiallyProven" => "PartiallyProven",
            "ReadyForImplementationPlanning" => "ReadyForImplementationPlanning",
            _ => "MissingEvidence",
        };

        return new ClientHostPreviewOwnershipBoundaryReport
        {
            Command = "clienthost-preview-ownership-boundary",
            Status = readiness,
            ReadinessStatus = readiness,
            RuntimeReadinessV2Status = runtime.ReadinessStatus,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            AllowedFutureInputs =
            [
                new FutureBoundaryInputItem { InputId = "runtime-readiness-v2", Boundary = "ClientHostPreviewRuntimeReadSeam", Source = "Runtime read seam readiness v2 evidence.", Status = runtime.ReadinessStatus },
                new FutureBoundaryInputItem { InputId = "scene-entity-input-contracts", Boundary = "ClientHostPreviewRuntimeReadSeam", Source = "Future Runtime-readable scene/entity input contracts.", Status = "FutureInput" },
                new FutureBoundaryInputItem { InputId = "accepted-asset-reference-evidence", Boundary = "ClientHostPreviewRuntimeReadSeam", Source = "Accepted asset reference evidence.", Status = "FutureInput" },
                new FutureBoundaryInputItem { InputId = "phase162-launch-profile-contract", Boundary = "ClientHostPreviewRuntimeReadSeam", Source = "Future launch/profile contract evidence from Phase 162.", Status = "FutureInput" },
            ],
            ForbiddenDirectOwnership =
            [
                "ClientHost must not own scene/entity source truth.",
                "ClientHost must not own Runtime source truth.",
                "ClientHost must not treat generated projections as canonical state.",
                "ClientHost must not own package/launch summaries.",
                "ClientHost must not own server state.",
                "ClientHost must not own socket/network/session transport.",
                "ClientHost must not own asset import/cook outputs.",
            ],
            DeferredSystems = DeferredSystems,
            Checks =
            [
                Check("RuntimeReadinessV2", runtime.ReadinessStatus, "Runtime readiness v2 is the only required input for this ownership boundary report."),
                Check("ClientHostImplementationNotClaimed", "Passed", "ClientHost remains not implemented."),
                Check("ClientPreviewDeferred", "Deferred", "Client Preview remains deferred."),
                Check("ReportOnlyInvariants", "Passed", "The command reads reports and writes one report only."),
            ],
            NonClaims = ClientPreviewNonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static ClientPreviewLaunchProfileContractReport BuildClientPreviewLaunchProfileContract(
        ManifestLoadResult load,
        string runtimeReadinessV2Path)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var runtime = ReadReport(load, runtimeReadinessV2Path, "runtime-readiness-v2", diagnostics);
        var launchProfiles = BuildLaunchProfileClassifications(load);
        var packageProfiles = BuildPackageProfileClassifications(load);
        var plannedProfileExists = launchProfiles.Any(item => item.ProfileId == PlannedClientPreviewProfileId);

        var readiness = runtime.ReadinessStatus switch
        {
            "MissingEvidence" => "MissingEvidence",
            "Blocked" => "Blocked",
            "PartiallyProven" => "PartiallyProven",
            "ReadyForImplementationPlanning" => plannedProfileExists ? "ReadyForImplementationPlanning" : "PartiallyProven",
            _ => "MissingEvidence",
        };

        return new ClientPreviewLaunchProfileContractReport
        {
            Command = "client-preview-launch-profile-contract",
            Status = readiness,
            ReadinessStatus = readiness,
            RuntimeReadinessV2Status = runtime.ReadinessStatus,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            PlannedProfileId = PlannedClientPreviewProfileId,
            PlannedProfileStatus = plannedProfileExists ? "DeferredProfileContractPresent" : "DeferredProfileNotImplemented",
            CurrentLaunchProfiles = launchProfiles,
            CurrentPackageProfiles = packageProfiles,
            RequiredFutureEvidence =
            [
                Check("RuntimeReadinessV2NotMissingOrBlocked", RuntimeCanProceed(runtime.ReadinessStatus), "Runtime readiness v2 must not be missing or blocked before any future Client Preview launch."),
                Check("ClientHostOwnershipBoundaryReport", "RequiredFutureEvidence", "Phase 161 boundary evidence must exist before implementation planning can proceed."),
                Check("NoNetworkLocalModePlan", "RequiredFutureEvidence", "Phase 163 no-network local mode plan must exist before launch planning can proceed."),
                Check("PackageSourceTruthAlignmentReportOnly", "RequiredFutureEvidence", "Package/source-truth alignment evidence must remain report-only."),
            ],
            CannotLaunchYet =
            [
                "No Client Preview profile exists.",
                "Runtime read adapters are not implemented.",
                "ClientHost is not implemented.",
                "No process launch is permitted in this phase.",
            ],
            Checks =
            [
                Check("LocalServerHeadlessClassifiedServerOnly", LaunchStatus(launchProfiles, "local-server-headless"), "The current local-server-headless profile is server-only evidence, not a Client Preview profile."),
                Check("PlannedClientPreviewProfile", plannedProfileExists ? "DeferredProfileContractPresent" : "DeferredProfileNotImplemented", "The planned Client Preview profile id is client-preview-local-no-network."),
                Check("NoProcessLaunched", "Passed", "This command does not run LaunchLab and does not launch any process."),
                Check("ClientPreviewDeferred", "Deferred", "Client Preview remains deferred."),
                Check("ReportOnlyInvariants", "Passed", "The command reads manifest/profile references and Runtime readiness v2 only."),
            ],
            NonClaims = ClientPreviewNonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static ClientPreviewNoNetworkPlanReport BuildClientPreviewNoNetworkPlan(ManifestLoadResult load)
    {
        return new ClientPreviewNoNetworkPlanReport
        {
            Command = "client-preview-no-network-plan",
            Status = "PartiallyProven",
            ReadinessStatus = "PartiallyProven",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(load.Diagnostics),
            NoNetworkDefinition =
            [
                Check("Sockets", "Forbidden", "No sockets are opened or required."),
                Check("ServerProcess", "Forbidden", "No server process is started."),
                Check("TransportLayer", "Forbidden", "No transport layer is used."),
                Check("ExternalNetwork", "Forbidden", "No external network is used."),
                Check("MultiplayerProof", "NotClaimed", "No multiplayer proof is claimed."),
                Check("NetworkProof", "NotClaimed", "No network proof is claimed."),
            ],
            AllowedLaterSimulatedInputs =
            [
                new RuntimeReadInputItem { InputId = "static-scene-entity-fixtures", Kind = "SceneEntityFixture", Path = "Future fixture path", Classification = "FutureLocalOnlySimulatedInput", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "accepted-asset-reference-summaries", Kind = "AssetReferenceSummary", Path = "Future report evidence", Classification = "FutureLocalOnlySimulatedInput", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "placeholder-session-metadata", Kind = "DeterministicPlaceholderSessionMetadata", Path = "Future local metadata", Classification = "FutureLocalOnlySimulatedInput", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "fresh-or-partial-generated-projection-evidence", Kind = "GeneratedProjectionEvidence", Path = "Future freshness evidence", Classification = "EvidenceOnlyWhenFreshOrExplicitlyPartial", Status = "PlanOnly" },
            ],
            ForbiddenLaterInputs =
            [
                new RuntimeForbiddenSourceTruthItem { InputId = "live-server-state", Kind = "ServerState", Path = "Future", Reason = "No-network preview must not consume live server state." },
                new RuntimeForbiddenSourceTruthItem { InputId = "real-transport-packets", Kind = "TransportPacket", Path = "Future", Reason = "No-network preview must not consume real transport packets." },
                new RuntimeForbiddenSourceTruthItem { InputId = "socket-bound-sessions", Kind = "SocketSession", Path = "Future", Reason = "No-network preview must not bind socket sessions." },
                new RuntimeForbiddenSourceTruthItem { InputId = "multiplayer-replication-proof", Kind = "ReplicationProof", Path = "Future", Reason = "No-network preview is not multiplayer proof." },
                new RuntimeForbiddenSourceTruthItem { InputId = "asset-import-cook-outputs", Kind = "AssetPipelineOutput", Path = "Future", Reason = "Asset import/cook outputs are deferred." },
            ],
            BlockersBeforeImplementation =
            [
                "ClientHost boundary not implemented.",
                "Runtime read adapters not implemented.",
                "Client Preview profile not implemented.",
                "diagnostics shell not implemented until Phase 164.",
            ],
            NotStartedSystems =
            [
                "sockets",
                "server process",
                "transport layer",
                "external network",
            ],
            NonClaims = ClientPreviewNonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static ClientPreviewDiagnosticsShellReport BuildClientPreviewDiagnosticsShell(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var inputs = ReadRequiredInputs(load, evidenceRoot, diagnostics, DiagnosticsInputs());
        var status = AggregateReadiness(inputs.Select(item => item.ReadinessStatus));

        return new ClientPreviewDiagnosticsShellReport
        {
            Command = "client-preview-diagnostics-shell",
            Status = status,
            ReadinessStatus = status,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RuntimeReadinessV2Status = StatusFor(inputs, "runtime-readiness-v2"),
            ClientHostBoundaryStatus = StatusFor(inputs, "clienthost-preview-ownership-boundary"),
            LaunchProfileContractStatus = StatusFor(inputs, "client-preview-launch-profile-contract"),
            NoNetworkPlanStatus = StatusFor(inputs, "client-preview-no-network-plan"),
            RequiredInputs = inputs,
            DeferredSystems = DeferredSystems,
            NonClaims = ClientPreviewNonClaims,
            ImplementationBlockers =
            [
                "Runtime read adapters are not implemented.",
                "ClientHost is not implemented.",
                "Client Preview implementation is not implemented.",
                "Client Preview launch/profile contract remains deferred.",
                "No-network local mode remains plan-only.",
            ],
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static ClientPreviewBlockerMatrixReport BuildClientPreviewBlockerMatrix(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var required = ReadRequiredInputs(load, evidenceRoot, diagnostics, BlockerMatrixInputs());
        var evidenceStatus = AggregateReadiness(required.Select(item => item.ReadinessStatus));
        var status = evidenceStatus switch
        {
            "MissingEvidence" => "MissingEvidence",
            "Blocked" => "Blocked",
            "PartiallyProven" => "PartiallyProven",
            _ => "ReadyForImplementationPlanning",
        };

        return new ClientPreviewBlockerMatrixReport
        {
            Command = "client-preview-blocker-matrix",
            Status = status,
            ReadinessStatus = status,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredReports = required,
            Blockers = BuildBlockerRows(required),
            NonClaims = ClientPreviewNonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static List<LaunchProfileContractItem> BuildLaunchProfileClassifications(ManifestLoadResult load) =>
        (load.Descriptor?.LaunchProfiles ?? [])
            .OrderBy(item => item.Id, StringComparer.Ordinal)
            .Select(item =>
            {
                var isServerOnly = item.Id == "local-server-headless";
                var isPlannedClient = item.Id == PlannedClientPreviewProfileId;
                return new LaunchProfileContractItem
                {
                    ProfileId = item.Id,
                    Kind = "launch",
                    Path = NormalizeRelative(item.Path),
                    Classification = isServerOnly ? "ServerHeadlessEvidenceOnly" :
                        isPlannedClient ? "ClientPreviewNoNetworkContract" :
                        "UnknownLaunchEvidence",
                    Status = isServerOnly ? "NotClientPreview" :
                        isPlannedClient ? "DeferredProfileContractPresent" :
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
                return new LaunchProfileContractItem
                {
                    ProfileId = item.Id,
                    Kind = "package",
                    Path = NormalizeRelative(item.Path),
                    Classification = isServerOnly ? "ServerHeadlessPackageEvidenceOnly" : "UnknownPackageEvidence",
                    Status = isServerOnly ? "NotClientPreview" : "Unclassified",
                };
            })
            .ToList();

    private static List<ClientPreviewBlockerMatrixRow> BuildBlockerRows(List<ReportEvidenceStatusItem> required)
    {
        var runtime = StatusFor(required, "runtime-readiness-v2");
        var boundary = StatusFor(required, "clienthost-preview-ownership-boundary");
        var launch = StatusFor(required, "client-preview-launch-profile-contract");
        var noNetwork = StatusFor(required, "client-preview-no-network-plan");
        var diagnostics = StatusFor(required, "client-preview-diagnostics-shell");

        return
        [
            Row("runtime-read-readiness", "Runtime read readiness", runtime, "NotImplemented", EvidenceMatrixStatus(runtime), "Runtime readiness v2 evidence gates future Runtime read adapter work."),
            Row("clienthost-ownership-boundary", "ClientHost ownership boundary", boundary, "NotImplemented", EvidenceMatrixStatus(boundary), "ClientHost ownership remains planning-only."),
            Row("client-preview-launch-profile-contract", "Client Preview launch/profile contract", launch, "DeferredProfileNotImplemented", EvidenceMatrixStatus(launch), "Client Preview launch profile is not implemented."),
            Row("no-network-local-mode", "no-network local mode", noNetwork, "PlanOnly", EvidenceMatrixStatus(noNetwork), "No-network local mode is a plan, not behavior."),
            Row("diagnostics-shell", "diagnostics shell", diagnostics, "ReportOnly", EvidenceMatrixStatus(diagnostics), "Diagnostics shell aggregates report JSON only."),
            Row("runtime-adapters", "Runtime adapters", runtime, "NotImplemented", "BlockedByFutureImplementation", "Runtime read adapters are missing."),
            Row("clienthost-implementation", "ClientHost implementation", boundary, "NotImplemented", "BlockedByFutureImplementation", "ClientHost implementation is missing."),
            Row("client-preview-implementation", "Client Preview implementation", launch, "NotImplemented", "BlockedByFutureImplementation", "Client Preview implementation is missing."),
            Row("networking-explicitly-forbidden", "networking explicitly forbidden", noNetwork, "Forbidden", "Deferred", "Sockets, server process, transport layer, and external network remain forbidden for no-network preview planning."),
            Row("asset-import-cook-deferred", "asset import/cook deferred", noNetwork, "Deferred", "Deferred", "Asset import/cook outputs remain deferred and forbidden as Client Preview inputs."),
        ];
    }

    private static ClientPreviewBlockerMatrixRow Row(
        string id,
        string area,
        string evidenceStatus,
        string implementationStatus,
        string matrixStatus,
        string summary) =>
        new()
        {
            BlockerId = id,
            Area = area,
            EvidenceStatus = evidenceStatus,
            ImplementationStatus = implementationStatus,
            MatrixStatus = matrixStatus,
            Summary = summary,
        };

    private static string EvidenceMatrixStatus(string readiness) =>
        readiness switch
        {
            "ReadyForImplementationPlanning" => "PlanningEvidenceReady",
            "PartiallyProven" => "PartiallyProven",
            "Blocked" => "Blocked",
            _ => "MissingEvidence",
        };

    private static string RuntimeCanProceed(string readiness) =>
        readiness is "PartiallyProven" or "ReadyForImplementationPlanning" ? readiness : readiness;

    private static string LaunchStatus(List<LaunchProfileContractItem> profiles, string id) =>
        profiles.FirstOrDefault(item => item.ProfileId == id)?.Classification ?? "MissingEvidence";

    private static List<ReportSpec> DiagnosticsInputs() =>
    [
        new("runtime-readiness-v2", "runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        new("clienthost-preview-ownership-boundary", "clienthost-preview-ownership-boundary", "clienthost_preview_ownership_boundary_report.json"),
        new("client-preview-launch-profile-contract", "client-preview-launch-profile-contract", "client_preview_launch_profile_contract_report.json"),
        new("client-preview-no-network-plan", "client-preview-no-network-plan", "client_preview_no_network_plan_report.json"),
    ];

    private static List<ReportSpec> BlockerMatrixInputs() =>
    [
        ..DiagnosticsInputs(),
        new("client-preview-diagnostics-shell", "client-preview-diagnostics-shell", "client_preview_diagnostics_shell_report.json"),
    ];

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
                    $"Project.Phase161_165.{expectedCommand}.Missing",
                    "ClientPreviewRuntimeReadSeam",
                    NormalizeRelative(reportPath),
                    $"Required report is missing: {expectedCommand}."));
                return new ReportStatus("Missing", "MissingEvidence");
            }

            var root = JsonNode.Parse(File.ReadAllText(fullPath)) as JsonObject;
            if (root is null)
            {
                diagnostics.Add(Error(
                    $"Project.Phase161_165.{expectedCommand}.Malformed",
                    "ClientPreviewRuntimeReadSeam",
                    NormalizeRelative(reportPath),
                    $"Required report JSON root must be an object: {expectedCommand}."));
                return new ReportStatus("Malformed", "MissingEvidence");
            }

            var command = ReadString(root, "command");
            if (!string.IsNullOrWhiteSpace(command) && command != expectedCommand)
            {
                diagnostics.Add(Error(
                    $"Project.Phase161_165.{expectedCommand}.UnexpectedCommand",
                    "ClientPreviewRuntimeReadSeam",
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
                $"Project.Phase161_165.{expectedCommand}.Malformed",
                "ClientPreviewRuntimeReadSeam",
                NormalizeRelative(reportPath),
                $"Required report JSON could not be read: {e.Message}"));
            return new ReportStatus("Malformed", "MissingEvidence");
        }
    }

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
