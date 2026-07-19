// SPDX-License-Identifier: MPL-2.0
namespace SagaPackager;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
    private const int MissingInput = 3;
    private const int InternalError = 4;

    public static async Task<int> Main(string[] args)
    {
        try
        {
            if (args.Length == 0 || args[0] is "--help" or "-h" or "help")
            {
                PrintUsage();
                return args.Length == 0 ? InvalidUsage : Passed;
            }

            var command = args[0];
            var options = CommandOptions.Parse(args.Skip(1).ToArray());
            if (!options.Ok)
            {
                Console.Error.WriteLine(options.Error);
                PrintUsage();
                return InvalidUsage;
            }

            return command switch
            {
                "validate" => RunValidate(options),
                "asset-validate" => RunAssetValidate(options),
                "profile-matrix" => RunProfileMatrix(options),
                "source-truth-alignment" => RunSourceTruthAlignment(options),
                "stage" => RunStage(options),
                "publish-check" => RunPublishCheck(options),
                "smoke" => await RunSmoke(options),
                "compile-assets" => RunContentCompile(options, "assets"),
                "compile-scenes" => RunContentCompile(options, "scenes"),
                "compile-inputs" => RunContentCompile(options, "inputs"),
                "compile-content" => RunContentCompile(options, "all"),
                "resolve" => RunResolvePackages(options),
                "lock-verify" => RunVerifyLock(options),
                _ => InvalidUsage,
            };
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagapack error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunValidate(CommandOptions options)
    {
        if (!RequireProjectProfileOut(options))
        {
            return InvalidUsage;
        }
        if (!ProjectExists(options.ProjectPath))
        {
            return MissingInput;
        }
        var report = PackageValidator.Validate(options.ProjectPath, options.Profile);
        ReportWriter.Write(options.OutPath, report);
        PrintReport(report);
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunContentCompile(CommandOptions options, string domain)
    {
        if (!RequireProjectOut(options))
        {
            return InvalidUsage;
        }
        var result = ContentCompiler.Compile(options.ProjectPath, options.OutPath, domain,
            options.ScriptManifestPath);
        SagaProjectKit.Core.CanonicalJson.Write(Path.Combine(options.OutPath, "compile-result.json"), result);
        Console.WriteLine($"sagapack compile-{domain}: {result.Status}");
        return result.Status == "Passed" ? Passed : Failed;
    }

    private static int RunResolvePackages(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ProjectPath) ||
            string.IsNullOrWhiteSpace(options.LockPath) ||
            string.IsNullOrWhiteSpace(options.GeneratedOut) ||
            string.IsNullOrWhiteSpace(options.RuntimeBridgePath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("resolve requires --project, --lock, --generated-out, --runtime-bridge, and --out.");
            return InvalidUsage;
        }
        var report = PackageResolutionService.Resolve(
            options.ProjectPath, options.EngineRoot, options.LockPath, options.GeneratedOut,
            options.RuntimeBridgePath);
        SagaProjectKit.Core.CanonicalJson.Write(options.OutPath, report);
        Console.WriteLine($"sagapack resolve: {report.Status}");
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunVerifyLock(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ProjectPath) ||
            string.IsNullOrWhiteSpace(options.LockPath) ||
            string.IsNullOrWhiteSpace(options.RuntimeBridgePath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("lock-verify requires --project, --lock, --runtime-bridge, and --out.");
            return InvalidUsage;
        }
        var report = PackageResolutionService.Verify(options.ProjectPath, options.EngineRoot, options.LockPath,
            options.RuntimeBridgePath, options.GeneratedOut);
        SagaProjectKit.Core.CanonicalJson.Write(options.OutPath, report);
        Console.WriteLine($"sagapack lock-verify: {report.Status}");
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunAssetValidate(CommandOptions options)
    {
        if (!RequireProjectOut(options))
        {
            return InvalidUsage;
        }
        if (!ProjectExists(options.ProjectPath))
        {
            return MissingInput;
        }
        var report = AssetWorkflowValidator.Validate(options.ProjectPath, options.PackageReportPath);
        ReportWriter.Write(options.OutPath, report);
        Console.WriteLine($"sagapack asset-validate: {report.Status}");
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunProfileMatrix(CommandOptions options)
    {
        if (!RequireProjectOut(options))
        {
            return InvalidUsage;
        }
        if (!ProjectExists(options.ProjectPath))
        {
            return MissingInput;
        }
        var report = PackageProfileMatrixReporter.Build(
            options.ProjectPath,
            options.PackageReportPath,
            options.DiagnosticsSummaryPath);
        ReportWriter.Write(options.OutPath, report);
        Console.WriteLine($"sagapack profile-matrix: {report.Status}");
        return report.Status == "Failed" ? Failed : Passed;
    }

    private static int RunSourceTruthAlignment(CommandOptions options)
    {
        if (!RequireProjectOut(options) ||
            string.IsNullOrWhiteSpace(options.SourceTruthGatePath) ||
            string.IsNullOrWhiteSpace(options.AssetReferenceGatePath))
        {
            Console.Error.WriteLine("source-truth-alignment requires --project, --source-truth-gate, --asset-reference-gate, and --out.");
            return InvalidUsage;
        }
        if (!ProjectExists(options.ProjectPath))
        {
            return MissingInput;
        }
        var report = SourceTruthAlignmentReporter.Build(
            options.ProjectPath,
            options.SourceTruthGatePath,
            options.AssetReferenceGatePath);
        ReportWriter.Write(options.OutPath, report);
        Console.WriteLine($"sagapack source-truth-alignment: {report.Status}");
        return report.Status == "Failed" ? Failed : Passed;
    }

    private static int RunStage(CommandOptions options)
    {
        if (!RequireProjectProfileOut(options))
        {
            return InvalidUsage;
        }
        if (!ProjectExists(options.ProjectPath))
        {
            return MissingInput;
        }
        var report = PackageStager.Stage(options.ProjectPath, options.Profile);
        ReportWriter.Write(options.OutPath, report);
        PrintReport(report);
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunPublishCheck(CommandOptions options)
    {
        if (!RequireProjectProfileOut(options) ||
            string.IsNullOrWhiteSpace(options.PackageReportPath) ||
            string.IsNullOrWhiteSpace(options.DiagnosticsSummaryPath))
        {
            Console.Error.WriteLine("publish-check requires --project, --profile, --package-report, --diagnostics-summary, and --out.");
            return InvalidUsage;
        }
        if (!ProjectExists(options.ProjectPath))
        {
            return MissingInput;
        }
        var report = PublishGate.Check(
            options.ProjectPath,
            options.Profile,
            options.PackageReportPath,
            options.DiagnosticsSummaryPath,
            options.ScriptValidationPath,
            options.PolicyReportPath);
        ReportWriter.Write(options.OutPath, report);
        Console.WriteLine($"sagapack publish-check: {report.Status}");
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static async Task<int> RunSmoke(CommandOptions options)
    {
        if (!RequireProjectProfileOut(options) ||
            string.IsNullOrWhiteSpace(options.PackageReportPath) ||
            string.IsNullOrWhiteSpace(options.BinDir))
        {
            Console.Error.WriteLine("smoke requires --project, --profile, --package-report, --out, --timeout-sec, and --bin-dir.");
            return InvalidUsage;
        }
        if (!ProjectExists(options.ProjectPath))
        {
            return MissingInput;
        }
        var report = await PackagedSmokeRunner.RunAsync(
            options.ProjectPath,
            options.Profile,
            options.PackageReportPath,
            options.OutPath,
            options.BinDir,
            options.TimeoutSeconds);
        ReportWriter.Write(options.OutPath, report);
        Console.WriteLine($"sagapack smoke: {report.Status}");
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static bool RequireProjectProfileOut(CommandOptions options)
    {
        if (!string.IsNullOrWhiteSpace(options.ProjectPath) &&
            !string.IsNullOrWhiteSpace(options.Profile) &&
            !string.IsNullOrWhiteSpace(options.OutPath))
        {
            return true;
        }
        Console.Error.WriteLine("--project, --profile, and --out are required.");
        return false;
    }

    private static bool RequireProjectOut(CommandOptions options)
    {
        if (!string.IsNullOrWhiteSpace(options.ProjectPath) &&
            !string.IsNullOrWhiteSpace(options.OutPath))
        {
            return true;
        }
        Console.Error.WriteLine("--project and --out are required.");
        return false;
    }

    private static bool ProjectExists(string projectPath)
    {
        return File.Exists(projectPath) || Directory.Exists(projectPath);
    }

    private static void PrintReport(PackageReport report)
    {
        Console.WriteLine($"sagapack {report.Command}: {report.Status}");
        Console.WriteLine($"project: {report.Project.ProjectId}");
        Console.WriteLine($"profile: {report.PackageProfile.Id}");
        Console.WriteLine($"diagnostics: {report.Diagnostics.Count}");
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaPackager CLI");
        Console.Error.WriteLine("  sagapack validate --project <path> --profile <id> --out <report>");
        Console.Error.WriteLine("  sagapack asset-validate --project <path> [--package-report <stage-report>] --out <report>");
        Console.Error.WriteLine("  sagapack profile-matrix --project <path> [--package-report <stage-report>] [--diagnostics-summary <summary>] --out <report>");
        Console.Error.WriteLine("  sagapack source-truth-alignment --project <path> --source-truth-gate <report> --asset-reference-gate <report> --out <report>");
        Console.Error.WriteLine("  sagapack stage --project <path> --profile <id> --out <report>");
        Console.Error.WriteLine("  sagapack publish-check --project <path> --profile <id> --package-report <report> --diagnostics-summary <summary> [--script-validation <report>] [--policy-report <report>] --out <report>");
        Console.Error.WriteLine("  sagapack smoke --project <path> --profile <id> --package-report <report> --out <report> --timeout-sec <n> --bin-dir <dir>");
        Console.Error.WriteLine("  sagapack compile-assets --project <path> --out <artifact-root>");
        Console.Error.WriteLine("  sagapack compile-scenes --project <path> --script-manifest <SagaScript current.json> --out <artifact-root>");
        Console.Error.WriteLine("  sagapack compile-inputs --project <path> --out <artifact-root>");
        Console.Error.WriteLine("  sagapack compile-content --project <path> --script-manifest <SagaScript current.json> --out <artifact-root>");
        Console.Error.WriteLine("  sagapack resolve --project <path> [--engine-root <root>] --lock <SagaPackage.lock> --generated-out <dir> --runtime-bridge <dll> --out <report>");
        Console.Error.WriteLine("  sagapack lock-verify --project <path> [--engine-root <root>] --lock <SagaPackage.lock> [--generated-out <dir>] --runtime-bridge <dll> --out <report>");
    }

    private sealed record CommandOptions(
        bool Ok,
        string Error,
        string ProjectPath,
        string Profile,
        string OutPath,
        string PackageReportPath,
        string DiagnosticsSummaryPath,
        string ScriptValidationPath,
        string PolicyReportPath,
        string SourceTruthGatePath,
        string AssetReferenceGatePath,
        string BinDir,
        int TimeoutSeconds,
        string EngineRoot,
        string LockPath,
        string GeneratedOut,
        string RuntimeBridgePath,
        string ScriptManifestPath)
    {
        public static CommandOptions Parse(string[] args)
        {
            string project = "";
            string profile = "";
            string output = "";
            string packageReport = "";
            string diagnosticsSummary = "";
            string scriptValidation = "";
            string policyReport = "";
            string sourceTruthGate = "";
            string assetReferenceGate = "";
            string binDir = "";
            string engineRoot = "";
            string lockPath = "";
            string generatedOut = "";
            string runtimeBridge = "";
            string scriptManifest = "";
            var timeout = 5;

            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--project" when i + 1 < args.Length:
                        project = args[++i];
                        break;
                    case "--profile" when i + 1 < args.Length:
                        profile = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    case "--package-report" when i + 1 < args.Length:
                        packageReport = args[++i];
                        break;
                    case "--diagnostics-summary" when i + 1 < args.Length:
                        diagnosticsSummary = args[++i];
                        break;
                    case "--script-validation" when i + 1 < args.Length:
                        scriptValidation = args[++i];
                        break;
                    case "--policy-report" when i + 1 < args.Length:
                        policyReport = args[++i];
                        break;
                    case "--source-truth-gate" when i + 1 < args.Length:
                        sourceTruthGate = args[++i];
                        break;
                    case "--asset-reference-gate" when i + 1 < args.Length:
                        assetReferenceGate = args[++i];
                        break;
                    case "--bin-dir" when i + 1 < args.Length:
                        binDir = args[++i];
                        break;
                    case "--timeout-sec" when i + 1 < args.Length:
                        if (!int.TryParse(args[++i], out timeout) || timeout <= 0)
                        {
                            return Fail("--timeout-sec must be a positive integer.");
                        }
                        break;
                    case "--engine-root" when i + 1 < args.Length:
                        engineRoot = args[++i];
                        break;
                    case "--lock" when i + 1 < args.Length:
                        lockPath = args[++i];
                        break;
                    case "--generated-out" when i + 1 < args.Length:
                        generatedOut = args[++i];
                        break;
                    case "--runtime-bridge" when i + 1 < args.Length:
                        runtimeBridge = args[++i];
                        break;
                    case "--script-manifest" when i + 1 < args.Length:
                        scriptManifest = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", project, profile, output, packageReport, diagnosticsSummary, scriptValidation, policyReport, sourceTruthGate, assetReferenceGate, binDir, timeout, engineRoot, lockPath, generatedOut, runtimeBridge, scriptManifest);
        }

        private static CommandOptions Fail(string error)
        {
            return new CommandOptions(false, error, "", "", "", "", "", "", "", "", "", "", 5, "", "", "", "", "");
        }
    }
}
