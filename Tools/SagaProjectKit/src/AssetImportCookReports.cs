using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class AssetImportCookReports
{
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
        "No raw full CTest, heavy stress, or real transport proof is claimed.",
    ];

    public static AssetImportCookPrerequisiteReport BuildAssetImportCookPrerequisite(
        ManifestLoadResult load,
        string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var fullRoot = FullRoot(load, evidenceRoot);
        var reports = ReadReports(load, fullRoot, diagnostics, AssetImportCookInputs(), "AssetImportCook");
        var assetInventory = ReadJson(Path.Combine(fullRoot, "asset_source_manifest_inventory_report.json"), diagnostics, "Project.AssetImportCook.AssetInventory.Malformed");
        var assetReferenceGate = ReadJson(Path.Combine(fullRoot, "asset_reference_gate_report.json"), diagnostics, "Project.AssetImportCook.AssetReferenceGate.Malformed");
        var freshness = ReadJson(Path.Combine(fullRoot, "generated_artifact_freshness_report.json"), diagnostics, "Project.AssetImportCook.Freshness.Malformed");
        var deferral = ReadJson(Path.Combine(fullRoot, "asset_import_cook_deferral_report.json"), diagnostics, "Project.AssetImportCook.AssetImportCookDeferral.Malformed");

        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));
        if (readiness == "ReadyForImplementationPlanning" && (HasStaleFreshness(freshness) || HasDeferredPipeline(deferral)))
        {
            readiness = "PartiallyProven";
        }

        return new AssetImportCookPrerequisiteReport
        {
            Command = "evaluation-asset-import-cook-prerequisite",
            Status = readiness,
            ReadinessStatus = readiness,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredEvidence = reports,
            AssetSourceTruth = BuildAssetSourceTruth(load, assetInventory),
            AcceptedFutureEvaluationReferences = ReadResolvedReferences(assetReferenceGate),
            EvidenceOnlyArtifacts = BuildAssetEvidenceOnly(assetInventory, freshness),
            MissingPipelines =
            [
                new PipelinePrerequisiteItem { PipelineId = "asset-import", Kind = "AssetImportPipeline", ImplementationClaim = "NotImplemented", Status = "NotImplemented" },
                new PipelinePrerequisiteItem { PipelineId = "asset-cook", Kind = "AssetCookPipeline", ImplementationClaim = "NotImplemented", Status = "NotImplemented" },
            ],
            Checks =
            [
                Check("MissingRequiredEvidenceDoesNotPass", reports.Any(item => item.ReadinessStatus == "MissingEvidence") ? "MissingEvidence" : "Passed", "Missing prerequisite evidence prevents readiness."),
                Check("BlockedRequiredEvidenceDoesNotPass", reports.Any(item => item.ReadinessStatus == "Blocked") ? "Blocked" : "Passed", "Blocked prerequisite evidence prevents readiness."),
                Check("GeneratedAndPackageArtifactsEvidenceOnly", "Passed", "Generated and package artifacts are evidence only, never source truth."),
                Check("AssetImportNotImplemented", "Passed", "Asset import implementation remains NotImplemented."),
                Check("AssetCookNotImplemented", "Passed", "Asset cook implementation remains NotImplemented."),
                Check("ReportOnlyInvariants", "Passed", "The command reads project/evidence JSON and emits one report only."),
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static RuntimeAssetConsumptionPrerequisiteReport BuildRuntimeAssetConsumptionPrerequisite(
        ManifestLoadResult load,
        string sourceTruthRoot,
        string evaluationRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var fullSourceTruthRoot = FullRoot(load, sourceTruthRoot);
        var fullEvaluationRoot = FullRoot(load, evaluationRoot);
        var reports = ReadReports(load, fullSourceTruthRoot, diagnostics, RuntimeAssetSourceTruthInputs(), "RuntimeAssetConsumption")
            .Concat(ReadReports(load, fullEvaluationRoot, diagnostics, RuntimeAssetEvaluationInputs(), "RuntimeAssetConsumption"))
            .OrderBy(item => item.EvidenceId, StringComparer.Ordinal)
            .ToList();
        var assetReferenceGate = ReadJson(Path.Combine(fullSourceTruthRoot, "asset_reference_gate_report.json"), diagnostics, "Project.RuntimeAssetConsumption.AssetReferenceGate.Malformed");
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));

        return new RuntimeAssetConsumptionPrerequisiteReport
        {
            Command = "runtime-asset-consumption-prerequisite",
            Status = readiness,
            ReadinessStatus = readiness,
            SourceTruthRoot = NormalizeRelative(sourceTruthRoot),
            EvaluationRoot = NormalizeRelative(evaluationRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredEvidence = reports,
            FutureRuntimeEvaluationAssetNeeds =
            [
                new RuntimeReadInputItem { InputId = "accepted-asset-references", Kind = "AssetReferenceSourceTruth", Path = "asset_reference_gate_report.json", Classification = "FutureRuntimeEvaluationAssetNeed", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "source-controlled-asset-metadata", Kind = "AssetSourceManifest", Path = "Assets/assets.source.json", Classification = "SourceTruth", Status = "PlanOnly" },
                new RuntimeReadInputItem { InputId = "local-placeholder-asset-descriptors", Kind = "PlaceholderAssetDescriptor", Path = "Assets/README.md", Classification = "FutureLocalOnlyPlaceholder", Status = "PlanOnly" },
            ],
            AcceptedAssetReferences = ReadResolvedReferences(assetReferenceGate),
            ForbiddenRuntimeAssetInputs =
            [
                Forbidden("generated-asset-manifests", "GeneratedAssetManifest", "Generated/Assets", "Generated/package manifests cannot be consumed as canonical Runtime asset state."),
                Forbidden("package-asset-manifests", "PackageAssetManifest", "Build/Packages", "Package manifests are package evidence only."),
                Forbidden("asset-import-cook-outputs", "AssetPipelineOutput", "Future", "Asset import/cook outputs do not exist yet."),
                Forbidden("unresolved-asset-references", "UnresolvedAssetReference", "asset_reference_gate_report.json", "Unresolved references must not be consumed."),
                Forbidden("server-state", "ServerState", "Server", "Server state is outside Runtime evaluation asset source truth."),
                Forbidden("socket-session-transport", "TransportState", "Networking", "Sockets/session transport are outside no-network evaluation planning."),
                Forbidden("live-package-staging-output", "LivePackageStagingOutput", "Build/Packages", "Live staging output is not canonical state."),
            ],
            MissingRuntimeAssetConsumptionSeams =
            [
                new RuntimeReadAdapterAuditItem { AdapterId = "runtime-asset-reference-consumption-seam", SourceEvidenceId = "asset-reference-gate", ExpectedInput = "accepted asset references", Status = "NotImplemented" },
                new RuntimeReadAdapterAuditItem { AdapterId = "runtime-asset-metadata-consumption-seam", SourceEvidenceId = "asset-source-manifest-inventory", ExpectedInput = "assets.source.json", Status = "NotImplemented" },
            ],
            Checks =
            [
                Check("RuntimeAssetConsumptionSeamMissing", "NotImplemented", "Runtime asset consumption seam is not implemented."),
                Check("AssetsSourceJsonIsSourceTruth", "Passed", "assets.source.json remains source-controlled asset source truth."),
                Check("GeneratedPackageManifestsEvidenceOnly", "Passed", "Generated and package manifests are evidence only."),
                Check("RuntimeNotTouched", "Passed", "This report does not execute or modify Runtime."),
                Check("ReportOnlyInvariants", "Passed", "The command reads existing evidence and emits one report only."),
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static RuntimeProjectionFreshnessGateReport BuildRuntimeProjectionFreshnessGate(
        ManifestLoadResult load,
        string sourceTruthRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var fullRoot = FullRoot(load, sourceTruthRoot);
        var reports = ReadReports(load, fullRoot, diagnostics, RuntimeProjectionFreshnessInputs(), "RuntimeProjectionFreshness");
        var freshness = ReadJson(Path.Combine(fullRoot, "generated_artifact_freshness_report.json"), diagnostics, "Project.RuntimeProjectionFreshness.Freshness.Malformed");
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));
        if (readiness == "ReadyForImplementationPlanning" && HasStaleFreshness(freshness))
        {
            readiness = "PartiallyProven";
        }

        var staleProjections = ReadFreshnessArtifacts(freshness)
            .Where(item => item.Status == "Stale" || item.Status == "StaleGeneratedEvidence")
            .OrderBy(item => item.ArtifactId, StringComparer.Ordinal)
            .ToList();

        return new RuntimeProjectionFreshnessGateReport
        {
            Command = "runtime-projection-freshness-gate",
            Status = readiness,
            ReadinessStatus = readiness,
            SourceTruthRoot = NormalizeRelative(sourceTruthRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredEvidence = reports,
            RuntimeFacingProjectionReadiness = ReadFreshnessArtifacts(freshness)
                .OrderBy(item => item.ArtifactId, StringComparer.Ordinal)
                .Select(item => new RuntimeReadInputItem
                {
                    InputId = item.ArtifactId,
                    Kind = "GeneratedProjectionEvidence",
                    Path = item.Path,
                    Classification = "NonCanonicalRuntimeFacingProjectionEvidence",
                    Status = item.Status,
                })
                .ToList(),
            StaleSourceInputs = staleProjections.Count == 0 ? [] : ReadFreshnessSourceInputs(freshness),
            StaleGeneratedProjections = staleProjections,
            NonCanonicalGeneratedProjections = ReadFreshnessArtifacts(freshness)
                .OrderBy(item => item.ArtifactId, StringComparer.Ordinal)
                .Select(item => new GeneratedEvidenceOwnershipItem
                {
                    SceneId = item.SceneId,
                    ArtifactId = item.ArtifactId,
                    Path = item.Path,
                    ArtifactKind = "GeneratedProjection",
                    Classification = "EvidenceOnly",
                    Canonical = false,
                    Status = item.Status,
                })
                .ToList(),
            Checks =
            [
                Check("GeneratedProjectionsNonCanonical", "Passed", "All generated projections remain non-canonical, including fresh projections."),
                Check("StaleProjectionEvidence", staleProjections.Count > 0 ? "PartiallyProven" : "Passed", "Stale generated projections keep Runtime projection freshness partial."),
                Check("RuntimeImplementationNotClaimed", "Passed", "No Runtime implementation or Runtime gameplay is claimed."),
                Check("ReportOnlyInvariants", "Passed", "The command reads source-truth evidence and emits one report only."),
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static ClientEvaluationRegressionFixturePlanReport BuildClientEvaluationRegressionFixturePlan(
        ManifestLoadResult load,
        string evaluationRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var fullRoot = FullRoot(load, evaluationRoot);
        var reports = ReadReports(load, fullRoot, diagnostics, ClientRegressionFixtureInputs(), "ClientRegressionFixture");
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));

        return new ClientEvaluationRegressionFixturePlanReport
        {
            Command = "client-evaluation-regression-fixture-plan",
            Status = readiness,
            ReadinessStatus = readiness,
            EvaluationRoot = NormalizeRelative(evaluationRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredReports = reports,
            Fixtures =
            [
                Fixture("valid-minimal-scene-entity", ["scene-entity-validation", "runtime-readiness-v2"], "Future evaluation accepts one valid minimal scene/entity fixture."),
                Fixture("stale-projection", ["runtime-projection-freshness-gate"], "Future evaluation surfaces stale projection evidence without consuming it as source truth."),
                Fixture("missing-asset", ["asset-import-cook-prerequisite", "runtime-asset-consumption-prerequisite"], "Future evaluation blocks unresolved or missing asset references."),
                Fixture("no-network-evaluation", ["client-evaluation-no-network-plan"], "Future evaluation opens no sockets and launches no server process."),
                Fixture("clienthost-not-implemented", ["minimal-clienthost-evaluation-shell-plan"], "Current fixture asserts ClientHost remains NotImplemented."),
                Fixture("runtime-read-seam-missing", ["minimal-runtime-read-seam-plan"], "Current fixture asserts Runtime read seam remains missing until implemented later."),
            ],
            ForbiddenExecution =
            [
                "No Runtime execution.",
                "No ClientHost execution.",
                "No client process launch.",
                "No server process launch.",
                "No sockets.",
                "No networking.",
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static EvaluationFocusedTestHealthGateReport BuildEvaluationFocusedTestHealthGate(
        ManifestLoadResult load,
        string evaluationRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var fullRoot = FullRoot(load, evaluationRoot);
        var reports = ReadReports(load, fullRoot, diagnostics, FocusedTestHealthInputs(), "FocusedTestHealth");
        var readiness = AggregateReadiness(reports.Select(item => item.ReadinessStatus));
        if (readiness == "ReadyForImplementationPlanning")
        {
            readiness = "PartiallyProven";
        }

        return new EvaluationFocusedTestHealthGateReport
        {
            Command = "evaluation-focused-test-health-gate",
            Status = readiness,
            ReadinessStatus = readiness,
            EvaluationRoot = NormalizeRelative(evaluationRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredReports = reports,
            FocusedEvidence =
            [
                new TestHealthEvidenceItem { EvidenceId = "sagaprojectkit-focused-cli-coverage", Path = "Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py", Status = File.Exists("Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py") ? "Present" : "Missing", Classification = "RequiredFocusedEvidence" },
            ],
            OptionalEvidence =
            [
                new TestHealthEvidenceItem { EvidenceId = "sagapackager-focused-tests", Path = "Tools/SagaPackager/tests/test_sagapack_cli.py", Status = File.Exists("Tools/SagaPackager/tests/test_sagapack_cli.py") ? "PresentOptional" : "NotRelevantYet", Classification = "OptionalEvidence" },
            ],
            UnclaimedEvidence =
            [
                new TestHealthEvidenceItem { EvidenceId = "raw-full-ctest", Path = "CTest", Status = "Unclaimed", Classification = "UnclaimedEvidence" },
                new TestHealthEvidenceItem { EvidenceId = "heavy-stress", Path = "stress/load/slow suites", Status = "Unclaimed", Classification = "UnclaimedEvidence" },
                new TestHealthEvidenceItem { EvidenceId = "soak", Path = "soak suites", Status = "Unclaimed", Classification = "UnclaimedEvidence" },
                new TestHealthEvidenceItem { EvidenceId = "bot-swarm", Path = "bot swarm suites", Status = "Unclaimed", Classification = "UnclaimedEvidence" },
                new TestHealthEvidenceItem { EvidenceId = "real-transport-proof", Path = "real transport stress", Status = "Unclaimed", Classification = "UnclaimedEvidence" },
            ],
            Checks =
            [
                Check("RequiredEvaluationEvidence", readiness, "Required evaluation/prerequisite reports determine gate readiness."),
                Check("SagaProjectKitFocusedCoverage", File.Exists("Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py") ? "Passed" : "MissingEvidence", "SagaProjectKit focused CLI coverage is the required focused evidence."),
                Check("RawFullCTestUnclaimed", "Unclaimed", "Raw full CTest remains unclaimed."),
                Check("HeavyStressUnclaimed", "Unclaimed", "Heavy stress remains unclaimed."),
                Check("RealTransportProofUnclaimed", "Unclaimed", "Real transport proof remains unclaimed."),
                Check("ReportOnlyInvariants", "Passed", "The command reads evidence and emits one report only."),
            ],
            NonClaims = NonClaims,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static List<ReportSpec> AssetImportCookInputs() =>
    [
        new("asset-source-manifest-inventory", "asset-source-manifest-inventory", "asset_source_manifest_inventory_report.json"),
        new("asset-reference-gate", "asset-reference-gate", "asset_reference_gate_report.json"),
        new("asset-import-cook-deferral", "asset-import-cook-deferral", "asset_import_cook_deferral_report.json"),
        new("package-source-truth-alignment", "source-truth-alignment", "package_source_truth_alignment_report.json"),
        new("generated-artifact-freshness", "generated-freshness-gate", "generated_artifact_freshness_report.json"),
    ];

    private static List<ReportSpec> RuntimeAssetSourceTruthInputs() =>
    [
        new("asset-source-manifest-inventory", "asset-source-manifest-inventory", "asset_source_manifest_inventory_report.json"),
        new("asset-reference-gate", "asset-reference-gate", "asset_reference_gate_report.json"),
        new("generated-artifact-freshness", "generated-freshness-gate", "generated_artifact_freshness_report.json"),
    ];

    private static List<ReportSpec> RuntimeAssetEvaluationInputs() =>
    [
        new("minimal-runtime-read-seam-plan", "minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
        new("asset-import-cook-prerequisite", "evaluation-asset-import-cook-prerequisite", "asset_import_cook_prerequisite_report.json"),
    ];

    private static List<ReportSpec> RuntimeProjectionFreshnessInputs() =>
    [
        new("source-truth-inventory", "source-truth-inventory", "source_truth_inventory_report.json"),
        new("scene-entity-validation", "scene-entity-validate", "scene_entity_validation_report.json"),
        new("generated-artifact-freshness", "generated-freshness-gate", "generated_artifact_freshness_report.json"),
        new("runtime-readiness-gate", "runtime-readiness-gate", "runtime_readiness_gate_report.json"),
    ];

    private static List<ReportSpec> ClientRegressionFixtureInputs() =>
    [
        new("runtime-readiness-v2", "runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        new("clienthost-evaluation-ownership-boundary", "clienthost-evaluation-ownership-boundary", "clienthost_evaluation_ownership_boundary_report.json"),
        new("client-evaluation-launch-profile-contract", "client-evaluation-launch-profile-contract", "client_evaluation_launch_profile_contract_report.json"),
        new("client-evaluation-no-network-plan", "client-evaluation-no-network-plan", "client_evaluation_no_network_plan_report.json"),
        new("client-evaluation-diagnostics-shell", "client-evaluation-diagnostics-shell", "client_evaluation_diagnostics_shell_report.json"),
        new("client-evaluation-blocker-matrix", "client-evaluation-blocker-matrix", "client_evaluation_blocker_matrix_report.json"),
        new("minimal-runtime-read-seam-plan", "minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
        new("minimal-clienthost-evaluation-shell-plan", "minimal-clienthost-evaluation-shell-plan", "minimal_clienthost_evaluation_shell_plan_report.json"),
        new("package-launch-evaluation-alignment-plan", "package-launch-evaluation-alignment-plan", "package_launch_evaluation_alignment_plan_report.json"),
        new("editorless-evaluation-workflow-plan", "editorless-evaluation-workflow-plan", "editorless_evaluation_workflow_plan_report.json"),
        new("evaluation-evidence-gate", "evaluation-evidence-gate", "evaluation_evidence_gate_report.json"),
        new("asset-import-cook-prerequisite", "evaluation-asset-import-cook-prerequisite", "asset_import_cook_prerequisite_report.json"),
        new("runtime-asset-consumption-prerequisite", "runtime-asset-consumption-prerequisite", "runtime_asset_consumption_prerequisite_report.json"),
        new("runtime-projection-freshness-gate", "runtime-projection-freshness-gate", "runtime_projection_freshness_gate_report.json"),
    ];

    private static List<ReportSpec> FocusedTestHealthInputs() =>
    [
        .. ClientRegressionFixtureInputs(),
        new("client-evaluation-regression-fixture-plan", "client-evaluation-regression-fixture-plan", "client_evaluation_regression_fixture_plan_report.json"),
    ];

    private static List<ReportEvidenceStatusItem> ReadReports(
        ManifestLoadResult load,
        string fullRoot,
        List<ProjectDiagnostic> diagnostics,
        IEnumerable<ReportSpec> specs,
        string diagnosticCategory)
    {
        return specs
            .OrderBy(item => item.Id, StringComparer.Ordinal)
            .Select(spec =>
            {
                var path = Path.Combine(fullRoot, spec.FileName);
                var status = ReadReport(load, path, spec.Command, diagnostics, diagnosticCategory);
                return new ReportEvidenceStatusItem
                {
                    EvidenceId = spec.Id,
                    Command = spec.Command,
                    Path = NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)),
                    SourceStatus = status.SourceStatus,
                    ReadinessStatus = status.ReadinessStatus,
                    Required = true,
                };
            })
            .ToList();
    }

    private static ReportStatus ReadReport(
        ManifestLoadResult load,
        string path,
        string expectedCommand,
        List<ProjectDiagnostic> diagnostics,
        string diagnosticCategory)
    {
        if (!File.Exists(path))
        {
            diagnostics.Add(Error($"Project.{diagnosticCategory}.{expectedCommand}.Missing", diagnosticCategory, NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)), "Required evidence report is missing."));
            return new ReportStatus("Missing", "MissingEvidence");
        }

        var root = ReadJson(path, diagnostics, $"Project.{diagnosticCategory}.{expectedCommand}.Malformed");
        if (root is null)
        {
            return new ReportStatus("Malformed", "MissingEvidence");
        }

        var command = ReadString(root, "command");
        if (!string.IsNullOrWhiteSpace(command) && command != expectedCommand)
        {
            diagnostics.Add(Error($"Project.{diagnosticCategory}.{expectedCommand}.UnexpectedCommand", diagnosticCategory, NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)), $"Expected command {expectedCommand}, found {command}."));
            return new ReportStatus(command, "Blocked");
        }

        var sourceStatus = FirstNonEmpty(ReadString(root, "readinessStatus"), FirstNonEmpty(ReadString(root, "status"), ReadString(root, "outcome")));
        if (string.IsNullOrWhiteSpace(sourceStatus))
        {
            sourceStatus = "Missing";
        }

        var readiness = MapReadiness(sourceStatus);
        if (readiness == "Blocked")
        {
            diagnostics.Add(Error($"Project.{diagnosticCategory}.{expectedCommand}.Blocked", diagnosticCategory, NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)), "Required evidence report is failed or blocked."));
        }
        return new ReportStatus(sourceStatus, readiness);
    }

    private static List<PrerequisiteEvidenceItem> BuildAssetSourceTruth(ManifestLoadResult load, JsonObject? inventory)
    {
        var result = new List<PrerequisiteEvidenceItem>();
        result.AddRange((load.Descriptor?.Assets ?? [])
            .OrderBy(item => item.Id, StringComparer.Ordinal)
            .Select(item => new PrerequisiteEvidenceItem
            {
                ItemId = item.Id,
                Kind = "AssetRoot",
                Path = NormalizeRelative(item.Path),
                Classification = "SourceAssetRoot",
                Canonical = true,
                Status = "Declared",
            }));

        result.AddRange(ReadObjectArray(inventory, "assetSourceManifests")
            .Where(item => ReadString(item, "classification") == "SourceControlledAssetMetadata")
            .Select(item => new PrerequisiteEvidenceItem
            {
                ItemId = ReadString(item, "manifestId"),
                Kind = "AssetSourceManifest",
                Path = ReadString(item, "relativePath"),
                Classification = ReadString(item, "classification"),
                Canonical = ReadBool(item, "canonical"),
                Status = ReadString(item, "status"),
            }));

        result.AddRange(ReadObjectArray(inventory, "assetOwners")
            .Select(item => new PrerequisiteEvidenceItem
            {
                ItemId = ReadString(item, "assetId"),
                Kind = "AcceptedAssetOwner",
                Path = ReadString(item, "path"),
                Classification = ReadString(item, "classification"),
                Canonical = true,
                Status = ReadString(item, "status"),
            }));

        return result
            .OrderBy(item => item.Kind, StringComparer.Ordinal)
            .ThenBy(item => item.ItemId, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ToList();
    }

    private static List<PrerequisiteEvidenceItem> BuildAssetEvidenceOnly(JsonObject? inventory, JsonObject? freshness)
    {
        var result = ReadObjectArray(inventory, "generatedArtifacts")
            .Select(item => new PrerequisiteEvidenceItem
            {
                ItemId = ReadString(item, "artifactId"),
                Kind = "GeneratedAssetArtifact",
                Path = ReadString(item, "path"),
                Classification = "EvidenceOnly",
                Canonical = false,
                Status = ReadString(item, "status"),
            })
            .ToList();

        result.Add(new PrerequisiteEvidenceItem
        {
            ItemId = "package-generated-manifests",
            Kind = "PackageGeneratedManifest",
            Path = "Build/Packages",
            Classification = "EvidenceOnly",
            Canonical = false,
            Status = "EvidenceOnly",
        });

        result.AddRange(ReadFreshnessArtifacts(freshness)
            .Select(item => new PrerequisiteEvidenceItem
            {
                ItemId = item.ArtifactId,
                Kind = "GeneratedProjection",
                Path = item.Path,
                Classification = "EvidenceOnly",
                Canonical = false,
                Status = item.Status,
            }));

        return result
            .OrderBy(item => item.Kind, StringComparer.Ordinal)
            .ThenBy(item => item.ItemId, StringComparer.Ordinal)
            .ToList();
    }

    private static List<ResolvedAssetReferenceItem> ReadResolvedReferences(JsonObject? root) =>
        ReadObjectArray(root, "resolvedReferences")
            .Select(item => new ResolvedAssetReferenceItem
            {
                SceneId = ReadString(item, "sceneId"),
                EntityId = ReadString(item, "entityId"),
                AssetId = ReadString(item, "assetId"),
                Owner = ReadString(item, "owner"),
                Path = ReadString(item, "path"),
                Resolution = ReadString(item, "resolution"),
            })
            .OrderBy(item => item.SceneId, StringComparer.Ordinal)
            .ThenBy(item => item.EntityId, StringComparer.Ordinal)
            .ThenBy(item => item.AssetId, StringComparer.Ordinal)
            .ToList();

    private static List<GeneratedFreshnessArtifactItem> ReadFreshnessArtifacts(JsonObject? root) =>
        ReadObjectArray(root, "generatedArtifacts")
            .Select(item => new GeneratedFreshnessArtifactItem
            {
                SceneId = ReadString(item, "sceneId"),
                ArtifactId = ReadString(item, "artifactId"),
                Path = ReadString(item, "path"),
                ExpectedSourceHash = ReadString(item, "expectedSourceHash"),
                ActualSourceHash = ReadString(item, "actualSourceHash"),
                Status = ReadString(item, "status"),
            })
            .ToList();

    private static List<GeneratedFreshnessSourceInputItem> ReadFreshnessSourceInputs(JsonObject? root) =>
        ReadObjectArray(root, "sourceInputs")
            .Select(item => new GeneratedFreshnessSourceInputItem
            {
                SceneId = ReadString(item, "sceneId"),
                SourceHash = ReadString(item, "sourceHash"),
                RelativePath = ReadString(item, "relativePath"),
            })
            .OrderBy(item => item.SceneId, StringComparer.Ordinal)
            .ThenBy(item => item.RelativePath, StringComparer.Ordinal)
            .ToList();

    private static bool HasStaleFreshness(JsonObject? root) =>
        ReadObjectArray(root, "freshnessStatus").Any(item => ReadString(item, "status") is "Stale" or "StaleGeneratedEvidence") ||
        ReadFreshnessArtifacts(root).Any(item => item.Status is "Stale" or "StaleGeneratedEvidence");

    private static bool HasDeferredPipeline(JsonObject? root) =>
        ReadString(root, "outcome") == "Deferred" ||
        ReadObjectArray(root, "deferredPipelines").Any(item => ReadString(item, "status") == "Deferred");

    private static ClientEvaluationRegressionFixtureItem Fixture(string fixtureId, List<string> requiredEvidence, string expectedFutureAssertion) =>
        new()
        {
            FixtureId = fixtureId,
            RequiredEvidence = requiredEvidence.OrderBy(item => item, StringComparer.Ordinal).ToList(),
            ExpectedFutureAssertion = expectedFutureAssertion,
            CurrentStatus = "PlanOnly",
            ExecutionStatus = "NotExecuted",
        };

    private static RuntimeForbiddenSourceTruthItem Forbidden(string inputId, string kind, string path, string reason) =>
        new() { InputId = inputId, Kind = kind, Path = path, Reason = reason };

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
            "PartiallyProven" or "Deferred" or "Unclaimed" or "PlanOnly" or "NotRelevantYet" => "PartiallyProven",
            "Blocked" or "Failed" => "Blocked",
            _ => "MissingEvidence",
        };

    private static JsonObject? ReadJson(string path, List<ProjectDiagnostic> diagnostics, string code)
    {
        try
        {
            if (!File.Exists(path))
            {
                return null;
            }

            return JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Error(code, "AssetImportCookWorkflow", path, $"Report JSON could not be read: {e.Message}"));
            return null;
        }
    }

    private static List<JsonObject> ReadObjectArray(JsonObject? root, string key) =>
        root?[key] is JsonArray array
            ? array.OfType<JsonObject>().ToList()
            : [];

    private static string FullRoot(ManifestLoadResult load, string root)
    {
        if (Path.IsPathRooted(root))
        {
            return Path.GetFullPath(root);
        }

        var projectRelative = Path.GetFullPath(Path.Combine(load.ProjectRoot, root));
        if (Directory.Exists(projectRelative))
        {
            return projectRelative;
        }

        return Path.GetFullPath(root);
    }

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";

    private static bool ReadBool(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<bool>(out var result) &&
        result;

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
