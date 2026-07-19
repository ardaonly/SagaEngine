// SPDX-License-Identifier: MPL-2.0
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static partial class Program
{
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
                case "--client-evaluation-prerequisite":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--client-evaluation-prerequisite requires a value.");
                    }
                    options.ClientEvaluationPrerequisitePath = args[++i];
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
                case "--evaluation-root":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--evaluation-root requires a value.");
                    }
                    options.EvaluationRootPath = args[++i];
                    break;
                case "--destination":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--destination requires a value.");
                    }
                    options.DestinationPath = args[++i];
                    break;
                case "--display-name":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--display-name requires a value.");
                    }
                    options.DisplayName = args[++i];
                    break;
                case "--template":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--template requires a value.");
                    }
                    options.TemplatePath = args[++i];
                    break;
                case "--plan":
                    if (i + 1 >= args.Length)
                    {
                        return CommandOptions.Fail("--plan requires a value.");
                    }
                    options.PlanPath = args[++i];
                    break;
                case "--sagapack":
                    if (i + 1 >= args.Length) return CommandOptions.Fail("--sagapack requires a value.");
                    options.SagaPackPath = args[++i];
                    break;
                case "--engine-root":
                    if (i + 1 >= args.Length) return CommandOptions.Fail("--engine-root requires a value.");
                    options.EngineRoot = args[++i];
                    break;
                case "--runtime-bridge":
                    if (i + 1 >= args.Length) return CommandOptions.Fail("--runtime-bridge requires a value.");
                    options.RuntimeBridgePath = args[++i];
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
        Console.Error.WriteLine("  sagaproject template-plan --destination <path> [--display-name <name>] [--template <seed>] --out <plan>");
        Console.Error.WriteLine("  sagaproject template-create --plan <plan> --sagapack <tool> --engine-root <root> --runtime-bridge <dll> --out <report>");
        Console.Error.WriteLine("  sagaproject resolve --project <path> --out <report>");
        Console.Error.WriteLine("  sagaproject doctor --project <path> --out <report>");
        Console.Error.WriteLine("  sagaproject resolve-slice --project <path> --slice <project_slice.json> --out <report>");
        Console.Error.WriteLine("  sagaproject restricted-resolve --project <path> --slice <project_slice.json> --out <report>");
        Console.Error.WriteLine("  sagaproject source-truth-inventory --project <path> --out <source_truth_inventory_report.json>");
        Console.Error.WriteLine("  sagaproject source-truth-gate --project <path> --inventory-report <source_truth_inventory_report.json> --out <source_truth_gate_report.json>");
        Console.Error.WriteLine("  sagaproject asset-workflow-opening --project <path> --inventory-report <source_truth_inventory_report.json> --gate-report <source_truth_gate_report.json> [--enterprise-closure <enterprise_closure_report.json>] --out <asset_workflow_opening_report.json>");
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
        Console.Error.WriteLine("  sagaproject client-evaluation-prerequisite-audit --project <path> --evidence-root <Build/SourceTruth> --out <client_evaluation_prerequisite_audit_report.json>");
        Console.Error.WriteLine("  sagaproject clienthost-boundary-plan --project <path> --client-evaluation-prerequisite <client_evaluation_prerequisite_audit_report.json> --out <clienthost_boundary_plan_report.json>");
        Console.Error.WriteLine("  sagaproject asset-import-cook-deferral --project <path> --asset-manifest-inventory <asset_source_manifest_inventory_report.json> --out <asset_import_cook_deferral_report.json>");
        Console.Error.WriteLine("  sagaproject broad-test-health-preflight --project <path> --evidence-root <Build/SourceTruth> --out <broad_test_health_preflight_report.json>");
        Console.Error.WriteLine("  sagaproject asset-workflow-evidence-matrix --project <path> --evidence-root <Build/SourceTruth> --out <asset_workflow_evidence_matrix_report.json>");
        Console.Error.WriteLine("  sagaproject asset-workflow-closure --project <path> --evidence-root <Build/SourceTruth> --out <asset_workflow_closure_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-readiness-v2 --project <path> --evidence-root <Build/SourceTruth> --out <runtime_readiness_v2_report.json>");
        Console.Error.WriteLine("  sagaproject clienthost-evaluation-ownership-boundary --project <path> --runtime-readiness-v2 <runtime_readiness_v2_report.json> --out <clienthost_evaluation_ownership_boundary_report.json>");
        Console.Error.WriteLine("  sagaproject client-evaluation-launch-profile-contract --project <path> --runtime-readiness-v2 <runtime_readiness_v2_report.json> --out <client_evaluation_launch_profile_contract_report.json>");
        Console.Error.WriteLine("  sagaproject client-evaluation-no-network-plan --project <path> --out <client_evaluation_no_network_plan_report.json>");
        Console.Error.WriteLine("  sagaproject client-evaluation-diagnostics-shell --project <path> --evidence-root <Build/Evaluation> --out <client_evaluation_diagnostics_shell_report.json>");
        Console.Error.WriteLine("  sagaproject client-evaluation-blocker-matrix --project <path> --evidence-root <Build/Evaluation> --out <client_evaluation_blocker_matrix_report.json>");
        Console.Error.WriteLine("  sagaproject minimal-runtime-read-seam-plan --project <path> --evidence-root <Build/Evaluation> --out <minimal_runtime_read_seam_plan_report.json>");
        Console.Error.WriteLine("  sagaproject minimal-clienthost-evaluation-shell-plan --project <path> --evidence-root <Build/Evaluation> --out <minimal_clienthost_evaluation_shell_plan_report.json>");
        Console.Error.WriteLine("  sagaproject package-launch-evaluation-alignment-plan --project <path> --evidence-root <Build/Evaluation> --out <package_launch_evaluation_alignment_plan_report.json>");
        Console.Error.WriteLine("  sagaproject editorless-evaluation-workflow-plan --project <path> --evidence-root <Build/Evaluation> --out <editorless_evaluation_workflow_plan_report.json>");
        Console.Error.WriteLine("  sagaproject evaluation-evidence-gate --project <path> --evidence-root <Build/Evaluation> --out <evaluation_evidence_gate_report.json>");
        Console.Error.WriteLine("  sagaproject evaluation-asset-import-cook-prerequisite --project <path> --evidence-root <Build/SourceTruth> --out <asset_import_cook_prerequisite_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-asset-consumption-prerequisite --project <path> --source-truth-root <Build/SourceTruth> --evaluation-root <Build/Evaluation> --out <runtime_asset_consumption_prerequisite_report.json>");
        Console.Error.WriteLine("  sagaproject runtime-projection-freshness-gate --project <path> --source-truth-root <Build/SourceTruth> --out <runtime_projection_freshness_gate_report.json>");
        Console.Error.WriteLine("  sagaproject client-evaluation-regression-fixture-plan --project <path> --evaluation-root <Build/Evaluation> --out <client_evaluation_regression_fixture_plan_report.json>");
        Console.Error.WriteLine("  sagaproject evaluation-focused-test-health-gate --project <path> --evaluation-root <Build/Evaluation> --out <evaluation_focused_test_health_gate_report.json>");
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
        public string ClientEvaluationPrerequisitePath { get; set; } = "";
        public string EvidenceRootPath { get; set; } = "";
        public string SourceTruthRootPath { get; set; } = "";
        public string EvaluationRootPath { get; set; } = "";
        public string DestinationPath { get; set; } = "";
        public string DisplayName { get; set; } = "";
        public string TemplatePath { get; set; } = "";
        public string PlanPath { get; set; } = "";
        public string SagaPackPath { get; set; } = "";
        public string EngineRoot { get; set; } = "";
        public string RuntimeBridgePath { get; set; } = "";
        public string OutputPath { get; set; } = "";

        public static CommandOptions Fail(string error)
        {
            return new CommandOptions { Ok = false, Error = error };
        }
    }
}
