// Program.cs
// Command-line entrypoint for the standalone SagaScript toolchain.

using System.Diagnostics;

namespace SagaScript;

internal static class Program
{
    private static int Main(string[] args)
    {
        try
        {
            return Run(args);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"sagascript: {ex.Message}");
            return 2;
        }
    }

    private static int Run(string[] args)
    {
        if (args.Length == 0 || args[0] is "--help" or "-h" or "help")
        {
            PrintHelp();
            return 0;
        }

        if (args[0] is "--version" or "version")
        {
            Console.WriteLine($"{ToolInfo.Name} {ToolInfo.Version}");
            return 0;
        }

        var command = args[0];
        var options = CliOptions.Parse(args.Skip(1).ToArray());
        if (command == "compile" && !DotNetSdkProbe.HasRequiredSdk())
        {
            return ReportMissingDotNetSdk(options);
        }

        if (RequiresSource(command) && options.Sources.Count == 0)
        {
            throw new InvalidOperationException("At least one --source path is required.");
        }

        var sourceFiles = options.Sources.Count == 0
            ? Array.Empty<string>()
            : SourceDiscovery.Discover(options.Sources);
        if (command is "analyze" or "emit-bindings" or "source-map" or "project-blocks" or "patch-preview" or "patch-apply" or "patch-diff" or "patch-review" or "patch-rollback" or "extract-nodes" or "compatibility-profile" or "validate-artifacts")
        {
            return RunSagaWeaverCommand(command, sourceFiles, options);
        }

        var analyzer = new SagaScriptAnalyzer();
        var requireManifestReady = command is "emit-manifests" or "compile";
        var result = analyzer.Analyze(sourceFiles, requireManifestReady);

        return command switch
        {
            "validate" => RunValidate(result, options),
            "inspect-bindings" => RunInspectBindings(result, options),
            "emit-manifests" => RunEmitManifests(result, options),
            "compile" => RunCompile(result, sourceFiles, options),
            _ => throw new InvalidOperationException($"Unknown command '{command}'.")
        };
    }

    private static bool RequiresSource(string command)
    {
        return command is not ("patch-review" or "patch-rollback" or "validate-artifacts");
    }

    private static int RunSagaWeaverCommand(
        string command,
        IReadOnlyList<string> sourceFiles,
        CliOptions options)
    {
        var outputDirectory = string.IsNullOrWhiteSpace(options.OutputDirectory)
            ? Path.Combine(Directory.GetCurrentDirectory(), "Build", "SagaScript")
            : options.OutputDirectory;

        if (command is "patch-preview" or "patch-apply" or "patch-diff")
        {
            if (string.IsNullOrWhiteSpace(options.SourceMapPath))
            {
                throw new InvalidOperationException($"{command} requires --source-map <file>.");
            }
            if (string.IsNullOrWhiteSpace(options.PatchRequestPath))
            {
                throw new InvalidOperationException($"{command} requires --request <file>.");
            }
            if (string.IsNullOrWhiteSpace(options.OutputPath))
            {
                throw new InvalidOperationException($"{command} requires --out <file>.");
            }

            if (command == "patch-apply")
            {
                var apply = SagaWeaverArtifacts.BuildPatchApply(
                    sourceFiles,
                    options.Sources,
                    options.SourceMapPath,
                    options.PatchRequestPath);
                ManifestWriter.WriteJson(options.OutputPath, apply);
                if (options.Json)
                {
                    ManifestWriter.PrintJson(apply);
                }
                return apply["status"]?.GetValue<string>() == "Passed" ? 0 : 1;
            }

            if (command == "patch-diff")
            {
                var diff = SagaWeaverArtifacts.BuildPatchDiff(
                    sourceFiles,
                    options.Sources,
                    options.SourceMapPath,
                    options.PatchRequestPath);
                ManifestWriter.WriteJson(options.OutputPath, diff);
                if (options.Json)
                {
                    ManifestWriter.PrintJson(diff);
                }
                return diff["status"]?.GetValue<string>() == "Passed" ? 0 : 1;
            }

            var preview = SagaWeaverArtifacts.BuildPatchPreview(
                sourceFiles,
                options.SourceMapPath,
                options.PatchRequestPath);
            ManifestWriter.WriteJson(options.OutputPath, preview);
            if (options.Json)
            {
                ManifestWriter.PrintJson(preview);
            }
            return preview["status"]?.GetValue<string>() == "Passed" ? 0 : 1;
        }

        if (command == "patch-review")
        {
            if (string.IsNullOrWhiteSpace(options.DiffReportPath))
            {
                throw new InvalidOperationException("patch-review requires --diff <file>.");
            }
            if (string.IsNullOrWhiteSpace(options.Decision))
            {
                throw new InvalidOperationException("patch-review requires --decision <Approved|Rejected>.");
            }
            if (string.IsNullOrWhiteSpace(options.Reviewer))
            {
                throw new InvalidOperationException("patch-review requires --reviewer <id>.");
            }
            if (string.IsNullOrWhiteSpace(options.OutputPath))
            {
                throw new InvalidOperationException("patch-review requires --out <file>.");
            }

            var review = SagaWeaverArtifacts.BuildPatchReview(
                options.DiffReportPath,
                options.Decision,
                options.Reviewer);
            ManifestWriter.WriteJson(options.OutputPath, review);
            if (options.Json)
            {
                ManifestWriter.PrintJson(review);
            }
            return review["status"]?.GetValue<string>() == "Passed" ? 0 : 1;
        }

        if (command == "patch-rollback")
        {
            if (string.IsNullOrWhiteSpace(options.ApplyReportPath))
            {
                throw new InvalidOperationException("patch-rollback requires --apply-report <file>.");
            }
            if (string.IsNullOrWhiteSpace(options.OutputPath))
            {
                throw new InvalidOperationException("patch-rollback requires --out <file>.");
            }

            var rollback = SagaWeaverArtifacts.BuildPatchRollback(options.ApplyReportPath);
            ManifestWriter.WriteJson(options.OutputPath, rollback);
            if (options.Json)
            {
                ManifestWriter.PrintJson(rollback);
            }
            return rollback["status"]?.GetValue<string>() == "Passed" ? 0 : 1;
        }

        if (command == "validate-artifacts")
        {
            if (string.IsNullOrWhiteSpace(options.ArtifactRoot))
            {
                throw new InvalidOperationException("validate-artifacts requires --artifact-root <dir>.");
            }
            if (string.IsNullOrWhiteSpace(options.OutputPath))
            {
                throw new InvalidOperationException("validate-artifacts requires --out <file>.");
            }

            var validation = SagaWeaverArtifacts.ValidateArtifacts(options.ArtifactRoot);
            ManifestWriter.WriteJson(options.OutputPath, validation);
            if (options.Json)
            {
                ManifestWriter.PrintJson(validation);
            }
            return validation.Status == "Passed" ? 0 : 1;
        }

        Directory.CreateDirectory(outputDirectory);
        if (command == "compatibility-profile")
        {
            var profile = SagaWeaverArtifacts.BuildCompatibilityProfile(sourceFiles);
            ManifestWriter.WriteJson(Path.Combine(outputDirectory, "csharp_compatibility_profile_v2.json"), profile);
            if (options.Json)
            {
                ManifestWriter.PrintJson(new
                {
                    schemaVersion = 2,
                    command,
                    outputDirectory = Path.GetFullPath(outputDirectory),
                    status = profile.Status,
                    summary = profile.Summary
                });
            }
            else
            {
                Console.WriteLine($"Wrote SagaScript compatibility profile to {Path.GetFullPath(outputDirectory)}");
            }
            return profile.Status == "Passed" ? 0 : 1;
        }

        if (command == "extract-nodes")
        {
            var report = SagaWeaverArtifacts.ExtractNodeLibrary(sourceFiles);
            ManifestWriter.WriteJson(Path.Combine(outputDirectory, "node_library_report.json"), report);
            if (options.Json)
            {
                ManifestWriter.PrintJson(new
                {
                    schemaVersion = 1,
                    command,
                    outputDirectory = Path.GetFullPath(outputDirectory),
                    diagnostics = report.Summary
                });
            }
            else
            {
                Console.WriteLine($"Wrote SagaScript node library artifacts to {Path.GetFullPath(outputDirectory)}");
                PrintDiagnostics(new DiagnosticReport
                {
                    Inputs = sourceFiles,
                    Summary = report.Summary,
                    Diagnostics = report.Diagnostics
                });
            }
            return report.Status == "Passed" ? 0 : 1;
        }

        var artifacts = SagaWeaverArtifacts.Analyze(sourceFiles, options.Strict);
        var diagnostics = SagaWeaverArtifacts.BuildDiagnosticReport(artifacts);
        var exitCode = artifacts.Summary.HasBlockingDiagnostics ? 1 : 0;

        switch (command)
        {
            case "analyze":
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "analysis_report.json"),
                    SagaWeaverArtifacts.BuildAnalysisReport(artifacts));
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "sagascript_diagnostics.json"),
                    diagnostics);
                break;
            case "emit-bindings":
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "runtime_bindings.json"),
                    SagaWeaverArtifacts.BuildRuntimeBindings(artifacts));
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "sagascript_diagnostics.json"),
                    diagnostics);
                break;
            case "source-map":
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "source_map.json"),
                    SagaWeaverArtifacts.BuildSourceMap(artifacts));
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "sagascript_diagnostics.json"),
                    diagnostics);
                break;
            case "project-blocks":
                var compatibilityProfile = SagaWeaverArtifacts.BuildCompatibilityProfile(sourceFiles);
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "projection_report.json"),
                    SagaWeaverArtifacts.BuildProjection(artifacts));
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "source_map.json"),
                    SagaWeaverArtifacts.BuildSourceMap(artifacts, "project-blocks"));
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "node_metadata.json"),
                    SagaWeaverArtifacts.BuildNodeMetadata(artifacts));
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "visual_blocks_projection_v1.json"),
                    SagaWeaverArtifacts.BuildVisualBlocksProjection(compatibilityProfile));
                ManifestWriter.WriteJson(
                    Path.Combine(outputDirectory, "sagascript_diagnostics.json"),
                    diagnostics);
                if (compatibilityProfile.Status != "Passed")
                {
                    exitCode = 1;
                }
                break;
            default:
                throw new InvalidOperationException($"Unknown command '{command}'.");
        }

        if (options.Json)
        {
            ManifestWriter.PrintJson(new
            {
                schemaVersion = 1,
                command,
                outputDirectory = Path.GetFullPath(outputDirectory),
                diagnostics = artifacts.Summary
            });
        }
        else
        {
            Console.WriteLine($"Wrote SagaScript {command} artifacts to {Path.GetFullPath(outputDirectory)}");
            PrintDiagnostics(diagnostics);
        }

        return exitCode;
    }

    private static int RunValidate(AnalysisResult result, CliOptions options)
    {
        var report = ManifestWriter.BuildDiagnosticReport(result);
        WriteDiagnosticsIfRequested(report, options);
        if (options.Json)
        {
            ManifestWriter.PrintJson(report);
        }
        else
        {
            PrintDiagnostics(report);
        }

        return result.Summary.HasBlockingDiagnostics ? 1 : 0;
    }

    private static int RunCompile(
        AnalysisResult result,
        IReadOnlyList<string> sourceFiles,
        CliOptions options)
    {
        var outputDirectory = string.IsNullOrWhiteSpace(options.OutputDirectory)
            ? Path.Combine(Directory.GetCurrentDirectory(), "Build", "Manifests")
            : options.OutputDirectory;
        var artifactDirectory = string.IsNullOrWhiteSpace(options.ArtifactOutputDirectory)
            ? Path.Combine(Directory.GetCurrentDirectory(), "Build", "Artifacts", "Scripts")
            : options.ArtifactOutputDirectory;
        var projectRoot = string.IsNullOrWhiteSpace(options.ProjectRoot)
            ? Directory.GetCurrentDirectory()
            : options.ProjectRoot;

        var compileResult = SagaScriptCompiler.Compile(new SagaScriptCompileRequest
        {
            Analysis = result,
            SourceFiles = sourceFiles,
            ManifestOutputDirectory = outputDirectory,
            ArtifactOutputDirectory = artifactDirectory,
            ProjectRoot = projectRoot,
            AssemblyName = options.AssemblyName
        });

        WriteDiagnosticsIfRequested(compileResult.DiagnosticReport, options);
        if (options.Json)
        {
            ManifestWriter.PrintJson(new
            {
                schemaVersion = 1,
                manifestOutputDirectory = Path.GetFullPath(outputDirectory),
                artifactOutputDirectory = Path.GetFullPath(artifactDirectory),
                targetFramework = ToolInfo.TargetFramework,
                artifacts = compileResult.ScriptArtifactManifest.Artifacts.Count,
                diagnostics = compileResult.DiagnosticReport.Summary
            });
        }
        else
        {
            Console.WriteLine($"Wrote SagaScript compile outputs to {Path.GetFullPath(artifactDirectory)}");
            Console.WriteLine($"Wrote SagaScript manifests to {Path.GetFullPath(outputDirectory)}");
            PrintDiagnostics(compileResult.DiagnosticReport);
        }

        return compileResult.DiagnosticReport.Summary.HasBlockingDiagnostics ? 1 : 0;
    }

    private static int ReportMissingDotNetSdk(CliOptions options)
    {
        var report = ManifestWriter.BuildMissingDotNetSdkReport();
        WriteDiagnosticsIfRequested(report, options);
        if (options.Json)
        {
            ManifestWriter.PrintJson(report);
        }
        else
        {
            PrintDiagnostics(report);
        }

        return 1;
    }

    private static int RunInspectBindings(AnalysisResult result, CliOptions options)
    {
        var report = new InspectionReport
        {
            Bindings = result.Bindings,
            DiagnosticSummary = result.Summary
        };

        if (!string.IsNullOrWhiteSpace(options.OutputPath))
        {
            ManifestWriter.WriteJson(options.OutputPath, report);
        }
        else
        {
            ManifestWriter.PrintJson(report);
        }

        WriteDiagnosticsIfRequested(ManifestWriter.BuildDiagnosticReport(result), options);
        return result.Summary.HasBlockingDiagnostics ? 1 : 0;
    }

    private static int RunEmitManifests(AnalysisResult result, CliOptions options)
    {
        var outputDirectory = string.IsNullOrWhiteSpace(options.OutputDirectory)
            ? Path.Combine(Directory.GetCurrentDirectory(), "Build", "Manifests")
            : options.OutputDirectory;
        Directory.CreateDirectory(outputDirectory);

        ManifestWriter.WriteJson(
            Path.Combine(outputDirectory, "script_bindings.json"),
            ManifestWriter.BuildBindingManifest(result));
        ManifestWriter.WriteJson(
            Path.Combine(outputDirectory, "script_capabilities.json"),
            ManifestWriter.BuildCapabilityManifest(result));
        ManifestWriter.WriteJson(
            Path.Combine(outputDirectory, "sagascript_diagnostics.json"),
            ManifestWriter.BuildDiagnosticReport(result));

        WriteDiagnosticsIfRequested(ManifestWriter.BuildDiagnosticReport(result), options);
        if (options.Json)
        {
            ManifestWriter.PrintJson(new
            {
                schemaVersion = 1,
                outputDirectory = Path.GetFullPath(outputDirectory),
                diagnostics = result.Summary
            });
        }
        else
        {
            Console.WriteLine($"Wrote SagaScript manifests to {Path.GetFullPath(outputDirectory)}");
            PrintDiagnostics(ManifestWriter.BuildDiagnosticReport(result));
        }

        return result.Summary.HasBlockingDiagnostics ? 1 : 0;
    }

    private static void WriteDiagnosticsIfRequested(DiagnosticReport report, CliOptions options)
    {
        if (!string.IsNullOrWhiteSpace(options.DiagnosticsPath))
        {
            ManifestWriter.WriteJson(options.DiagnosticsPath, report);
        }
    }

    private static void PrintDiagnostics(DiagnosticReport report)
    {
        foreach (var diagnostic in report.Diagnostics)
        {
            var location = diagnostic.SourceFile is null ? "" : $" {diagnostic.SourceFile}";
            Console.WriteLine($"{diagnostic.Severity} {diagnostic.Code}:{location} {diagnostic.Message}");
        }

        Console.WriteLine(
            $"Diagnostics: errors={report.Summary.ErrorCount} security={report.Summary.SecurityCount} warnings={report.Summary.WarningCount}");
    }

    private static void PrintHelp()
    {
        Console.WriteLine("SagaScript CLI");
        Console.WriteLine();
        Console.WriteLine("Commands:");
        Console.WriteLine("  validate --source <file-or-dir> [--diagnostics <file>] [--json]");
        Console.WriteLine("  inspect-bindings --source <file-or-dir> [--out <file>]");
        Console.WriteLine("  emit-manifests --source <file-or-dir> --out <dir> [--json]");
        Console.WriteLine("  compile --source <file-or-dir> --out <manifest-dir> [--artifacts-out <dir>] [--json]");
        Console.WriteLine("  analyze --source <file-or-dir> --out <dir> [--json]");
        Console.WriteLine("  emit-bindings --source <file-or-dir> --out <dir> [--json]");
        Console.WriteLine("  source-map --source <file-or-dir> --out <dir> [--json]");
        Console.WriteLine("  project-blocks --source <file-or-dir> --out <dir> [--json]");
        Console.WriteLine("  extract-nodes --source <file-or-dir> --out <dir> [--json]");
        Console.WriteLine("  compatibility-profile --source <file-or-dir> --out <dir> [--json]");
        Console.WriteLine("  validate-artifacts --artifact-root <dir> --out <file> [--json]");
        Console.WriteLine("  patch-preview --source <file-or-dir> --source-map <file> --request <file> --out <file> [--json]");
        Console.WriteLine("  patch-apply --source <file-or-dir> --source-map <file> --request <file> --out <file> [--json]");
        Console.WriteLine("  patch-diff --source <file-or-dir> --source-map <file> --request <file> --out <file> [--json]");
        Console.WriteLine("  patch-review --diff <file> --decision <Approved|Rejected> --reviewer <id> --out <file> [--json]");
        Console.WriteLine("  patch-rollback --apply-report <file> --out <file> [--json]");
    }
}

