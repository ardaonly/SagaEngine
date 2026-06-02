using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class Program
{
    private const int Passed = 0;
    private const int ValidationFailed = 1;
    private const int InvalidUsage = 2;
    private const int MissingInput = 3;
    private const int InternalToolError = 4;

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 0 || args[0] is "-h" or "--help" or "help")
            {
                PrintUsage();
                return args.Length == 0 ? InvalidUsage : Passed;
            }

            var command = args[0];
            if (command is not ("validate" or "resolve" or "doctor" or "resolve-slice" or "restricted-resolve" or
                "source-truth-inventory" or "source-truth-gate" or "hedef4-opening" or
                "asset-source-manifest-inventory" or "asset-reference-gate" or
                "generated-freshness-gate" or "scene-entity-validate" or
                "component-metadata-ownership" or "editor-inspection-model" or
                "editor-source-truth-read" or "runtime-read-model-audit" or
                "runtime-readiness-gate" or "source-truth-scenario" or
                "client-preview-prerequisite-audit" or "clienthost-boundary-plan" or
                "asset-import-cook-deferral" or "broad-test-health-preflight" or
                "hedef4-evidence-matrix" or "hedef4-closure" or
                "runtime-readiness-v2" or "clienthost-preview-ownership-boundary" or
                "client-preview-launch-profile-contract" or "client-preview-no-network-plan" or
                "client-preview-diagnostics-shell" or "client-preview-blocker-matrix" or
                "minimal-runtime-read-seam-plan" or "minimal-clienthost-preview-shell-plan" or
                "package-launch-preview-alignment-plan" or "editorless-preview-workflow-plan" or
                "preview-evidence-gate" or "preview-asset-import-cook-prerequisite" or
                "runtime-asset-consumption-prerequisite" or "runtime-projection-freshness-gate" or
                "client-preview-regression-fixture-plan" or "preview-focused-test-health-gate"))
            {
                Console.Error.WriteLine($"Unknown command: {command}");
                PrintUsage();
                return InvalidUsage;
            }

            var options = ParseOptions(args.Skip(1).ToArray());
            if (!options.Ok)
            {
                Console.Error.WriteLine(options.Error);
                return InvalidUsage;
            }
            if (string.IsNullOrWhiteSpace(options.ProjectPath))
            {
                Console.Error.WriteLine("--project is required.");
                return InvalidUsage;
            }
            if (string.IsNullOrWhiteSpace(options.OutputPath))
            {
                Console.Error.WriteLine("--out is required.");
                return InvalidUsage;
            }
            if (!File.Exists(options.ProjectPath) && !Directory.Exists(options.ProjectPath))
            {
                WriteMissingInputReport(command, options);
                return MissingInput;
            }

            return command switch
            {
                "validate" => RunValidate(options),
                "resolve" => RunResolve(options),
                "doctor" => RunDoctor(options),
                "resolve-slice" => RunResolveSlice(options),
                "restricted-resolve" => RunRestrictedResolve(options),
                "source-truth-inventory" => RunSourceTruthInventory(options),
                "source-truth-gate" => RunSourceTruthGate(options),
                "hedef4-opening" => RunHedef4Opening(options),
                "asset-source-manifest-inventory" => RunAssetSourceManifestInventory(options),
                "asset-reference-gate" => RunAssetReferenceGate(options),
                "generated-freshness-gate" => RunGeneratedFreshnessGate(options),
                "scene-entity-validate" => RunSceneEntityValidate(options),
                "component-metadata-ownership" => RunComponentMetadataOwnership(options),
                "editor-inspection-model" => RunEditorInspectionModel(options),
                "editor-source-truth-read" => RunEditorSourceTruthRead(options),
                "runtime-read-model-audit" => RunRuntimeReadModelAudit(options),
                "runtime-readiness-gate" => RunRuntimeReadinessGate(options),
                "source-truth-scenario" => RunSourceTruthScenario(options),
                "client-preview-prerequisite-audit" => RunClientPreviewPrerequisiteAudit(options),
                "clienthost-boundary-plan" => RunClientHostBoundaryPlan(options),
                "asset-import-cook-deferral" => RunAssetImportCookDeferral(options),
                "broad-test-health-preflight" => RunBroadTestHealthPreflight(options),
                "hedef4-evidence-matrix" => RunHedef4EvidenceMatrix(options),
                "hedef4-closure" => RunHedef4Closure(options),
                "runtime-readiness-v2" => RunRuntimeReadinessV2(options),
                "clienthost-preview-ownership-boundary" => RunClientHostPreviewOwnershipBoundary(options),
                "client-preview-launch-profile-contract" => RunClientPreviewLaunchProfileContract(options),
                "client-preview-no-network-plan" => RunClientPreviewNoNetworkPlan(options),
                "client-preview-diagnostics-shell" => RunClientPreviewDiagnosticsShell(options),
                "client-preview-blocker-matrix" => RunClientPreviewBlockerMatrix(options),
                "minimal-runtime-read-seam-plan" => RunMinimalRuntimeReadSeamPlan(options),
                "minimal-clienthost-preview-shell-plan" => RunMinimalClientHostPreviewShellPlan(options),
                "package-launch-preview-alignment-plan" => RunPackageLaunchPreviewAlignmentPlan(options),
                "editorless-preview-workflow-plan" => RunEditorlessPreviewWorkflowPlan(options),
                "preview-evidence-gate" => RunPreviewEvidenceGate(options),
                "preview-asset-import-cook-prerequisite" => RunAssetImportCookPrerequisite(options),
                "runtime-asset-consumption-prerequisite" => RunRuntimeAssetConsumptionPrerequisite(options),
                "runtime-projection-freshness-gate" => RunRuntimeProjectionFreshnessGate(options),
                "client-preview-regression-fixture-plan" => RunClientPreviewRegressionFixturePlan(options),
                "preview-focused-test-health-gate" => RunPreviewFocusedTestHealthGate(options),
                _ => InvalidUsage,
            };
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagaproject error: {e.Message}");
            return InternalToolError;
        }
    }

    private static int RunValidate(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = BuildReport("validate", load);
        ReportWriter.Write(options.OutputPath, report);
        return load.Passed ? Passed : ValidationFailed;
    }

    private static int RunResolve(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = new ProjectResolutionReport
        {
            Command = "resolve",
            Status = load.Passed ? "Passed" : "Failed",
            Project = BuildProjectSummary(load),
            Diagnostics = load.Diagnostics,
            ResolvedPaths = load.ResolvedPaths,
        };
        ReportWriter.Write(options.OutputPath, report);
        return load.Passed ? Passed : ValidationFailed;
    }

    private static int RunDoctor(CommandOptions options)
    {
        var loader = new ProjectManifestLoader();
        var load = loader.Load(options.ProjectPath, checkReferences: true);
        var diagnostics = load.Diagnostics.Concat(loader.CheckDoctorProfileJson(load))
            .OrderByDescending(d => d.Severity == "Error")
            .ThenBy(d => d.Code, StringComparer.Ordinal)
            .ThenBy(d => d.Path, StringComparer.Ordinal)
            .ThenBy(d => d.Message, StringComparer.Ordinal)
            .ToList();
        var report = new ProjectReport
        {
            Command = "doctor",
            Status = diagnostics.Any(d => d.Severity == "Error") ? "Failed" : "Passed",
            Project = BuildProjectSummary(load),
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Passed" ? Passed : ValidationFailed;
    }

    private static int RunResolveSlice(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SlicePath))
        {
            Console.Error.WriteLine("resolve-slice requires --slice.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var resolved = new ProjectSliceResolver().Resolve(load, options.SlicePath);
        var report = new ProjectSliceResolutionReport
        {
            Command = "resolve-slice",
            Status = resolved.Passed ? "Passed" : "Failed",
            Project = BuildProjectSummary(load),
            Slice = resolved.Slice,
            Diagnostics = resolved.Diagnostics,
            VisibleArtifacts = resolved.VisibleArtifacts,
            RestrictedArtifacts = resolved.RestrictedArtifacts,
            MissingArtifacts = resolved.MissingArtifacts,
            ExcludedArtifacts = resolved.ExcludedArtifacts,
            ResourceVisibility = resolved.ResourceVisibility,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
        ReportWriter.Write(options.OutputPath, report);
        return resolved.Passed ? Passed : ValidationFailed;
    }

    private static int RunRestrictedResolve(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SlicePath))
        {
            Console.Error.WriteLine("restricted-resolve requires --slice.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var resolved = new ProjectSliceResolver().Resolve(load, options.SlicePath);
        var report = new RestrictedProjectResolutionReport
        {
            Command = "restricted-resolve",
            Status = resolved.Passed ? "Passed" : "Failed",
            Project = BuildProjectSummary(load),
            Slice = resolved.Slice,
            Diagnostics = resolved.Diagnostics,
            ResolvedResources = resolved.VisibleArtifacts,
            RestrictedResources = resolved.RestrictedArtifacts
                .Concat(resolved.MissingArtifacts)
                .Concat(resolved.ExcludedArtifacts)
                .OrderBy(resource => resource.Kind, StringComparer.Ordinal)
                .ThenBy(resource => resource.Id, StringComparer.Ordinal)
                .ThenBy(resource => resource.RelativePath, StringComparer.Ordinal)
                .ToList(),
            ResourceVisibility = resolved.ResourceVisibility,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
        ReportWriter.Write(options.OutputPath, report);
        return resolved.Passed ? Passed : ValidationFailed;
    }

    private static int RunSourceTruthInventory(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = BuildSourceTruthInventory(load);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunSourceTruthGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.InventoryReportPath))
        {
            Console.Error.WriteLine("source-truth-gate requires --inventory-report.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = BuildSourceTruthGate(load, options.InventoryReportPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunHedef4Opening(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.InventoryReportPath) ||
            string.IsNullOrWhiteSpace(options.GateReportPath))
        {
            Console.Error.WriteLine("hedef4-opening requires --inventory-report and --gate-report.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = BuildHedef4Opening(load, options.InventoryReportPath, options.GateReportPath, options.EnterpriseClosurePath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunAssetSourceManifestInventory(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase134Reports.BuildAssetSourceManifestInventory(load);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunAssetReferenceGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SourceTruthInventoryPath) ||
            string.IsNullOrWhiteSpace(options.AssetManifestInventoryPath))
        {
            Console.Error.WriteLine("asset-reference-gate requires --source-truth-inventory and --asset-manifest-inventory.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase134Reports.BuildAssetReferenceGate(load, options.SourceTruthInventoryPath, options.AssetManifestInventoryPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunGeneratedFreshnessGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.InventoryPath))
        {
            Console.Error.WriteLine("generated-freshness-gate requires --inventory.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase134Reports.BuildGeneratedFreshnessGate(load, options.InventoryPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunSceneEntityValidate(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase134Reports.BuildSceneEntityValidation(load);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunComponentMetadataOwnership(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SceneValidationPath) ||
            string.IsNullOrWhiteSpace(options.FreshnessPath))
        {
            Console.Error.WriteLine("component-metadata-ownership requires --scene-validation and --freshness.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase141Reports.BuildComponentMetadataOwnership(load, options.SceneValidationPath, options.FreshnessPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunEditorInspectionModel(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SceneValidationPath) ||
            string.IsNullOrWhiteSpace(options.FreshnessPath))
        {
            Console.Error.WriteLine("editor-inspection-model requires --scene-validation and --freshness.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase141Reports.BuildEditorInspectionModel(load, options.SceneValidationPath, options.FreshnessPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunEditorSourceTruthRead(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.InspectionModelPath))
        {
            Console.Error.WriteLine("editor-source-truth-read requires --inspection-model.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase141Reports.BuildEditorSourceTruthRead(load, options.InspectionModelPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunRuntimeReadModelAudit(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SceneValidationPath) ||
            string.IsNullOrWhiteSpace(options.FreshnessPath))
        {
            Console.Error.WriteLine("runtime-read-model-audit requires --scene-validation and --freshness.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase141Reports.BuildRuntimeReadModelAudit(load, options.SceneValidationPath, options.FreshnessPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunRuntimeReadinessGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SceneValidationPath) ||
            string.IsNullOrWhiteSpace(options.FreshnessPath) ||
            string.IsNullOrWhiteSpace(options.RuntimeReadModelPath))
        {
            Console.Error.WriteLine("runtime-readiness-gate requires --scene-validation, --freshness, and --runtime-read-model.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase141Reports.BuildRuntimeReadinessGate(load, options.SceneValidationPath, options.FreshnessPath, options.RuntimeReadModelPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunSourceTruthScenario(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("source-truth-scenario requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase141Reports.BuildSourceTruthScenario(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunClientPreviewPrerequisiteAudit(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("client-preview-prerequisite-audit requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase149Reports.BuildClientPreviewPrerequisiteAudit(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunClientHostBoundaryPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ClientPreviewPrerequisitePath))
        {
            Console.Error.WriteLine("clienthost-boundary-plan requires --client-preview-prerequisite.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase149Reports.BuildClientHostBoundaryPlan(load, options.ClientPreviewPrerequisitePath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunAssetImportCookDeferral(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.AssetManifestInventoryPath))
        {
            Console.Error.WriteLine("asset-import-cook-deferral requires --asset-manifest-inventory.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase149Reports.BuildAssetImportCookDeferral(load, options.AssetManifestInventoryPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunBroadTestHealthPreflight(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("broad-test-health-preflight requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase149Reports.BuildBroadTestHealthPreflight(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunHedef4EvidenceMatrix(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("hedef4-evidence-matrix requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase149Reports.BuildHedef4EvidenceMatrix(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunHedef4Closure(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("hedef4-closure requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase149Reports.BuildHedef4Closure(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunRuntimeReadinessV2(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("runtime-readiness-v2 requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase156Reports.BuildRuntimeReadinessV2(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientHostPreviewOwnershipBoundary(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.RuntimeReadinessV2Path))
        {
            Console.Error.WriteLine("clienthost-preview-ownership-boundary requires --runtime-readiness-v2.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase161Reports.BuildClientHostPreviewOwnershipBoundary(load, options.RuntimeReadinessV2Path);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientPreviewLaunchProfileContract(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.RuntimeReadinessV2Path))
        {
            Console.Error.WriteLine("client-preview-launch-profile-contract requires --runtime-readiness-v2.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase161Reports.BuildClientPreviewLaunchProfileContract(load, options.RuntimeReadinessV2Path);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientPreviewNoNetworkPlan(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase161Reports.BuildClientPreviewNoNetworkPlan(load);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientPreviewDiagnosticsShell(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("client-preview-diagnostics-shell requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase161Reports.BuildClientPreviewDiagnosticsShell(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientPreviewBlockerMatrix(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("client-preview-blocker-matrix requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase161Reports.BuildClientPreviewBlockerMatrix(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunMinimalRuntimeReadSeamPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("minimal-runtime-read-seam-plan requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase166Reports.BuildMinimalRuntimeReadSeamPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunMinimalClientHostPreviewShellPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("minimal-clienthost-preview-shell-plan requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase166Reports.BuildMinimalClientHostPreviewShellPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunPackageLaunchPreviewAlignmentPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("package-launch-preview-alignment-plan requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase166Reports.BuildPackageLaunchPreviewAlignmentPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunEditorlessPreviewWorkflowPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("editorless-preview-workflow-plan requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase166Reports.BuildEditorlessPreviewWorkflowPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunPreviewEvidenceGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("preview-evidence-gate requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase166Reports.BuildPreviewEvidenceGate(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunAssetImportCookPrerequisite(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("preview-asset-import-cook-prerequisite requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase171Reports.BuildAssetImportCookPrerequisite(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunRuntimeAssetConsumptionPrerequisite(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SourceTruthRootPath) ||
            string.IsNullOrWhiteSpace(options.PreviewRootPath))
        {
            Console.Error.WriteLine("runtime-asset-consumption-prerequisite requires --source-truth-root and --preview-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase171Reports.BuildRuntimeAssetConsumptionPrerequisite(load, options.SourceTruthRootPath, options.PreviewRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunRuntimeProjectionFreshnessGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SourceTruthRootPath))
        {
            Console.Error.WriteLine("runtime-projection-freshness-gate requires --source-truth-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase171Reports.BuildRuntimeProjectionFreshnessGate(load, options.SourceTruthRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientPreviewRegressionFixturePlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.PreviewRootPath))
        {
            Console.Error.WriteLine("client-preview-regression-fixture-plan requires --preview-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase171Reports.BuildClientPreviewRegressionFixturePlan(load, options.PreviewRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunPreviewFocusedTestHealthGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.PreviewRootPath))
        {
            Console.Error.WriteLine("preview-focused-test-health-gate requires --preview-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = Phase171Reports.BuildPreviewFocusedTestHealthGate(load, options.PreviewRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static SourceTruthInventoryReport BuildSourceTruthInventory(ManifestLoadResult load)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var scenes = new List<SceneSourceTruthItem>();
        var entities = new List<EntitySourceTruthItem>();
        var assetReferences = new List<AssetReferenceTruthItem>();
        var generatedArtifacts = new List<GeneratedArtifactTruthItem>();
        var readBoundaries = new List<ReadBoundaryTruthItem>();

        var sceneRefs = load.ResolvedPaths
            .Where(path => path.Category == "scene")
            .OrderBy(path => path.Id, StringComparer.Ordinal)
            .ThenBy(path => path.RelativePath, StringComparer.Ordinal)
            .ToArray();

        if (sceneRefs.Length == 0)
        {
            diagnostics.Add(Error(
                "Project.SourceTruth.MissingSceneSourceTruth",
                "SourceTruth",
                load.ManifestPath,
                "Project manifest does not declare scene source truth."));
        }

        foreach (var sceneRef in sceneRefs)
        {
            if (!sceneRef.Exists)
            {
                diagnostics.Add(Error(
                    "Project.SourceTruth.MissingSceneSourceTruth",
                    "SourceTruth",
                    sceneRef.RelativePath,
                    "Declared scene source truth file is missing."));
                scenes.Add(new SceneSourceTruthItem
                {
                    SceneId = sceneRef.Id,
                    ProjectReferenceId = sceneRef.Id,
                    RelativePath = sceneRef.RelativePath,
                    Status = "MissingSourceTruth",
                });
                continue;
            }

            JsonDocument document;
            try
            {
                using var input = File.OpenRead(sceneRef.AbsolutePath);
                document = JsonDocument.Parse(input);
            }
            catch (JsonException e)
            {
                diagnostics.Add(Error(
                    "Project.SourceTruth.SceneMalformed",
                    "SourceTruth",
                    sceneRef.RelativePath,
                    $"Scene source truth JSON is invalid: {e.Message}"));
                scenes.Add(new SceneSourceTruthItem
                {
                    SceneId = sceneRef.Id,
                    ProjectReferenceId = sceneRef.Id,
                    RelativePath = sceneRef.RelativePath,
                    Status = "Malformed",
                });
                continue;
            }

            using (document)
            {
                var root = document.RootElement;
                if (root.ValueKind != JsonValueKind.Object)
                {
                    diagnostics.Add(Error(
                        "Project.SourceTruth.SceneMalformed",
                        "SourceTruth",
                        sceneRef.RelativePath,
                        "Scene source truth root must be a JSON object."));
                    continue;
                }

                var sceneId = ReadString(root, "sceneId");
                if (string.IsNullOrWhiteSpace(sceneId))
                {
                    sceneId = sceneRef.Id;
                    diagnostics.Add(Error(
                        "Project.SourceTruth.SceneIdMissing",
                        "SourceTruth",
                        sceneRef.RelativePath,
                        "Scene source truth must declare sceneId."));
                }

                var sourceKind = ReadString(root, "sourceKind");
                var sceneStatus = sourceKind == "SceneSourceTruth" ? "Passed" : "MissingSourceTruth";
                if (sceneStatus != "Passed")
                {
                    diagnostics.Add(Error(
                        "Project.SourceTruth.SceneSourceKindMissing",
                        "SourceTruth",
                        sceneRef.RelativePath,
                        "Scene source truth must declare sourceKind SceneSourceTruth."));
                }

                scenes.Add(new SceneSourceTruthItem
                {
                    SceneId = sceneId,
                    ProjectReferenceId = sceneRef.Id,
                    RelativePath = sceneRef.RelativePath,
                    Status = sceneStatus,
                });

                if (!root.TryGetProperty("entities", out var entityArray) || entityArray.ValueKind != JsonValueKind.Array || entityArray.GetArrayLength() == 0)
                {
                    diagnostics.Add(Error(
                        "Project.SourceTruth.MissingEntitySourceTruth",
                        "SourceTruth",
                        sceneRef.RelativePath,
                        "Scene source truth must declare at least one entity."));
                }
                else
                {
                    foreach (var entity in entityArray.EnumerateArray().Where(item => item.ValueKind == JsonValueKind.Object))
                    {
                        AddEntityTruth(sceneRef, sceneId, entity, entities, assetReferences, diagnostics);
                    }
                }

                AddGeneratedArtifacts(load.ProjectRoot, sceneRef, sceneId, root, generatedArtifacts, diagnostics);
                AddReadBoundaries(sceneRef, root, readBoundaries, diagnostics);
            }
        }

        var summary = BuildSourceTruthSummary(scenes, entities, assetReferences, generatedArtifacts, readBoundaries);
        var status = DetermineSourceTruthStatus(diagnostics, summary);
        return new SourceTruthInventoryReport
        {
            Command = "source-truth-inventory",
            Status = status,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            Summary = summary,
            Scenes = scenes.OrderBy(item => item.SceneId, StringComparer.Ordinal).ToList(),
            Entities = entities.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ToList(),
            AssetReferences = assetReferences.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            GeneratedArtifacts = generatedArtifacts.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.ArtifactId, StringComparer.Ordinal).ToList(),
            ReadBoundaries = readBoundaries.OrderBy(item => item.Boundary, StringComparer.Ordinal).ToList(),
            DiagnosticRules = SourceTruthDiagnosticRules,
            ResidualDebt = SourceTruthResidualDebt,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static SourceTruthGateReport BuildSourceTruthGate(ManifestLoadResult load, string inventoryPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var inventory = LoadJsonObject(inventoryPath, diagnostics, "Project.SourceTruth.InventoryMalformed");
        var checks = new List<SourceTruthGateCheck>();
        if (inventory is null)
        {
            checks.Add(Check("InventoryReportPresent", "Failed", "Inventory report is missing or malformed."));
        }
        else
        {
            checks.Add(Check("InventoryReportPresent", "Passed", "Inventory report is present."));
            checks.Add(Check("SceneSourceTruthExists", ArrayCount(inventory, "scenes") > 0 && AllItemStatus(inventory, "scenes", "Passed") ? "Passed" : "Failed", "Scene source truth files are classified as source truth."));
            checks.Add(Check("EntitySourceTruthExists", ArrayCount(inventory, "entities") > 0 && AllItemStatus(inventory, "entities", "Passed") ? "Passed" : "Failed", "Entity source truth records are classified as source truth."));
            checks.Add(Check("GeneratedArtifactsNonCanonical", ArrayCount(inventory, "generatedArtifacts") > 0 && AllGeneratedArtifactsNonCanonical(inventory) ? "Passed" : "Failed", "Generated artifacts are classified as non-canonical evidence."));
            checks.Add(Check("AssetReferencesOwned", ArrayCount(inventory, "assetReferences") > 0 && AllAssetReferencesOwned(inventory) ? "Passed" : "Failed", "Asset references have declared ownership."));
            checks.Add(Check("MissingSourceTruthDiagnosticsRegistered", HasDiagnosticRule(inventory, "Project.SourceTruth.MissingSceneSourceTruth") ? "Passed" : "Failed", "Missing source truth diagnostics are registered."));
            checks.Add(Check("StaleGeneratedEvidenceDiagnosed", HasDiagnosticRule(inventory, "Project.SourceTruth.GeneratedArtifactStale") ? "Passed" : "Failed", "Stale generated evidence diagnostics are registered."));
            checks.Add(Check("RuntimeEditorReadBoundariesReportOnly", HasBoundary(inventory, "runtime", "DocumentedReportOnly") && HasBoundary(inventory, "editor", "DocumentedReportOnly") ? "Passed" : "Failed", "Runtime and Editor read boundaries remain documented/report-only."));
            checks.Add(Check("ClientPreviewDeferred", HasBoundary(inventory, "clientPreview", "Deferred") ? "Passed" : "Failed", "Client preview remains deferred."));
            checks.Add(Check("ReportOnlyInvariants", ReadBool(inventory, "localOnly") && ReadString(inventory, "networkExposure") == "None" && !ReadBool(inventory, "mutatesSource") && ReadString(inventory, "enforcement") == "ReportOnly" ? "Passed" : "Failed", "Report-only invariants are preserved."));
        }

        var inventoryStatus = inventory is null ? "Failed" : ReadString(inventory, "status");
        var status = checks.Any(item => item.Status == "Failed") ? "Failed" :
            inventoryStatus == "PartiallyProven" ? "PartiallyProven" :
            "Passed";
        return new SourceTruthGateReport
        {
            Command = "source-truth-gate",
            Status = status,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            Summary = inventory is null ? new SourceTruthSummary() : ReadSummary(inventory),
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            ResidualDebt = SourceTruthResidualDebt,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static Hedef4OpeningReport BuildHedef4Opening(ManifestLoadResult load, string inventoryPath, string gatePath, string enterpriseClosurePath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var inventory = LoadJsonObject(inventoryPath, diagnostics, "Project.SourceTruth.InventoryMalformed");
        var gate = LoadJsonObject(gatePath, diagnostics, "Project.SourceTruth.GateMalformed");
        var closure = string.IsNullOrWhiteSpace(enterpriseClosurePath)
            ? null
            : LoadJsonObject(enterpriseClosurePath, diagnostics, "Project.SourceTruth.EnterpriseClosureMalformed");

        var inventoryStatus = inventory is null ? "MissingEvidence" : ReadString(inventory, "status");
        var gateStatus = gate is null ? "MissingEvidence" : ReadString(gate, "status");
        if (inventory is null)
        {
            diagnostics.Add(Error("Project.SourceTruth.InventoryMissing", "SourceTruth", inventoryPath, "Hedef 4 opening requires source truth inventory evidence."));
        }
        if (gate is null)
        {
            diagnostics.Add(Error("Project.SourceTruth.GateMissing", "SourceTruth", gatePath, "Hedef 4 opening requires source truth gate evidence."));
        }

        var residualDebt = ReadStringArray(closure, "residualDebt");
        if (residualDebt.Count == 0)
        {
            residualDebt = SourceTruthResidualDebt;
        }

        var status = diagnostics.Any(item => item.Severity == "Error") || gateStatus == "Failed" || inventoryStatus == "Failed"
            ? "Failed"
            : "PartiallyProven";
        return new Hedef4OpeningReport
        {
            Command = "hedef4-opening",
            Status = status,
            Outcome = status == "Failed" ? "Blocked" : "PartiallyProven",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            OpenedPhases =
            [
                "Phase 126 - Hedef 4 scope freeze and claim boundary",
                "Phase 127 - Scene and Entity source-truth contract",
                "Phase 128 - Generated artifact and asset-reference ownership contract",
                "Phase 129 - Runtime and Editor read-boundary contract",
                "Phase 130 - Local source-truth fixture set",
                "Phase 131 - Report-only source-truth inventory",
                "Phase 132 - Report-only source-truth gate",
                "Phase 133 - Hedef 4 opening acceptance report",
            ],
            ReservedFollowUpPhases = ["Phase 134-155 reserved for follow-up planning only"],
            InventoryStatus = inventoryStatus,
            GateStatus = gateStatus,
            Docs =
            [
                "docs/roadmaps/SAGAENGINE_TARGET4_SOURCE_TRUTH_FOUNDATION_ROADMAP.md",
                "docs/architecture/SCENE_ENTITY_SOURCE_TRUTH_FOUNDATION.md",
            ],
            Reports = [inventoryPath, gatePath],
            ResidualDebt = residualDebt,
            NonClaims =
            [
                "No productization milestone is claimed.",
                "No release-track milestone is claimed.",
                "No production readiness is claimed.",
                "No ClientHost, Runtime gameplay, Server gameplay, Editor UI, Qt UI, asset import, or asset cook implementation is claimed.",
                "No raw full CTest, heavy stress, soak, bot swarm, or real transport stress proof is claimed.",
            ],
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static ProjectReport BuildReport(string command, ManifestLoadResult load)
    {
        return new ProjectReport
        {
            Command = command,
            Status = load.Passed ? "Passed" : "Failed",
            Project = BuildProjectSummary(load),
            Diagnostics = load.Diagnostics,
        };
    }

    private static ProjectSummary BuildProjectSummary(ManifestLoadResult load)
    {
        return new ProjectSummary
        {
            ProjectId = load.Descriptor?.ProjectId ?? "",
            DisplayName = load.Descriptor?.DisplayName ?? "",
            ProjectRoot = load.ProjectRoot,
            ManifestPath = load.ManifestPath,
            SchemaVersion = load.Descriptor?.SchemaVersion ?? 0,
        };
    }

    private static readonly List<string> SourceTruthDiagnosticRules =
    [
        "Project.SourceTruth.MissingSceneSourceTruth",
        "Project.SourceTruth.MissingEntitySourceTruth",
        "Project.SourceTruth.GeneratedArtifactStale",
        "Project.SourceTruth.AssetReferenceOwnerMissing",
        "Project.SourceTruth.ReadBoundaryMissing",
    ];

    private static readonly List<string> SourceTruthResidualDebt =
    [
        "Hedef 3 asset workflow source truth debt remains visible.",
        "ClientHost preview remains deferred.",
        "Editor UI and Qt UI remain deferred.",
        "Runtime gameplay and Server gameplay remain deferred.",
        "Asset import and asset cook remain deferred.",
        "Raw full CTest, heavy stress, and real transport proof remain unclaimed.",
    ];

    private static void AddEntityTruth(
        ResolvedProjectPath sceneRef,
        string sceneId,
        JsonElement entity,
        List<EntitySourceTruthItem> entities,
        List<AssetReferenceTruthItem> assetReferences,
        List<ProjectDiagnostic> diagnostics)
    {
        var entityId = ReadString(entity, "entityId");
        if (string.IsNullOrWhiteSpace(entityId))
        {
            diagnostics.Add(Error(
                "Project.SourceTruth.EntityIdMissing",
                "SourceTruth",
                sceneRef.RelativePath,
                "Entity source truth must declare entityId."));
            entityId = "missing-entity-id";
        }

        var sourceKind = ReadString(entity, "sourceKind");
        var status = sourceKind == "EntitySourceTruth" ? "Passed" : "MissingSourceTruth";
        if (status != "Passed")
        {
            diagnostics.Add(Error(
                "Project.SourceTruth.EntitySourceKindMissing",
                "SourceTruth",
                sceneRef.RelativePath,
                "Entity source truth must declare sourceKind EntitySourceTruth."));
        }

        entities.Add(new EntitySourceTruthItem
        {
            SceneId = sceneId,
            EntityId = entityId,
            RelativePath = sceneRef.RelativePath,
            Status = status,
        });

        if (!entity.TryGetProperty("assetReferences", out var refs) || refs.ValueKind != JsonValueKind.Array)
        {
            return;
        }

        foreach (var assetRef in refs.EnumerateArray().Where(item => item.ValueKind == JsonValueKind.Object))
        {
            var assetId = ReadString(assetRef, "assetId");
            var owner = ReadString(assetRef, "owner");
            var path = ReadString(assetRef, "path");
            var itemStatus = string.IsNullOrWhiteSpace(owner) ? "MissingOwner" : "Passed";
            if (itemStatus != "Passed")
            {
                diagnostics.Add(Error(
                    "Project.SourceTruth.AssetReferenceOwnerMissing",
                    "SourceTruth",
                    sceneRef.RelativePath,
                    "Scene/entity asset reference must declare an owner."));
            }

            assetReferences.Add(new AssetReferenceTruthItem
            {
                SceneId = sceneId,
                EntityId = entityId,
                AssetId = assetId,
                Owner = owner,
                Path = path,
                Status = itemStatus,
            });
        }
    }

    private static void AddGeneratedArtifacts(
        string projectRoot,
        ResolvedProjectPath sceneRef,
        string sceneId,
        JsonElement root,
        List<GeneratedArtifactTruthItem> generatedArtifacts,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!root.TryGetProperty("generatedArtifacts", out var artifacts) || artifacts.ValueKind != JsonValueKind.Array)
        {
            return;
        }

        foreach (var artifact in artifacts.EnumerateArray().Where(item => item.ValueKind == JsonValueKind.Object))
        {
            var artifactId = ReadString(artifact, "artifactId");
            var path = ReadString(artifact, "path");
            var expectedHash = ReadString(artifact, "expectedSourceHash");
            var canonical = ReadBool(artifact, "canonical");
            var actualHash = "";
            var status = canonical ? "InvalidCanonical" : "Passed";
            if (canonical)
            {
                diagnostics.Add(Error(
                    "Project.SourceTruth.GeneratedArtifactCanonical",
                    "SourceTruth",
                    sceneRef.RelativePath,
                    "Generated artifact must not be marked canonical source truth."));
            }

            var artifactPath = Path.GetFullPath(Path.Combine(projectRoot, path));
            if (path.Length == 0 || !File.Exists(artifactPath))
            {
                status = "MissingGeneratedEvidence";
                diagnostics.Add(Warning(
                    "Project.SourceTruth.GeneratedArtifactMissing",
                    "SourceTruth",
                    path.Length == 0 ? sceneRef.RelativePath : path,
                    "Generated artifact evidence is missing."));
            }
            else
            {
                var generated = LoadJsonElement(artifactPath, diagnostics, "Project.SourceTruth.GeneratedArtifactMalformed");
                if (generated.HasValue)
                {
                    actualHash = ReadString(generated.Value, "sourceHash");
                    if (expectedHash.Length > 0 && actualHash.Length > 0 && expectedHash != actualHash)
                    {
                        status = "StaleGeneratedEvidence";
                        diagnostics.Add(Warning(
                            "Project.SourceTruth.GeneratedArtifactStale",
                            "SourceTruth",
                            path,
                            "Generated artifact source hash does not match scene source truth."));
                    }
                }
            }

            generatedArtifacts.Add(new GeneratedArtifactTruthItem
            {
                SceneId = sceneId,
                ArtifactId = artifactId,
                Path = path,
                Canonical = canonical,
                ExpectedSourceHash = expectedHash,
                ActualSourceHash = actualHash,
                Status = status,
            });
        }
    }

    private static void AddReadBoundaries(
        ResolvedProjectPath sceneRef,
        JsonElement root,
        List<ReadBoundaryTruthItem> readBoundaries,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!root.TryGetProperty("readBoundaries", out var boundaries) || boundaries.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(Error(
                "Project.SourceTruth.ReadBoundaryMissing",
                "SourceTruth",
                sceneRef.RelativePath,
                "Scene source truth must document read boundaries."));
            return;
        }

        foreach (var boundaryName in new[] { "runtime", "editor", "clientPreview" })
        {
            if (!boundaries.TryGetProperty(boundaryName, out var boundary) || boundary.ValueKind != JsonValueKind.Object)
            {
                diagnostics.Add(Error(
                    "Project.SourceTruth.ReadBoundaryMissing",
                    "SourceTruth",
                    sceneRef.RelativePath,
                    $"Read boundary is missing: {boundaryName}."));
                continue;
            }

            var status = ReadString(boundary, "status");
            var expectedStatus = boundaryName == "clientPreview" ? "Deferred" : "DocumentedReportOnly";
            if (status != expectedStatus)
            {
                diagnostics.Add(Error(
                    "Project.SourceTruth.ReadBoundaryInvalid",
                    "SourceTruth",
                    sceneRef.RelativePath,
                    $"{boundaryName} read boundary must be {expectedStatus}."));
            }

            readBoundaries.Add(new ReadBoundaryTruthItem
            {
                Boundary = boundaryName,
                Status = status,
                Enforcement = "ReportOnly",
                Summary = ReadString(boundary, "summary"),
            });
        }
    }

    private static SourceTruthSummary BuildSourceTruthSummary(
        IReadOnlyList<SceneSourceTruthItem> scenes,
        IReadOnlyList<EntitySourceTruthItem> entities,
        IReadOnlyList<AssetReferenceTruthItem> assetReferences,
        IReadOnlyList<GeneratedArtifactTruthItem> generatedArtifacts,
        IReadOnlyList<ReadBoundaryTruthItem> readBoundaries) =>
        new()
        {
            Scenes = scenes.Count,
            Entities = entities.Count,
            AssetReferences = assetReferences.Count,
            GeneratedArtifacts = generatedArtifacts.Count,
            SourceTruthFiles = scenes.Count(item => item.Status == "Passed") + entities.Count(item => item.Status == "Passed"),
            NonCanonicalGeneratedArtifacts = generatedArtifacts.Count(item => !item.Canonical),
            MissingSourceTruth = scenes.Count(item => item.Status == "MissingSourceTruth") + entities.Count(item => item.Status == "MissingSourceTruth"),
            StaleGeneratedEvidence = generatedArtifacts.Count(item => item.Status == "StaleGeneratedEvidence"),
            DeferredReadBoundaries = readBoundaries.Count(item => item.Status == "Deferred"),
        };

    private static string DetermineSourceTruthStatus(IReadOnlyList<ProjectDiagnostic> diagnostics, SourceTruthSummary summary)
    {
        if (diagnostics.Any(item => item.Severity == "Error"))
        {
            return "Failed";
        }
        return summary.StaleGeneratedEvidence > 0 || summary.DeferredReadBoundaries > 0
            ? "PartiallyProven"
            : "Passed";
    }

    private static JsonElement? LoadJsonElement(string path, List<ProjectDiagnostic> diagnostics, string code)
    {
        try
        {
            using var input = File.OpenRead(path);
            using var document = JsonDocument.Parse(input);
            return document.RootElement.Clone();
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Warning(code, "SourceTruth", path, $"JSON evidence could not be read: {e.Message}"));
            return null;
        }
    }

    private static JsonObject? LoadJsonObject(string path, List<ProjectDiagnostic> diagnostics, string code)
    {
        try
        {
            var node = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            if (node is null)
            {
                diagnostics.Add(Error(code, "SourceTruth", path, "Report JSON root must be an object."));
            }
            return node;
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Error(code, "SourceTruth", path, $"Report JSON could not be read: {e.Message}"));
            return null;
        }
    }

    private static SourceTruthGateCheck Check(string checkId, string status, string summary) =>
        new() { CheckId = checkId, Status = status, Summary = summary };

    private static int ArrayCount(JsonObject root, string key) =>
        root[key] is JsonArray array ? array.Count : 0;

    private static bool AllItemStatus(JsonObject root, string key, string status) =>
        root[key] is JsonArray array && array.Count > 0 && array.All(item => ReadString(item as JsonObject, "status") == status);

    private static bool AllGeneratedArtifactsNonCanonical(JsonObject root) =>
        root["generatedArtifacts"] is JsonArray array && array.Count > 0 && array.All(item => !ReadBool(item as JsonObject, "canonical"));

    private static bool AllAssetReferencesOwned(JsonObject root) =>
        root["assetReferences"] is JsonArray array && array.Count > 0 && array.All(item => !string.IsNullOrWhiteSpace(ReadString(item as JsonObject, "owner")));

    private static bool HasDiagnosticRule(JsonObject root, string rule) =>
        root["diagnosticRules"] is JsonArray array &&
        array.Any(item => item is JsonValue value && value.TryGetValue<string>(out var text) && text == rule);

    private static bool HasBoundary(JsonObject root, string boundary, string status) =>
        root["readBoundaries"] is JsonArray array &&
        array.Any(item => ReadString(item as JsonObject, "boundary") == boundary && ReadString(item as JsonObject, "status") == status);

    private static SourceTruthSummary ReadSummary(JsonObject root)
    {
        var summary = root["summary"] as JsonObject;
        return new SourceTruthSummary
        {
            Scenes = ReadInt(summary, "scenes"),
            Entities = ReadInt(summary, "entities"),
            AssetReferences = ReadInt(summary, "assetReferences"),
            GeneratedArtifacts = ReadInt(summary, "generatedArtifacts"),
            SourceTruthFiles = ReadInt(summary, "sourceTruthFiles"),
            NonCanonicalGeneratedArtifacts = ReadInt(summary, "nonCanonicalGeneratedArtifacts"),
            MissingSourceTruth = ReadInt(summary, "missingSourceTruth"),
            StaleGeneratedEvidence = ReadInt(summary, "staleGeneratedEvidence"),
            DeferredReadBoundaries = ReadInt(summary, "deferredReadBoundaries"),
        };
    }

    private static List<string> ReadStringArray(JsonObject? root, string key)
    {
        if (root?[key] is not JsonArray array)
        {
            return [];
        }

        return array
            .Select(item => item is JsonValue value && value.TryGetValue<string>(out var text) ? text : "")
            .Where(text => !string.IsNullOrWhiteSpace(text))
            .OrderBy(text => text, StringComparer.Ordinal)
            .ToList();
    }

    private static string ReadString(JsonElement element, string key) =>
        element.ValueKind == JsonValueKind.Object &&
        element.TryGetProperty(key, out var value) &&
        value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? ""
            : "";

    private static bool ReadBool(JsonElement element, string key) =>
        element.ValueKind == JsonValueKind.Object &&
        element.TryGetProperty(key, out var value) &&
        value.ValueKind is JsonValueKind.True or JsonValueKind.False &&
        value.GetBoolean();

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

    private static int ReadInt(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<int>(out var result)
            ? result
            : 0;

    private static ProjectDiagnostic Error(string code, string category, string path, string message) =>
        new() { Severity = "Error", Code = code, Category = category, Path = path, Message = message };

    private static ProjectDiagnostic Warning(string code, string category, string path, string message) =>
        new() { Severity = "Warning", Code = code, Category = category, Path = path, Message = message };

    private static List<ProjectDiagnostic> SortDiagnostics(IEnumerable<ProjectDiagnostic> diagnostics) =>
        diagnostics
            .OrderByDescending(item => item.Severity == "Error")
            .ThenByDescending(item => item.Severity == "Warning")
            .ThenBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal)
            .ToList();

    private static void WriteMissingInputReport(string command, CommandOptions options)
    {
        var report = new ProjectReport
        {
            Command = command,
            Status = "Failed",
            Diagnostics =
            [
                new ProjectDiagnostic
                {
                    Severity = "Error",
                    Code = "Project.Input.Missing",
                    Category = "Input",
                    Path = options.ProjectPath,
                    Message = "Project path does not exist.",
                },
            ],
        };
        ReportWriter.Write(options.OutputPath, report);
    }

    private static CommandOptions ParseOptions(string[] args)
    {
        var options = new CommandOptions();
        for (var i = 0; i < args.Length; ++i)
        {
            switch (args[i])
            {
                case "--project":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--project requires a value.");
                    }
                    options.ProjectPath = args[++i];
                    break;
                case "--out":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--out requires a value.");
                    }
                    options.OutputPath = args[++i];
                    break;
                case "--slice":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--slice requires a value.");
                    }
                    options.SlicePath = args[++i];
                    break;
                case "--inventory-report":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--inventory-report requires a value.");
                    }
                    options.InventoryReportPath = args[++i];
                    break;
                case "--gate-report":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--gate-report requires a value.");
                    }
                    options.GateReportPath = args[++i];
                    break;
                case "--enterprise-closure":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--enterprise-closure requires a value.");
                    }
                    options.EnterpriseClosurePath = args[++i];
                    break;
                case "--source-truth-inventory":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--source-truth-inventory requires a value.");
                    }
                    options.SourceTruthInventoryPath = args[++i];
                    break;
                case "--asset-manifest-inventory":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--asset-manifest-inventory requires a value.");
                    }
                    options.AssetManifestInventoryPath = args[++i];
                    break;
                case "--inventory":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--inventory requires a value.");
                    }
                    options.InventoryPath = args[++i];
                    break;
                case "--scene-validation":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--scene-validation requires a value.");
                    }
                    options.SceneValidationPath = args[++i];
                    break;
                case "--freshness":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--freshness requires a value.");
                    }
                    options.FreshnessPath = args[++i];
                    break;
                case "--inspection-model":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--inspection-model requires a value.");
                    }
                    options.InspectionModelPath = args[++i];
                    break;
                case "--runtime-read-model":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--runtime-read-model requires a value.");
                    }
                    options.RuntimeReadModelPath = args[++i];
                    break;
                case "--runtime-readiness-v2":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--runtime-readiness-v2 requires a value.");
                    }
                    options.RuntimeReadinessV2Path = args[++i];
                    break;
                case "--client-preview-prerequisite":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--client-preview-prerequisite requires a value.");
                    }
                    options.ClientPreviewPrerequisitePath = args[++i];
                    break;
                case "--evidence-root":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--evidence-root requires a value.");
                    }
                    options.EvidenceRootPath = args[++i];
                    break;
                case "--source-truth-root":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--source-truth-root requires a value.");
                    }
                    options.SourceTruthRootPath = args[++i];
                    break;
                case "--preview-root":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--preview-root requires a value.");
                    }
                    options.PreviewRootPath = args[++i];
                    break;
                default:
                    return CommandOptions.Fail($"Unknown argument: {args[i]}");
            }
        }
        options.Ok = true;
        return options;
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaProjectKit CLI");
        Console.Error.WriteLine("Usage:");
        Console.Error.WriteLine("  sagaproject validate --project <path> --out <report>");
        Console.Error.WriteLine("  sagaproject resolve --project <path> --out <report>");
        Console.Error.WriteLine("  sagaproject doctor --project <path> --out <report>");
        Console.Error.WriteLine("  sagaproject resolve-slice --project <path> --slice <project_slice.json> --out <report>");
        Console.Error.WriteLine("  sagaproject restricted-resolve --project <path> --slice <project_slice.json> --out <report>");
        Console.Error.WriteLine("  sagaproject source-truth-inventory --project <path> --out <source_truth_inventory_report.json>");
        Console.Error.WriteLine("  sagaproject source-truth-gate --project <path> --inventory-report <source_truth_inventory_report.json> --out <source_truth_gate_report.json>");
        Console.Error.WriteLine("  sagaproject hedef4-opening --project <path> --inventory-report <source_truth_inventory_report.json> --gate-report <source_truth_gate_report.json> [--enterprise-closure <enterprise_closure_report.json>] --out <hedef4_opening_report.json>");
        Console.Error.WriteLine("  sagaproject asset-source-manifest-inventory --project <path> --out <asset_source_manifest_inventory_report.json>");
        Console.Error.WriteLine("  sagaproject asset-reference-gate --project <path> --source-truth-inventory <source_truth_inventory_report.json> --asset-manifest-inventory <asset_source_manifest_inventory_report.json> --out <asset_reference_gate_report.json>");
        Console.Error.WriteLine("  sagaproject generated-freshness-gate --project <path> --inventory <source_truth_inventory_report.json> --out <generated_artifact_freshness_report.json>");
        Console.Error.WriteLine("  sagaproject scene-entity-validate --project <path> --out <scene_entity_validation_report.json>");
        Console.Error.WriteLine("  sagaproject component-metadata-ownership --project <path> --scene-validation <scene_entity_validation_report.json> --freshness <generated_artifact_freshness_report.json> --out <component_metadata_ownership_report.json>");
        Console.Error.WriteLine("  sagaproject editor-inspection-model --project <path> --scene-validation <scene_entity_validation_report.json> --freshness <generated_artifact_freshness_report.json> --out <editor_inspection_model_report.json>");
        Console.Error.WriteLine("  sagaproject editor-source-truth-read --project <path> --inspection-model <editor_inspection_model_report.json> --out <editor_source_truth_read_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-read-model-audit --project <path> --scene-validation <scene_entity_validation_report.json> --freshness <generated_artifact_freshness_report.json> --out <runtime_read_model_audit_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-readiness-gate --project <path> --scene-validation <scene_entity_validation_report.json> --freshness <generated_artifact_freshness_report.json> --runtime-read-model <runtime_read_model_audit_report.json> --out <runtime_readiness_gate_report.json>");
        Console.Error.WriteLine("  sagaproject source-truth-scenario --project <path> --evidence-root <Build/SourceTruth> --out <source_truth_scenario_report.json>");
        Console.Error.WriteLine("  sagaproject client-preview-prerequisite-audit --project <path> --evidence-root <Build/SourceTruth> --out <client_preview_prerequisite_audit_report.json>");
        Console.Error.WriteLine("  sagaproject clienthost-boundary-plan --project <path> --client-preview-prerequisite <client_preview_prerequisite_audit_report.json> --out <clienthost_boundary_plan_report.json>");
        Console.Error.WriteLine("  sagaproject asset-import-cook-deferral --project <path> --asset-manifest-inventory <asset_source_manifest_inventory_report.json> --out <asset_import_cook_deferral_report.json>");
        Console.Error.WriteLine("  sagaproject broad-test-health-preflight --project <path> --evidence-root <Build/SourceTruth> --out <broad_test_health_preflight_report.json>");
        Console.Error.WriteLine("  sagaproject hedef4-evidence-matrix --project <path> --evidence-root <Build/SourceTruth> --out <hedef4_evidence_matrix_report.json>");
        Console.Error.WriteLine("  sagaproject hedef4-closure --project <path> --evidence-root <Build/SourceTruth> --out <hedef4_closure_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-readiness-v2 --project <path> --evidence-root <Build/SourceTruth> --out <runtime_readiness_v2_report.json>");
        Console.Error.WriteLine("  sagaproject clienthost-preview-ownership-boundary --project <path> --runtime-readiness-v2 <runtime_readiness_v2_report.json> --out <clienthost_preview_ownership_boundary_report.json>");
        Console.Error.WriteLine("  sagaproject client-preview-launch-profile-contract --project <path> --runtime-readiness-v2 <runtime_readiness_v2_report.json> --out <client_preview_launch_profile_contract_report.json>");
        Console.Error.WriteLine("  sagaproject client-preview-no-network-plan --project <path> --out <client_preview_no_network_plan_report.json>");
        Console.Error.WriteLine("  sagaproject client-preview-diagnostics-shell --project <path> --evidence-root <Build/Preview> --out <client_preview_diagnostics_shell_report.json>");
        Console.Error.WriteLine("  sagaproject client-preview-blocker-matrix --project <path> --evidence-root <Build/Preview> --out <client_preview_blocker_matrix_report.json>");
        Console.Error.WriteLine("  sagaproject minimal-runtime-read-seam-plan --project <path> --evidence-root <Build/Preview> --out <minimal_runtime_read_seam_plan_report.json>");
        Console.Error.WriteLine("  sagaproject minimal-clienthost-preview-shell-plan --project <path> --evidence-root <Build/Preview> --out <minimal_clienthost_preview_shell_plan_report.json>");
        Console.Error.WriteLine("  sagaproject package-launch-preview-alignment-plan --project <path> --evidence-root <Build/Preview> --out <package_launch_preview_alignment_plan_report.json>");
        Console.Error.WriteLine("  sagaproject editorless-preview-workflow-plan --project <path> --evidence-root <Build/Preview> --out <editorless_preview_workflow_plan_report.json>");
        Console.Error.WriteLine("  sagaproject preview-evidence-gate --project <path> --evidence-root <Build/Preview> --out <preview_evidence_gate_report.json>");
        Console.Error.WriteLine("  sagaproject preview-asset-import-cook-prerequisite --project <path> --evidence-root <Build/SourceTruth> --out <asset_import_cook_prerequisite_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-asset-consumption-prerequisite --project <path> --source-truth-root <Build/SourceTruth> --preview-root <Build/Preview> --out <runtime_asset_consumption_prerequisite_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-projection-freshness-gate --project <path> --source-truth-root <Build/SourceTruth> --out <runtime_projection_freshness_gate_report.json>");
        Console.Error.WriteLine("  sagaproject client-preview-regression-fixture-plan --project <path> --preview-root <Build/Preview> --out <client_preview_regression_fixture_plan_report.json>");
        Console.Error.WriteLine("  sagaproject preview-focused-test-health-gate --project <path> --preview-root <Build/Preview> --out <preview_focused_test_health_gate_report.json>");
    }

    private sealed class CommandOptions
    {
        public bool Ok { get; set; }
        public string Error { get; set; } = "";
        public string ProjectPath { get; set; } = "";
        public string SlicePath { get; set; } = "";
        public string InventoryReportPath { get; set; } = "";
        public string GateReportPath { get; set; } = "";
        public string EnterpriseClosurePath { get; set; } = "";
        public string SourceTruthInventoryPath { get; set; } = "";
        public string AssetManifestInventoryPath { get; set; } = "";
        public string InventoryPath { get; set; } = "";
        public string SceneValidationPath { get; set; } = "";
        public string FreshnessPath { get; set; } = "";
        public string InspectionModelPath { get; set; } = "";
        public string RuntimeReadModelPath { get; set; } = "";
        public string RuntimeReadinessV2Path { get; set; } = "";
        public string ClientPreviewPrerequisitePath { get; set; } = "";
        public string EvidenceRootPath { get; set; } = "";
        public string SourceTruthRootPath { get; set; } = "";
        public string PreviewRootPath { get; set; } = "";
        public string OutputPath { get; set; } = "";

        public static CommandOptions Fail(string error)
        {
            return new CommandOptions { Ok = false, Error = error };
        }
    }
}
