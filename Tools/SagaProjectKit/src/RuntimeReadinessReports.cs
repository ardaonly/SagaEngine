using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class RuntimeReadinessReports
{
    private static readonly EvidenceSpec[] RequiredEvidence =
    [
        new("source-truth-inventory", "source-truth-foundation", "source-truth-inventory", "source_truth_inventory_report.json"),
        new("source-truth-gate", "source-truth-foundation", "source-truth-gate", "source_truth_gate_report.json"),
        new("scene-validation", "asset-and-scene-integrity", "scene-entity-validate", "scene_entity_validation_report.json"),
        new("asset-reference-gate", "asset-and-scene-integrity", "asset-reference-gate", "asset_reference_gate_report.json"),
        new("freshness", "asset-and-scene-integrity", "generated-freshness-gate", "generated_artifact_freshness_report.json"),
        new("runtime-readiness", "runtime-editor-read-boundary", "runtime-readiness-gate", "runtime_readiness_gate_report.json"),
        new("package-alignment", "package-source-alignment", "source-truth-alignment", "package_source_truth_alignment_report.json"),
        new("launch-alignment", "launch-source-alignment", "source-truth-alignment", "launch_source_truth_alignment_report.json"),
        new("asset-workflow-closure", "asset-workflow-closure", "asset-workflow-closure", "asset_workflow_closure_report.json"),
    ];

    private static readonly List<string> ResidualDebt =
    [
        "Client Evaluation remains deferred.",
        "ClientHost remains planning-only and untouched.",
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
        "ReadyForImplementationPlanning does not mean Runtime, ClientHost, or Client Evaluation exists.",
    ];

    public static RuntimeReadinessV2Report BuildRuntimeReadinessV2(ManifestLoadResult load, string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var evidence = ReadEvidence(load, evidenceRoot, diagnostics);
        var missing = evidence.Any(item => item.ReadinessStatus == "MissingEvidence");
        var blocked = evidence.Any(item => item.ReadinessStatus == "Blocked");
        var partial = evidence.Any(item => item.ReadinessStatus == "PartiallyProven");
        var readiness = missing ? "MissingEvidence" :
            blocked ? "Blocked" :
            partial ? "PartiallyProven" :
            "ReadyForImplementationPlanning";

        var checks = new List<SourceTruthGateCheck>
        {
            Check("SourceTruthInventoryGate", CombinedEvidenceStatus(evidence, "source-truth-inventory", "source-truth-gate"), "Source truth inventory and gate evidence are present."),
            Check("SceneEntityValidation", EvidenceStatus(evidence, "scene-validation"), "Scene/entity validation evidence is present."),
            Check("AssetReferenceGate", EvidenceStatus(evidence, "asset-reference-gate"), "Asset reference gate evidence is present."),
            Check("GeneratedFreshness", EvidenceStatus(evidence, "freshness"), "Generated projection freshness evidence is present."),
            Check("RuntimeReadinessGate", EvidenceStatus(evidence, "runtime-readiness"), "Runtime readiness gate remains planning-only evidence."),
            Check("PackageAlignment", EvidenceStatus(evidence, "package-alignment"), "Package source-truth alignment evidence is present."),
            Check("LaunchAlignment", EvidenceStatus(evidence, "launch-alignment"), "Launch source-truth alignment evidence is present."),
            Check("SourceTruthClosure", EvidenceStatus(evidence, "asset-workflow-closure"), "Source-truth closure evidence is present."),
            Check("ClientEvaluationDeferred", "Deferred", "Client Evaluation remains deferred."),
            Check("RuntimeImplementationNotClaimed", "Passed", "Runtime read seam readiness does not implement Runtime gameplay."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new RuntimeReadinessV2Report
        {
            Command = "runtime-readiness-v2",
            Status = readiness,
            ReadinessStatus = readiness,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredEvidence = evidence,
            AdapterAudit = BuildAdapterAudit(evidence),
            FutureReadableInputs =
            [
                new RuntimeReadInputItem { InputId = "scene-entity-source-truth", Kind = "SceneEntitySourceTruth", Path = "Scene source truth declared by project manifest", Classification = "FutureRuntimeReadableInput", Status = "ContractOnly" },
                new RuntimeReadInputItem { InputId = "accepted-asset-references", Kind = "AssetReferenceSourceTruth", Path = "asset_reference_gate_report.json", Classification = "FutureRuntimeReadableInput", Status = "ContractOnly" },
                new RuntimeReadInputItem { InputId = "non-canonical-generated-projections", Kind = "GeneratedProjectionEvidence", Path = "generated_artifact_freshness_report.json", Classification = "EvidenceOnlyWhenFreshOrPartial", Status = "ContractOnly" },
                new RuntimeReadInputItem { InputId = "freshness-evidence", Kind = "GeneratedFreshnessEvidence", Path = "generated_artifact_freshness_report.json", Classification = "RequiredEvidence", Status = "ContractOnly" },
            ],
            ForbiddenRuntimeSourceTruth =
            [
                new RuntimeForbiddenSourceTruthItem { InputId = "generated-artifacts", Kind = "GeneratedProjection", Path = "Generated/", Reason = "Generated artifacts cannot own Runtime source truth." },
                new RuntimeForbiddenSourceTruthItem { InputId = "package-summaries", Kind = "PackageSummary", Path = "Build/Packages", Reason = "Package summaries are alignment evidence only." },
                new RuntimeForbiddenSourceTruthItem { InputId = "launch-summaries", Kind = "LaunchSummary", Path = "Build/Launch", Reason = "Launch summaries are alignment evidence only." },
                new RuntimeForbiddenSourceTruthItem { InputId = "editor-diagnostics", Kind = "EditorDiagnostic", Path = "Build/SourceTruth", Reason = "Editor diagnostics cannot own Runtime source truth." },
                new RuntimeForbiddenSourceTruthItem { InputId = "clienthost-state", Kind = "ClientHostState", Path = "Future", Reason = "ClientHost state cannot own Runtime source truth." },
                new RuntimeForbiddenSourceTruthItem { InputId = "stale-projections", Kind = "StaleGeneratedProjection", Path = "Generated/", Reason = "Stale projections remain partial evidence and cannot become Runtime source truth." },
                new RuntimeForbiddenSourceTruthItem { InputId = "asset-import-cook-outputs", Kind = "AssetPipelineOutput", Path = "Future", Reason = "Asset import and cook outputs are deferred." },
            ],
            FixtureContract =
            [
                new RuntimeReadFixtureContractItem { FixtureId = "validated-scene", RequiredInput = "One validated scene", ExpectedBehavior = "Missing or invalid fixture blocks implementation planning.", Status = "ContractOnly" },
                new RuntimeReadFixtureContractItem { FixtureId = "validated-entity", RequiredInput = "One entity in the validated scene", ExpectedBehavior = "Entity metadata remains scene/entity source truth.", Status = "ContractOnly" },
                new RuntimeReadFixtureContractItem { FixtureId = "component-metadata", RequiredInput = "Component metadata from scene/entity source truth", ExpectedBehavior = "Runtime adapters may read only after the seam is implemented later.", Status = "ContractOnly" },
                new RuntimeReadFixtureContractItem { FixtureId = "asset-references", RequiredInput = "Accepted asset references", ExpectedBehavior = "Unaccepted references block planning.", Status = "ContractOnly" },
                new RuntimeReadFixtureContractItem { FixtureId = "projection-metadata", RequiredInput = "Non-canonical projection metadata", ExpectedBehavior = "Stale projection metadata remains partial evidence.", Status = "ContractOnly" },
                new RuntimeReadFixtureContractItem { FixtureId = "freshness-metadata", RequiredInput = "Expected freshness metadata", ExpectedBehavior = "Missing freshness blocks implementation planning.", Status = "ContractOnly" },
            ],
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            MissingAdapterSeams =
            [
                "Runtime scene source truth input adapter",
                "Runtime entity source truth input adapter",
                "Runtime component metadata read adapter",
                "Runtime accepted asset reference read adapter",
                "Runtime generated projection evidence adapter",
                "Runtime freshness evidence adapter",
            ],
            ResidualDebt = ResidualDebt,
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static List<RuntimeReadinessEvidenceItem> ReadEvidence(
        ManifestLoadResult load,
        string evidenceRoot,
        List<ProjectDiagnostic> diagnostics)
    {
        var fullRoot = FullEvidenceRoot(load, evidenceRoot);
        if (!Directory.Exists(fullRoot))
        {
            diagnostics.Add(Error(
                "Project.RuntimeReadinessV2.EvidenceRootMissing",
                "RuntimeReadinessV2",
                evidenceRoot,
                "Runtime readiness v2 requires a Build/SourceTruth evidence root."));
        }

        var result = new List<RuntimeReadinessEvidenceItem>();
        foreach (var spec in RequiredEvidence.OrderBy(item => item.Id, StringComparer.Ordinal))
        {
            var path = Path.Combine(fullRoot, spec.FileName);
            var report = LoadJsonObject(path, diagnostics, $"Project.RuntimeReadinessV2.{spec.Id}.Malformed", missingIsDiagnostic: false);
            var status = report is null ? "Missing" : FirstNonEmpty(ReadString(report, "status"), ReadString(report, "outcome"));
            if (string.IsNullOrWhiteSpace(status))
            {
                status = "Missing";
            }

            var readiness = MapEvidenceStatus(status);
            if (readiness == "MissingEvidence")
            {
                diagnostics.Add(Error(
                    "Project.RuntimeReadinessV2.RequiredEvidenceMissing",
                    "RuntimeReadinessV2",
                    NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)),
                    $"Required Runtime read seam evidence is missing: {spec.Id}."));
            }
            else if (readiness == "Blocked")
            {
                diagnostics.Add(Error(
                    "Project.RuntimeReadinessV2.RequiredEvidenceBlocked",
                    "RuntimeReadinessV2",
                    NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)),
                    $"Required Runtime read seam evidence is failed or blocked: {spec.Id}."));
            }

            result.Add(new RuntimeReadinessEvidenceItem
            {
                EvidenceId = spec.Id,
                EvidenceScope = spec.EvidenceScope,
                Command = spec.Command,
                Path = NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)),
                SourceStatus = status,
                ReadinessStatus = readiness,
                Required = true,
            });
        }

        return result
            .OrderBy(item => item.EvidenceScope, StringComparer.Ordinal)
            .ThenBy(item => item.EvidenceId, StringComparer.Ordinal)
            .ToList();
    }

    private static List<RuntimeReadAdapterAuditItem> BuildAdapterAudit(IEnumerable<RuntimeReadinessEvidenceItem> evidence) =>
    [
        Adapter("scene-source-truth-adapter", "scene-validation", "validated scene source truth", evidence),
        Adapter("entity-source-truth-adapter", "scene-validation", "validated entity source truth", evidence),
        Adapter("component-metadata-adapter", "scene-validation", "component metadata source truth", evidence),
        Adapter("asset-reference-adapter", "asset-reference-gate", "accepted asset references", evidence),
        Adapter("generated-projection-adapter", "freshness", "non-canonical generated projections with freshness evidence", evidence),
        Adapter("freshness-evidence-adapter", "freshness", "freshness metadata", evidence),
    ];

    private static RuntimeReadAdapterAuditItem Adapter(
        string adapterId,
        string sourceEvidenceId,
        string expectedInput,
        IEnumerable<RuntimeReadinessEvidenceItem> evidence)
    {
        var status = EvidenceStatus(evidence, sourceEvidenceId) switch
        {
            "Passed" => "MissingAdapterSeam",
            "PartiallyProven" => "MissingAdapterSeamWithPartialEvidence",
            "Blocked" => "BlockedByEvidence",
            "MissingEvidence" => "MissingEvidence",
            _ => "MissingAdapterSeam",
        };
        return new RuntimeReadAdapterAuditItem
        {
            AdapterId = adapterId,
            SourceEvidenceId = sourceEvidenceId,
            ExpectedInput = expectedInput,
            Status = status,
        };
    }

    private static string CombinedEvidenceStatus(IEnumerable<RuntimeReadinessEvidenceItem> evidence, params string[] ids)
    {
        var statuses = ids.Select(id => EvidenceStatus(evidence, id)).ToArray();
        if (statuses.Any(status => status == "MissingEvidence"))
        {
            return "MissingEvidence";
        }
        if (statuses.Any(status => status == "Blocked"))
        {
            return "Blocked";
        }
        if (statuses.Any(status => status == "PartiallyProven"))
        {
            return "PartiallyProven";
        }
        return "Passed";
    }

    private static string EvidenceStatus(IEnumerable<RuntimeReadinessEvidenceItem> evidence, string id) =>
        evidence.FirstOrDefault(item => item.EvidenceId == id)?.ReadinessStatus ?? "MissingEvidence";

    private static string MapEvidenceStatus(string status) =>
        status switch
        {
            "Passed" or "ReadyForPlanning" or "ReadyForImplementationPlanning" or "SourceTruthFoundationEstablished" => "Passed",
            "PartiallyProven" or "Deferred" or "Unclaimed" => "PartiallyProven",
            "Failed" or "Blocked" => "Blocked",
            _ => "MissingEvidence",
        };

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

    private static string CategoryForCode(string code) =>
        code.Contains("RuntimeReadinessV2", StringComparison.Ordinal) ? "RuntimeReadinessV2" : "RuntimeReadiness";

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

    private sealed record EvidenceSpec(
        string Id,
        string EvidenceScope,
        string Command,
        string FileName);
}
