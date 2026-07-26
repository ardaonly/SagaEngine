// SPDX-License-Identifier: MPL-2.0
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static partial class Program
{
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

    private static int RunAssetWorkflowOpening(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.InventoryReportPath) ||
            string.IsNullOrWhiteSpace(options.GateReportPath))
        {
            Console.Error.WriteLine("asset-workflow-opening requires --inventory-report and --gate-report.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = BuildAssetWorkflowOpening(load, options.InventoryReportPath, options.GateReportPath, options.EnterpriseClosurePath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunAssetSourceManifestInventory(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = AssetSourceManifestReports.BuildAssetSourceManifestInventory(load);
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
        var report = AssetSourceManifestReports.BuildAssetReferenceGate(load, options.SourceTruthInventoryPath, options.AssetManifestInventoryPath);
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
        var report = AssetSourceManifestReports.BuildGeneratedFreshnessGate(load, options.InventoryPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunSceneEntityValidate(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = AssetSourceManifestReports.BuildSceneEntityValidation(load);
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
        var report = MetadataOwnershipReports.BuildComponentMetadataOwnership(load, options.SceneValidationPath, options.FreshnessPath);
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
        var report = MetadataOwnershipReports.BuildEditorInspectionModel(load, options.SceneValidationPath, options.FreshnessPath);
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
        var report = MetadataOwnershipReports.BuildEditorSourceTruthRead(load, options.InspectionModelPath);
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
        var report = MetadataOwnershipReports.BuildRuntimeReadModelAudit(load, options.SceneValidationPath, options.FreshnessPath);
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
        var report = MetadataOwnershipReports.BuildRuntimeReadinessGate(load, options.SceneValidationPath, options.FreshnessPath, options.RuntimeReadModelPath);
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
        var report = MetadataOwnershipReports.BuildSourceTruthScenario(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunClientEvaluationPrerequisiteAudit(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("client-evaluation-prerequisite-audit requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientReadinessReports.BuildClientEvaluationPrerequisiteAudit(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunClientHostBoundaryPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ClientEvaluationPrerequisitePath))
        {
            Console.Error.WriteLine("clienthost-boundary-plan requires --client-evaluation-prerequisite.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientReadinessReports.BuildClientHostBoundaryPlan(load, options.ClientEvaluationPrerequisitePath);
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
        var report = ClientReadinessReports.BuildAssetImportCookDeferral(load, options.AssetManifestInventoryPath);
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
        var report = ClientReadinessReports.BuildBroadTestHealthPreflight(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunAssetWorkflowEvidenceMatrix(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("asset-workflow-evidence-matrix requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientReadinessReports.BuildAssetWorkflowEvidenceMatrix(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.Status == "Failed" ? ValidationFailed : Passed;
    }

    private static int RunAssetWorkflowClosure(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("asset-workflow-closure requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientReadinessReports.BuildAssetWorkflowClosure(load, options.EvidenceRootPath);
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
        var report = RuntimeReadinessReports.BuildRuntimeReadinessV2(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientHostEvaluationOwnershipBoundary(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.RuntimeReadinessV2Path))
        {
            Console.Error.WriteLine("clienthost-evaluation-ownership-boundary requires --runtime-readiness-v2.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientHostBoundaryReports.BuildClientHostEvaluationOwnershipBoundary(load, options.RuntimeReadinessV2Path);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientEvaluationLaunchProfileContract(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.RuntimeReadinessV2Path))
        {
            Console.Error.WriteLine("client-evaluation-launch-profile-contract requires --runtime-readiness-v2.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientHostBoundaryReports.BuildClientEvaluationLaunchProfileContract(load, options.RuntimeReadinessV2Path);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientEvaluationNoNetworkPlan(CommandOptions options)
    {
        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientHostBoundaryReports.BuildClientEvaluationNoNetworkPlan(load);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientEvaluationDiagnosticsShell(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("client-evaluation-diagnostics-shell requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientHostBoundaryReports.BuildClientEvaluationDiagnosticsShell(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientEvaluationBlockerMatrix(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("client-evaluation-blocker-matrix requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = ClientHostBoundaryReports.BuildClientEvaluationBlockerMatrix(load, options.EvidenceRootPath);
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
        var report = RuntimeReadSeamReports.BuildMinimalRuntimeReadSeamPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunMinimalClientHostEvaluationShellPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("minimal-clienthost-evaluation-shell-plan requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = RuntimeReadSeamReports.BuildMinimalClientHostEvaluationShellPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunPackageLaunchEvaluationAlignmentPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("package-launch-evaluation-alignment-plan requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = RuntimeReadSeamReports.BuildPackageLaunchEvaluationAlignmentPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunEditorlessEvaluationWorkflowPlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("editorless-evaluation-workflow-plan requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = RuntimeReadSeamReports.BuildEditorlessEvaluationWorkflowPlan(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunEvaluationEvidenceGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("evaluation-evidence-gate requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = RuntimeReadSeamReports.BuildEvaluationEvidenceGate(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunAssetImportCookPrerequisite(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRootPath))
        {
            Console.Error.WriteLine("evaluation-asset-import-cook-prerequisite requires --evidence-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = AssetImportCookReports.BuildAssetImportCookPrerequisite(load, options.EvidenceRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunRuntimeAssetConsumptionPrerequisite(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.SourceTruthRootPath) ||
            string.IsNullOrWhiteSpace(options.EvaluationRootPath))
        {
            Console.Error.WriteLine("runtime-asset-consumption-prerequisite requires --source-truth-root and --evaluation-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = AssetImportCookReports.BuildRuntimeAssetConsumptionPrerequisite(load, options.SourceTruthRootPath, options.EvaluationRootPath);
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
        var report = AssetImportCookReports.BuildRuntimeProjectionFreshnessGate(load, options.SourceTruthRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunClientEvaluationRegressionFixturePlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvaluationRootPath))
        {
            Console.Error.WriteLine("client-evaluation-regression-fixture-plan requires --evaluation-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = AssetImportCookReports.BuildClientEvaluationRegressionFixturePlan(load, options.EvaluationRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }

    private static int RunEvaluationFocusedTestHealthGate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvaluationRootPath))
        {
            Console.Error.WriteLine("evaluation-focused-test-health-gate requires --evaluation-root.");
            return InvalidUsage;
        }

        var load = new ProjectManifestLoader().Load(options.ProjectPath, checkReferences: true);
        var report = AssetImportCookReports.BuildEvaluationFocusedTestHealthGate(load, options.EvaluationRootPath);
        ReportWriter.Write(options.OutputPath, report);
        return report.ReadinessStatus is "Blocked" or "MissingEvidence" ? ValidationFailed : Passed;
    }
}
