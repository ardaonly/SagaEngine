using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class ClientReadinessReports
{
    private static readonly EvidenceSpec[] PrerequisiteEvidence =
    [
        new("source-truth-inventory", "source-truth-foundation", "source-truth-inventory", "source_truth_inventory_report.json", true),
        new("source-truth-gate", "source-truth-foundation", "source-truth-gate", "source_truth_gate_report.json", true),
        new("asset-manifest-inventory", "asset-and-scene-integrity", "asset-source-manifest-inventory", "asset_source_manifest_inventory_report.json", true),
        new("asset-reference-gate", "asset-and-scene-integrity", "asset-reference-gate", "asset_reference_gate_report.json", true),
        new("freshness", "asset-and-scene-integrity", "generated-freshness-gate", "generated_artifact_freshness_report.json", true),
        new("scene-validation", "asset-and-scene-integrity", "scene-entity-validate", "scene_entity_validation_report.json", true),
        new("component-ownership", "runtime-editor-read-boundary", "component-metadata-ownership", "component_metadata_ownership_report.json", true),
        new("editor-inspection", "runtime-editor-read-boundary", "editor-inspection-model", "editor_inspection_model_report.json", true),
        new("editor-read", "runtime-editor-read-boundary", "editor-source-truth-read", "editor_source_truth_read_report.json", true),
        new("runtime-audit", "runtime-editor-read-boundary", "runtime-read-model-audit", "runtime_read_model_audit_report.json", true),
        new("runtime-readiness", "runtime-editor-read-boundary", "runtime-readiness-gate", "runtime_readiness_gate_report.json", true),
        new("package-alignment", "package-source-alignment", "source-truth-alignment", "package_source_truth_alignment_report.json", true),
        new("launch-alignment", "launch-source-alignment", "source-truth-alignment", "launch_source_truth_alignment_report.json", true),
        new("source-truth-scenario", "source-truth-scenario", "source-truth-scenario", "source_truth_scenario_report.json", true),
    ];

    private static readonly EvidenceSpec[] ClosureEvidence =
    [
        .. PrerequisiteEvidence,
        new("client-evaluation-prerequisite", "client-evaluation-prerequisite", "client-evaluation-prerequisite-audit", "client_evaluation_prerequisite_audit_report.json", true),
        new("clienthost-boundary-plan", "client-host-boundary", "clienthost-boundary-plan", "clienthost_boundary_plan_report.json", true),
        new("asset-import-cook-deferral", "asset-import-cook-deferral", "asset-import-cook-deferral", "asset_import_cook_deferral_report.json", true),
        new("broad-test-health-preflight", "broad-test-health", "broad-test-health-preflight", "broad_test_health_preflight_report.json", false),
    ];

    private static readonly List<string> ResidualDebt =
    [
        "Client Evaluation remains deferred.",
        "ClientHost boundary work remains planning-only.",
        "Asset import remains deferred.",
        "Asset cook remains deferred.",
        "Editor UI and Qt UI remain deferred.",
        "Runtime gameplay and Server gameplay remain deferred.",
        "Raw full CTest, heavy stress, soak, bot swarm, and real transport proof remain unclaimed or deferred.",
    ];

    private static readonly List<string> NonClaims =
    [
        "No Product Beta is claimed.",
        "No Release Candidate is claimed.",
        "No Production Readiness is claimed.",
        "No Playable Editor is claimed.",
        "No Client Evaluation implementation is claimed.",
        "No ClientHost implementation is claimed.",
        "No Runtime gameplay implementation is claimed.",
        "No Server gameplay implementation is claimed.",
        "No asset import or asset cook implementation is claimed.",
        "No raw full CTest, heavy stress, soak, bot swarm, or real transport stress pass is claimed.",
    ];

    public static ClientEvaluationPrerequisiteAuditReport BuildClientEvaluationPrerequisiteAudit(ManifestLoadResult load, string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var evidence = ReadEvidence(load, evidenceRoot, PrerequisiteEvidence, diagnostics);
        var present = evidence.Where(item => item.Status is not "Missing").ToList();
        var blocked = evidence.Any(item => item.EvidenceId is "source-truth-inventory" or "source-truth-gate" or "scene-validation" or "asset-reference-gate" && item.Status is "Failed" or "Missing");
        var outcome = blocked ? "Blocked" : present.Count == 0 ? "MissingEvidence" : "PartiallyProven";
        if (present.Count == 0)
        {
            diagnostics.Add(Error(
                "Project.ClientEvaluationPrerequisite.EvidenceMissing",
                "ClientEvaluationPrerequisite",
                NormalizeRelative(evidenceRoot),
                "Client Evaluation prerequisite audit requires prior source-truth evidence."));
        }

        var checks = new List<SourceTruthGateCheck>
        {
            Check("SceneEntitySourceTruth", EvidenceCheck(evidence, "scene-validation"), "Scene/entity source truth evidence is present and not treated as Client Evaluation implementation."),
            Check("AssetReferenceSourceTruth", EvidenceCheck(evidence, "asset-reference-gate"), "Asset reference source truth evidence is present."),
            Check("GeneratedFreshness", EvidenceCheck(evidence, "freshness"), "Generated freshness debt remains explicit."),
            Check("RuntimeReadinessPlanningOnly", EvidenceCheck(evidence, "runtime-readiness"), "Runtime readiness is planning-only evidence."),
            Check("PackageAlignment", EvidenceCheck(evidence, "package-alignment"), "Package alignment evidence is read-only."),
            Check("LaunchAlignment", EvidenceCheck(evidence, "launch-alignment"), "Launch alignment evidence is read-only and preserves Client Evaluation deferral."),
            Check("ClientEvaluationDeferred", "Deferred", "Client Evaluation remains deferred."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new ClientEvaluationPrerequisiteAuditReport
        {
            Command = "client-evaluation-prerequisite-audit",
            Status = outcome is "Blocked" or "MissingEvidence" ? "Failed" : "PartiallyProven",
            Outcome = outcome,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            ClientEvaluationStatus = "Deferred",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            PriorEvidence = evidence,
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            RequiredFutureSeams =
            [
                new FutureSeamItem
                {
                    SeamName = "ClientEvaluationRuntimeReadSeam",
                    Scope = "Future planning seam from source truth evidence into a bounded Client Evaluation read path.",
                    Status = "PlanningOnly",
                },
            ],
            RecommendedNextSlices = ["Client Evaluation / Runtime Read Seam planning slice"],
            ResidualDebt = ResidualDebt,
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static ClientHostBoundaryPlanReport BuildClientHostBoundaryPlan(ManifestLoadResult load, string prerequisitePath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var prerequisite = LoadJsonObject(prerequisitePath, diagnostics, "Project.ClientHostBoundary.PrerequisiteMalformed");
        var prerequisiteStatus = ReadString(prerequisite, "status");
        var prerequisiteOutcome = ReadString(prerequisite, "outcome");
        if (prerequisite is null)
        {
            diagnostics.Add(Error(
                "Project.ClientHostBoundary.PrerequisiteMissing",
                "ClientHostBoundary",
                prerequisitePath,
                "ClientHost boundary planning requires the Client Evaluation prerequisite audit."));
        }

        var failed = prerequisite is null || prerequisiteStatus == "Failed" || prerequisiteOutcome is "Blocked" or "MissingEvidence";
        var checks = new List<SourceTruthGateCheck>
        {
            Check("PrerequisiteEvidencePresent", prerequisite is null ? "Failed" : prerequisiteStatus, "Client Evaluation prerequisite report is present."),
            Check("FutureSeamNamed", "Passed", "Future seam is named ClientEvaluationRuntimeReadSeam."),
            Check("ForbiddenOwnershipRecorded", "Passed", "ClientHost must not own source truth, Runtime source truth, package source truth, or generated canonical state."),
            Check("DeferredConcernsRecorded", "Passed", "Network, session, UI, and render work remain deferred."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new ClientHostBoundaryPlanReport
        {
            Command = "clienthost-boundary-plan",
            Status = failed ? "Failed" : "PartiallyProven",
            Outcome = failed ? "MissingEvidence" : "PartiallyProven",
            FutureSeamName = "ClientEvaluationRuntimeReadSeam",
            ClientEvaluationStatus = "Deferred",
            PrerequisiteStatus = string.IsNullOrWhiteSpace(prerequisiteStatus) ? "MissingEvidence" : prerequisiteStatus,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            FutureInputs =
            [
                new FutureBoundaryInputItem { InputId = "scene-entity-source-truth", Boundary = "SourceTruth", Source = "scene_entity_validation_report.json" },
                new FutureBoundaryInputItem { InputId = "runtime-read-boundary", Boundary = "RuntimeRead", Source = "runtime_readiness_gate_report.json" },
                new FutureBoundaryInputItem { InputId = "package-launch-boundary", Boundary = "PackageLaunch", Source = "package_source_truth_alignment_report.json; launch_source_truth_alignment_report.json" },
            ],
            ForbiddenDirectOwnership =
            [
                "ClientHost must not own scene/entity source truth.",
                "ClientHost must not own Runtime source truth.",
                "ClientHost must not own package or launch source truth.",
                "ClientHost must not treat generated projections as canonical.",
            ],
            DeferredConcerns = ["NetworkTransport", "SessionLifecycle", "UiImplementation", "RenderImplementation"],
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static AssetImportCookDeferralReport BuildAssetImportCookDeferral(ManifestLoadResult load, string assetManifestInventoryPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var inventory = LoadJsonObject(assetManifestInventoryPath, diagnostics, "Project.AssetImportCook.AssetManifestInventoryMalformed");
        if (inventory is null)
        {
            diagnostics.Add(Error(
                "Project.AssetImportCook.AssetManifestInventoryMissing",
                "AssetImportCook",
                assetManifestInventoryPath,
                "Asset import/cook deferral requires asset manifest inventory evidence."));
        }

        var inventoryStatus = ReadString(inventory, "status");
        var classifications = new List<AssetPipelineDeferralItem>();
        foreach (var item in ReadArrayObjects(inventory, "assetRoots"))
        {
            classifications.Add(new AssetPipelineDeferralItem
            {
                ItemId = ReadString(item, "assetId"),
                Kind = "AssetRoot",
                Source = ReadString(item, "relativePath"),
                Classification = ReadString(item, "classification"),
                Status = ReadString(item, "status"),
            });
        }
        foreach (var item in ReadArrayObjects(inventory, "assetSourceManifests"))
        {
            classifications.Add(new AssetPipelineDeferralItem
            {
                ItemId = ReadString(item, "manifestId"),
                Kind = "AssetSourceManifest",
                Source = ReadString(item, "relativePath"),
                Classification = ReadString(item, "classification"),
                Status = ReadString(item, "status"),
            });
        }
        classifications.Add(new AssetPipelineDeferralItem { ItemId = "generated-package-artifacts", Kind = "GeneratedPackageArtifacts", Source = "Build/Packages", Classification = "NonCanonicalEvidence", Status = "Deferred" });
        classifications.Add(new AssetPipelineDeferralItem { ItemId = "missing-import-pipeline", Kind = "AssetImportPipeline", Source = "Future", Classification = "MissingPipeline", Status = "Deferred" });
        classifications.Add(new AssetPipelineDeferralItem { ItemId = "missing-cook-pipeline", Kind = "AssetCookPipeline", Source = "Future", Classification = "MissingPipeline", Status = "Deferred" });

        var failed = inventory is null || inventoryStatus == "Failed";
        return new AssetImportCookDeferralReport
        {
            Command = "asset-import-cook-deferral",
            Status = failed ? "Failed" : "PartiallyProven",
            Outcome = "Deferred",
            AssetManifestInventoryStatus = string.IsNullOrWhiteSpace(inventoryStatus) ? "MissingEvidence" : inventoryStatus,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            AssetClassifications = classifications.OrderBy(item => item.Kind, StringComparer.Ordinal).ThenBy(item => item.ItemId, StringComparer.Ordinal).ToList(),
            DeferredPipelines =
            [
                new AssetPipelineDeferralItem { ItemId = "asset-import", Kind = "AssetImportPipeline", Source = "Future", Classification = "NotImplemented", Status = "Deferred" },
                new AssetPipelineDeferralItem { ItemId = "asset-cook", Kind = "AssetCookPipeline", Source = "Future", Classification = "NotImplemented", Status = "Deferred" },
            ],
            FuturePrerequisites =
            [
                "Accepted asset source manifest schema beyond inventory evidence.",
                "Runtime/package read contract for cooked outputs.",
                "Report-only cook proof before any mutating cook command.",
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static BroadTestHealthPreflightReport BuildBroadTestHealthPreflight(ManifestLoadResult load, string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var focused = ReadEvidence(load, evidenceRoot, ClosureEvidence, diagnostics)
            .Where(item => item.Status is not "Missing")
            .Select(item => new TestHealthEvidenceItem
            {
                EvidenceId = item.EvidenceId,
                Path = item.Path,
                Status = item.Status,
                Classification = "FocusedEvidence",
            })
            .OrderBy(item => item.EvidenceId, StringComparer.Ordinal)
            .ToList();

        return new BroadTestHealthPreflightReport
        {
            Command = "broad-test-health-preflight",
            Status = "PartiallyProven",
            Outcome = "PartiallyProven",
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            FocusedEvidence = focused,
            UnclaimedEvidence =
            [
                new TestHealthEvidenceItem { EvidenceId = "raw-full-ctest", Path = "", Status = "Unclaimed", Classification = "BroadTest" },
            ],
            DeferredEvidence =
            [
                new TestHealthEvidenceItem { EvidenceId = "heavy-stress", Path = "", Status = "Deferred", Classification = "HeavyTest" },
                new TestHealthEvidenceItem { EvidenceId = "long-soak", Path = "", Status = "Deferred", Classification = "SoakTest" },
                new TestHealthEvidenceItem { EvidenceId = "bot-swarm", Path = "", Status = "Deferred", Classification = "ScaleTest" },
                new TestHealthEvidenceItem { EvidenceId = "real-transport-proof", Path = "", Status = "Deferred", Classification = "TransportTest" },
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static AssetWorkflowEvidenceMatrixReport BuildAssetWorkflowEvidenceMatrix(ManifestLoadResult load, string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var evidence = BuildMatrix(load, evidenceRoot, diagnostics);
        var blocked = evidence.Any(item => item.Classification == "RequiredSourceTruth" && item.MatrixStatus is "Blocked" or "MissingEvidence");
        var partial = evidence.Any(item => item.MatrixStatus is "PartiallyProven" or "Deferred" or "Unclaimed");
        var outcome = blocked ? "Blocked" : partial ? "PartiallyProven" : "Passed";

        return new AssetWorkflowEvidenceMatrixReport
        {
            Command = "asset-workflow-evidence-matrix",
            Status = blocked ? "Failed" : outcome,
            Outcome = outcome,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            Evidence = evidence,
            ResidualDebt = ResidualDebt,
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static AssetWorkflowClosureReport BuildAssetWorkflowClosure(ManifestLoadResult load, string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var evidence = BuildMatrix(load, evidenceRoot, diagnostics);
        const string limitationsDoc = "SagaWiki/pages/source-of-truth.html";
        var limitationsDocPresent = FindRepoFile(limitationsDoc) is not null;
        if (!limitationsDocPresent)
        {
            diagnostics.Add(Error(
                "Project.SourceTruthClosure.LimitationsDocMissing",
                "SourceTruthClosure",
                limitationsDoc,
                "Source-truth closure requires the source-truth foundation contract doc."));
        }

        var required = evidence.Where(item => item.Classification == "RequiredSourceTruth").ToList();
        var blocked = required.Any(item => item.MatrixStatus is "Blocked" or "MissingEvidence") || !limitationsDocPresent;
        var residual = ResidualDebt;
        var outcome = blocked ? "Blocked" :
            residual.Count > 0 || evidence.Any(item => item.MatrixStatus is "PartiallyProven" or "Deferred" or "Unclaimed")
                ? "PartiallyProven"
                : "SourceTruthFoundationEstablished";

        return new AssetWorkflowClosureReport
        {
            Command = "asset-workflow-closure",
            Status = blocked ? "Failed" : outcome,
            Outcome = outcome,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            LimitationsDocPresent = limitationsDocPresent,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredEvidence = required.OrderBy(item => item.EvidenceId, StringComparer.Ordinal).ToList(),
            ResidualDebt = residual,
            NextTargetRecommendations = ["Future Client Evaluation / Runtime Read Seam planning only."],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static List<EvidenceMatrixItem> BuildMatrix(ManifestLoadResult load, string evidenceRoot, List<ProjectDiagnostic> diagnostics)
    {
        var evidence = ReadEvidence(load, evidenceRoot, ClosureEvidence, diagnostics);
        var rows = evidence.Select(item => new EvidenceMatrixItem
        {
            EvidenceId = item.EvidenceId,
            EvidenceScope = ClosureEvidence.First(spec => spec.Id == item.EvidenceId).EvidenceScope,
            Path = item.Path,
            SourceStatus = item.Status,
            MatrixStatus = MatrixStatus(item.Status),
            Classification = ClosureEvidence.First(spec => spec.Id == item.EvidenceId).RequiredSourceTruth ? "RequiredSourceTruth" : "FocusedHealthPreflight",
        }).ToList();

        rows.Add(new EvidenceMatrixItem
        {
            EvidenceId = "raw-full-ctest",
            EvidenceScope = "broad-test-health",
            SourceStatus = "Unclaimed",
            MatrixStatus = "Unclaimed",
            Classification = "NonClaim",
        });
        rows.Add(new EvidenceMatrixItem
        {
            EvidenceId = "heavy-stress-soak-bot-real-transport",
            EvidenceScope = "broad-test-health",
            SourceStatus = "Deferred",
            MatrixStatus = "Deferred",
            Classification = "DeferredProof",
        });

        foreach (var blocked in rows.Where(item => item.Classification == "RequiredSourceTruth" && item.MatrixStatus is "Blocked" or "MissingEvidence"))
        {
            diagnostics.Add(Error(
                "Project.AssetWorkflowEvidence.RequiredEvidenceMissingOrFailed",
                "AssetWorkflowEvidence",
                blocked.Path,
                $"Required source-truth evidence is not passing: {blocked.EvidenceId}."));
        }

        return rows
            .OrderBy(item => item.EvidenceScope, StringComparer.Ordinal)
            .ThenBy(item => item.EvidenceId, StringComparer.Ordinal)
            .ToList();
    }

    private static List<ScenarioEvidenceItem> ReadEvidence(
        ManifestLoadResult load,
        string evidenceRoot,
        IEnumerable<EvidenceSpec> specs,
        List<ProjectDiagnostic> diagnostics)
    {
        var fullRoot = FullEvidenceRoot(load, evidenceRoot);
        var result = new List<ScenarioEvidenceItem>();
        foreach (var spec in specs.OrderBy(item => item.Id, StringComparer.Ordinal))
        {
            var path = Path.Combine(fullRoot, spec.FileName);
            var report = LoadJsonObject(path, diagnostics, $"Project.AssetWorkflowEvidence.{spec.Id}.Malformed", missingIsDiagnostic: false);
            var status = report is null ? "Missing" : ReadString(report, "status");
            if (string.IsNullOrWhiteSpace(status))
            {
                status = "Missing";
            }

            result.Add(new ScenarioEvidenceItem
            {
                EvidenceId = spec.Id,
                Command = spec.Command,
                Path = NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)),
                Status = status,
            });
        }

        return result.OrderBy(item => item.EvidenceId, StringComparer.Ordinal).ToList();
    }

    private static string EvidenceCheck(IEnumerable<ScenarioEvidenceItem> evidence, string id)
    {
        var status = evidence.FirstOrDefault(item => item.EvidenceId == id)?.Status ?? "Missing";
        return status is "Passed" or "PartiallyProven" or "ReadyForPlanning" ? status : status == "Missing" ? "MissingEvidence" : "Failed";
    }

    private static string MatrixStatus(string sourceStatus) =>
        sourceStatus switch
        {
            "Passed" or "ReadyForPlanning" or "SourceTruthFoundationEstablished" => "Passed",
            "PartiallyProven" => "PartiallyProven",
            "Deferred" => "Deferred",
            "Unclaimed" => "Unclaimed",
            "Failed" or "Blocked" => "Blocked",
            _ => "MissingEvidence",
        };

    private static string FullEvidenceRoot(ManifestLoadResult load, string evidenceRoot) =>
        Path.IsPathRooted(evidenceRoot)
            ? Path.GetFullPath(evidenceRoot)
            : Path.GetFullPath(Path.Combine(load.ProjectRoot, evidenceRoot));

    private static JsonObject? LoadJsonObject(
        string path,
        List<ProjectDiagnostic> diagnostics,
        string code,
        bool missingIsDiagnostic = true)
    {
        try
        {
            if (!File.Exists(path))
            {
                if (missingIsDiagnostic)
                {
                    diagnostics.Add(Error(code, CategoryForCode(code), path, "Report JSON could not be read: file is missing."));
                }
                return null;
            }

            var node = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            if (node is null)
            {
                diagnostics.Add(Error(code, CategoryForCode(code), path, "Report JSON root must be an object."));
            }
            return node;
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Error(code, CategoryForCode(code), path, $"Report JSON could not be read: {e.Message}"));
            return null;
        }
    }

    private static IEnumerable<JsonObject> ReadArrayObjects(JsonObject? root, string key) =>
        root?[key] is JsonArray array ? array.OfType<JsonObject>() : [];

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";

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

    private static string CategoryForCode(string code)
    {
        if (code.Contains("ClientHost", StringComparison.Ordinal))
        {
            return "ClientHostBoundary";
        }
        if (code.Contains("AssetImportCook", StringComparison.Ordinal))
        {
            return "AssetImportCook";
        }
        if (code.Contains("Closure", StringComparison.Ordinal))
        {
            return "AssetWorkflowClosure";
        }
        return "AssetWorkflowEvidence";
    }

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

    private static string? FindRepoFile(string relativePath)
    {
        var current = Directory.GetCurrentDirectory();
        while (!string.IsNullOrWhiteSpace(current))
        {
            var candidate = Path.Combine(current, relativePath);
            if (File.Exists(candidate))
            {
                return candidate;
            }

            var parent = Directory.GetParent(current)?.FullName;
            if (string.IsNullOrWhiteSpace(parent) || parent == current)
            {
                return null;
            }
            current = parent;
        }

        return null;
    }

    private sealed record EvidenceSpec(
        string Id,
        string EvidenceScope,
        string Command,
        string FileName,
        bool RequiredSourceTruth);
}