internal static class DotNetSdkProbe
{
    public static bool HasRequiredSdk()
    {
        try
        {
            var startInfo = new ProcessStartInfo
            {
                FileName = "dotnet",
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false
            };
            startInfo.ArgumentList.Add("--list-sdks");

            using var process = Process.Start(startInfo);
            if (process is null)
            {
                return false;
            }

            var output = process.StandardOutput.ReadToEnd();
            process.WaitForExit();
            return process.ExitCode == 0 &&
                output.Split('\n').Any(line => line.StartsWith("10.", StringComparison.Ordinal));
        }
        catch (Exception)
        {
            return false;
        }
    }
}

internal sealed class CliOptions
{
    public List<string> Sources { get; } = new();
    public string OutputDirectory { get; private set; } = "";
    public string OutputPath { get; private set; } = "";
    public string DiagnosticsPath { get; private set; } = "";
    public string ProjectRoot { get; private set; } = "";
    public string ArtifactOutputDirectory { get; private set; } = "";
    public string AssemblyName { get; private set; } = "SagaProjectScripts";
    public string SourceMapPath { get; private set; } = "";
    public string PatchRequestPath { get; private set; } = "";
    public string DiffReportPath { get; private set; } = "";
    public string ApplyReportPath { get; private set; } = "";
    public string ArtifactRoot { get; private set; } = "";
    public string Decision { get; private set; } = "";
    public string Reviewer { get; private set; } = "";
    public bool Strict { get; private set; }
    public bool Json { get; private set; }

