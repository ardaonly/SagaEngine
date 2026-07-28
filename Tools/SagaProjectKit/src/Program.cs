// SPDX-License-Identifier: MPL-2.0
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static partial class Program
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
                "source-truth-inventory" or "source-truth-gate" or "asset-workflow-opening" or
                "asset-source-manifest-inventory" or "asset-reference-gate" or
                "generated-freshness-gate" or "scene-entity-validate" or
                "component-metadata-ownership" or "editor-inspection-model" or
                "editor-source-truth-read" or "runtime-read-model-audit" or
                "runtime-readiness-gate" or "source-truth-scenario" or
                "client-evaluation-prerequisite-audit" or "clienthost-boundary-plan" or
                "asset-import-cook-deferral" or "broad-test-health-preflight" or
                "asset-workflow-evidence-matrix" or "asset-workflow-closure" or
                "runtime-readiness-v2" or "clienthost-evaluation-ownership-boundary" or
                "client-evaluation-launch-profile-contract" or "client-evaluation-no-network-plan" or
                "client-evaluation-diagnostics-shell" or "client-evaluation-blocker-matrix" or
                "minimal-runtime-read-seam-plan" or "minimal-clienthost-evaluation-shell-plan" or
                "package-launch-evaluation-alignment-plan" or "editorless-evaluation-workflow-plan" or
                "evaluation-evidence-gate" or "evaluation-asset-import-cook-prerequisite" or
                "runtime-asset-consumption-prerequisite" or "runtime-projection-freshness-gate" or
                "client-evaluation-regression-fixture-plan" or "evaluation-focused-test-health-gate" or
                "template-plan" or "template-create"))
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
            if (command == "template-plan")
            {
                if (string.IsNullOrWhiteSpace(options.DestinationPath) || string.IsNullOrWhiteSpace(options.OutputPath))
                {
                    Console.Error.WriteLine("template-plan requires --destination and --out.");
                    return InvalidUsage;
                }
                var plan = ProjectTemplateService.Plan(options.DestinationPath, options.DisplayName, options.TemplatePath);
                SagaProjectKit.Core.CanonicalJson.Write(options.OutputPath, plan);
                return Passed;
            }
            if (command == "template-create")
            {
                var privateInvocation =
                    !string.IsNullOrWhiteSpace(options.DotnetHostPath) &&
                    !string.IsNullOrWhiteSpace(options.SagaPackDllPath);
                var legacyInvocation = !string.IsNullOrWhiteSpace(options.SagaPackPath);
                if (string.IsNullOrWhiteSpace(options.PlanPath) || string.IsNullOrWhiteSpace(options.OutputPath) ||
                    (!privateInvocation && !legacyInvocation) ||
                    privateInvocation == legacyInvocation ||
                    string.IsNullOrWhiteSpace(options.EngineRoot) ||
                    string.IsNullOrWhiteSpace(options.RuntimeBridgePath))
                {
                    Console.Error.WriteLine("template-create requires --plan, exactly one of private --dotnet-host plus --sagapack-dll or legacy --sagapack, --engine-root, --runtime-bridge, and --out.");
                    return InvalidUsage;
                }
                var report = ProjectTemplateService.Create(options.PlanPath,
                    privateInvocation ? options.DotnetHostPath : options.SagaPackPath,
                    privateInvocation ? options.SagaPackDllPath : "",
                    options.EngineRoot, options.RuntimeBridgePath);
                SagaProjectKit.Core.CanonicalJson.Write(options.OutputPath, report);
                return report.Status == "Passed" ? Passed : ValidationFailed;
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
                "asset-workflow-opening" => RunAssetWorkflowOpening(options),
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
                "client-evaluation-prerequisite-audit" => RunClientEvaluationPrerequisiteAudit(options),
                "clienthost-boundary-plan" => RunClientHostBoundaryPlan(options),
                "asset-import-cook-deferral" => RunAssetImportCookDeferral(options),
                "broad-test-health-preflight" => RunBroadTestHealthPreflight(options),
                "asset-workflow-evidence-matrix" => RunAssetWorkflowEvidenceMatrix(options),
                "asset-workflow-closure" => RunAssetWorkflowClosure(options),
                "runtime-readiness-v2" => RunRuntimeReadinessV2(options),
                "clienthost-evaluation-ownership-boundary" => RunClientHostEvaluationOwnershipBoundary(options),
                "client-evaluation-launch-profile-contract" => RunClientEvaluationLaunchProfileContract(options),
                "client-evaluation-no-network-plan" => RunClientEvaluationNoNetworkPlan(options),
                "client-evaluation-diagnostics-shell" => RunClientEvaluationDiagnosticsShell(options),
                "client-evaluation-blocker-matrix" => RunClientEvaluationBlockerMatrix(options),
                "minimal-runtime-read-seam-plan" => RunMinimalRuntimeReadSeamPlan(options),
                "minimal-clienthost-evaluation-shell-plan" => RunMinimalClientHostEvaluationShellPlan(options),
                "package-launch-evaluation-alignment-plan" => RunPackageLaunchEvaluationAlignmentPlan(options),
                "editorless-evaluation-workflow-plan" => RunEditorlessEvaluationWorkflowPlan(options),
                "evaluation-evidence-gate" => RunEvaluationEvidenceGate(options),
                "evaluation-asset-import-cook-prerequisite" => RunAssetImportCookPrerequisite(options),
                "runtime-asset-consumption-prerequisite" => RunRuntimeAssetConsumptionPrerequisite(options),
                "runtime-projection-freshness-gate" => RunRuntimeProjectionFreshnessGate(options),
                "client-evaluation-regression-fixture-plan" => RunClientEvaluationRegressionFixturePlan(options),
                "evaluation-focused-test-health-gate" => RunEvaluationFocusedTestHealthGate(options),
                _ => InvalidUsage,
            };
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagaproject error: {e.Message}");
            return InternalToolError;
        }
    }

}
