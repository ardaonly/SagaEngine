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

        if (options.Sources.Count == 0)
        {
            throw new InvalidOperationException("At least one --source path is required.");
        }

        var sourceFiles = SourceDiscovery.Discover(options.Sources);
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