    public static CliOptions Parse(IReadOnlyList<string> args)
    {
        var options = new CliOptions();
        for (var i = 0; i < args.Count; i++)
        {
            switch (args[i])
            {
                case "--source":
                    options.Sources.Add(RequireValue(args, ref i, "--source"));
                    break;
                case "--out":
                    var output = RequireValue(args, ref i, "--out");
                    if (Path.HasExtension(output))
                    {
                        options.OutputPath = output;
                    }
                    else
                    {
                        options.OutputDirectory = output;
                    }
                    break;
                case "--diagnostics":
                    options.DiagnosticsPath = RequireValue(args, ref i, "--diagnostics");
                    break;
                case "--artifacts-out":
                    options.ArtifactOutputDirectory = RequireValue(args, ref i, "--artifacts-out");
                    break;
                case "--project-root":
                    options.ProjectRoot = RequireValue(args, ref i, "--project-root");
                    break;
                case "--assembly-name":
                    options.AssemblyName = RequireValue(args, ref i, "--assembly-name");
                    break;
                case "--source-map":
                    options.SourceMapPath = RequireValue(args, ref i, "--source-map");
                    break;
                case "--request":
                    options.PatchRequestPath = RequireValue(args, ref i, "--request");
                    break;
                case "--diff":
                    options.DiffReportPath = RequireValue(args, ref i, "--diff");
                    break;
                case "--apply-report":
                    options.ApplyReportPath = RequireValue(args, ref i, "--apply-report");
                    break;
                case "--artifact-root":
                    options.ArtifactRoot = RequireValue(args, ref i, "--artifact-root");
                    break;
                case "--decision":
                    options.Decision = RequireValue(args, ref i, "--decision");
                    break;
                case "--reviewer":
                    options.Reviewer = RequireValue(args, ref i, "--reviewer");
                    break;
                case "--strict":
                    options.Strict = true;
                    break;
                case "--json":
                    options.Json = true;
                    break;
                default:
                    throw new InvalidOperationException($"Unknown option '{args[i]}'.");
            }
        }

        return options;
    }

    private static string RequireValue(IReadOnlyList<string> args, ref int index, string option)
    {
        if (index + 1 >= args.Count)
        {
            throw new InvalidOperationException($"{option} requires a value.");
        }

        index++;
        return args[index];
    }
}
