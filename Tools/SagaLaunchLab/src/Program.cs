namespace SagaLaunchLab;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
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
                "server" => await RunServer(options, "server"),
                "accept" => await RunAccept(options),
                "profile-matrix" => RunProfileMatrix(options),
                "source-truth-alignment" => RunSourceTruthAlignment(options),
                _ => InvalidUsage,
            };
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagalaunch error: {e.Message}");
            return InternalError;
        }
    }

    private static async Task<int> RunServer(CommandOptions options, string command)
    {
        if (!RequireProjectProfileOut(options))
        {
            return InvalidUsage;
        }
        var report = await BuildServerReport(options, command);
        ReportWriter.Write(options.OutPath, report);
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static async Task<int> RunAccept(CommandOptions options)
    {
        if (!RequireProjectProfileOut(options))
        {
            return InvalidUsage;
        }
        var reportDir = Path.GetDirectoryName(Path.GetFullPath(options.OutPath)) ??
            Directory.GetCurrentDirectory();
        var validationPath = Path.Combine(reportDir, "project_validation_report.json");
        var resolutionPath = Path.Combine(reportDir, "project_resolution.json");
        var serverLaunchPath = Path.Combine(reportDir, "launch_preview_report.json");

        var resolver = new LaunchProfileResolver();
        var resolved = resolver.Resolve(options.ProjectPath, options.LaunchProfile);
        var project = BuildProjectSummary(resolved);
        var diagnostics = resolved.Diagnostics.ToList();

        var validation = await RunSagaProjectKit(
            options.SagaProjectPath,
            "validate",
            options.ProjectPath,
            validationPath,
            options.TimeoutSeconds);
        var resolution = await RunSagaProjectKit(
            options.SagaProjectPath,
            "resolve",
            options.ProjectPath,
            resolutionPath,
            options.TimeoutSeconds);

        var serverOptions = options with { OutPath = serverLaunchPath };
        var serverReport = await BuildServerReport(serverOptions, "server");

        diagnostics.AddRange(validation.Diagnostics);
        diagnostics.AddRange(resolution.Diagnostics);
        diagnostics.AddRange(serverReport.Diagnostics);

        var report = new AcceptanceReport
        {
            Status = validation.Passed && resolution.Passed && serverReport.Status == "Passed"
                ? "Passed"
                : "Failed",
            Project = project,
            ProjectValidation = new AcceptanceStage
            {
                Name = "SagaProjectKit validate",
                Status = validation.Passed ? "Passed" : "Failed",
                ReportPath = LaunchProfileResolver.Normalize(validationPath),
                Reason = validation.Reason,
            },
            ProjectResolution = new AcceptanceStage
            {
                Name = "SagaProjectKit resolve",
                Status = resolution.Passed ? "Passed" : "Failed",
                ReportPath = LaunchProfileResolver.Normalize(resolutionPath),
                Reason = resolution.Reason,
            },
            ServerPreview = new AcceptanceStage
            {
                Name = "SagaLaunchLab server",
                Status = serverReport.Status,
                ReportPath = LaunchProfileResolver.Normalize(serverLaunchPath),
                Reason = serverReport.Status == "Passed" ? "" : "Server preview did not pass.",
            },
            DeferredStages =
            [
                new AcceptanceStage
                {
                    Name = "Phase 23 server plus one runtime client",
                    Status = "Deferred",
                    Reason = "ClientHost is not yet exposed through a bounded runtime preview/report seam.",
                },
                new AcceptanceStage
                {
                    Name = "Phase 24 two-client local preview",
                    Status = "Deferred",
                    Reason = "Depends on accepted Phase 23 runtime client preview seam.",
                },
            ],
            Diagnostics = LaunchProfileResolver.Sort(diagnostics),
        };

        ReportWriter.Write(options.OutPath, report);
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunProfileMatrix(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ProjectPath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("profile-matrix requires --project and --out.");
            return InvalidUsage;
        }

        var report = LaunchProfileMatrixReporter.Build(options.ProjectPath, options.BinDir);
        ReportWriter.Write(options.OutPath, report);
        return report.Status == "Failed" ? Failed : Passed;
    }

    private static int RunSourceTruthAlignment(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ProjectPath) ||
            string.IsNullOrWhiteSpace(options.SourceTruthGatePath) ||
            string.IsNullOrWhiteSpace(options.RuntimeReadinessPath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("source-truth-alignment requires --project, --source-truth-gate, --runtime-readiness, and --out.");
            return InvalidUsage;
        }

        var report = SourceTruthAlignmentReporter.Build(
            options.ProjectPath,
            options.SourceTruthGatePath,
            options.RuntimeReadinessPath);
        ReportWriter.Write(options.OutPath, report);
        return report.Status == "Failed" ? Failed : Passed;
    }

    private static async Task<LaunchPreviewReport> BuildServerReport(
        CommandOptions options,
        string command)
    {
        var resolver = new LaunchProfileResolver();
        var resolved = resolver.Resolve(options.ProjectPath, options.LaunchProfile);
        var diagnostics = resolved.Diagnostics.ToList();
        var project = BuildProjectSummary(resolved);
        var profileSummary = new LaunchProfileSummary
        {
            Id = resolved.Profile?.Id ?? options.LaunchProfile,
            Role = resolved.Profile?.Role ?? "",
            Mode = resolved.Profile?.Mode ?? "",
        };

        var artifacts = BuildArtifacts(options, resolved);
        if (diagnostics.Any(d => d.Severity == "Error") || resolved.Profile is null)
        {
            return FailedReport(command, project, profileSummary, artifacts, diagnostics);
        }

        var executable = ResolveExecutable(options.BinDir, resolved.Profile.Executable);
        if (!File.Exists(executable))
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.Process.ExecutableMissing",
                "Process",
                executable,
                "Launch executable does not exist."));
            return FailedReport(command, project, profileSummary, artifacts, diagnostics);
        }

        var arguments = BuildArguments(resolved, artifacts);
        LaunchProcessInfo process;
        try
        {
            process = await new LaunchProcessRunner().RunAsync(
                executable,
                arguments,
                resolved.ProjectRoot,
                artifacts.StdoutPath,
                artifacts.StderrPath,
                options.TimeoutSeconds);
        }
        catch (Exception e)
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.Process.StartFailed",
                "Process",
                executable,
                e.Message));
            return FailedReport(command, project, profileSummary, artifacts, diagnostics);
        }

        if (process.TimedOut)
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.Process.Timeout",
                "Process",
                executable,
                "Launch process exceeded timeout and was stopped."));
        }
        else if (process.ExitCode != 0)
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.Process.ExitFailed",
                "Process",
                executable,
                $"Launch process exited with code {process.ExitCode}."));
        }
        if (!File.Exists(artifacts.HeadlessReportPath))
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.Artifact.ReportMissing",
                "Artifact",
                artifacts.HeadlessReportPath,
                "Expected headless server report was not produced."));
        }

        return new LaunchPreviewReport
        {
            Command = command,
            Status = diagnostics.Any(d => d.Severity == "Error") ? "Failed" : "Passed",
            Project = project,
            LaunchProfile = profileSummary,
            Process = process,
            Artifacts = artifacts,
            Diagnostics = LaunchProfileResolver.Sort(diagnostics),
        };
    }

    private static LaunchPreviewReport FailedReport(
        string command,
        ProjectReportSummary project,
        LaunchProfileSummary profile,
        LaunchArtifacts artifacts,
        List<LaunchDiagnostic> diagnostics)
    {
        return new LaunchPreviewReport
        {
            Command = command,
            Status = "Failed",
            Project = project,
            LaunchProfile = profile,
            Artifacts = artifacts,
            Diagnostics = LaunchProfileResolver.Sort(diagnostics),
        };
    }

    private static ProjectReportSummary BuildProjectSummary(LaunchResolution resolution)
    {
        return new ProjectReportSummary
        {
            ProjectId = resolution.Project?.ProjectId ?? "",
            DisplayName = resolution.Project?.DisplayName ?? "",
            ProjectRoot = resolution.ProjectRoot,
            ManifestPath = resolution.ManifestPath,
        };
    }

    private static LaunchArtifacts BuildArtifacts(
        CommandOptions options,
        LaunchResolution resolution)
    {
        var reportDir = Path.GetDirectoryName(Path.GetFullPath(options.OutPath)) ??
            Directory.GetCurrentDirectory();
        var headlessReport = Path.Combine(
            resolution.ProjectRoot.Length > 0 ? resolution.ProjectRoot : reportDir,
            resolution.Profile?.ReportPath ?? "Build/Reports/headless_server_report.json");
        var diagnosticsDir = Path.Combine(
            resolution.ProjectRoot.Length > 0 ? resolution.ProjectRoot : reportDir,
            resolution.Profile?.DiagnosticsPath ?? "Diagnostics");
        return new LaunchArtifacts
        {
            HeadlessReportPath = LaunchProfileResolver.Normalize(headlessReport),
            DiagnosticsDirectory = LaunchProfileResolver.Normalize(diagnosticsDir),
            StdoutPath = LaunchProfileResolver.Normalize(Path.Combine(reportDir, "sagalaunch_stdout.log")),
            StderrPath = LaunchProfileResolver.Normalize(Path.Combine(reportDir, "sagalaunch_stderr.log")),
        };
    }

    private static List<string> BuildArguments(
        LaunchResolution resolution,
        LaunchArtifacts artifacts)
    {
        var profile = resolution.Profile!;
        return profile.Arguments
            .Select(arg => arg
                .Replace("{project}", resolution.ManifestPath, StringComparison.Ordinal)
                .Replace("{projectRoot}", resolution.ProjectRoot, StringComparison.Ordinal)
                .Replace("{headlessReport}", artifacts.HeadlessReportPath, StringComparison.Ordinal)
                .Replace("{diagnosticsOut}", artifacts.DiagnosticsDirectory, StringComparison.Ordinal))
            .ToList();
    }

    private static string ResolveExecutable(string binDir, string executable)
    {
        if (Path.IsPathRooted(executable))
        {
            return Path.GetFullPath(executable);
        }
        if (!string.IsNullOrWhiteSpace(binDir))
        {
            return Path.GetFullPath(Path.Combine(binDir, executable));
        }
        return Path.GetFullPath(executable);
    }

    private static async Task<ProjectKitResult> RunSagaProjectKit(
        string sagaproject,
        string command,
        string project,
        string output,
        int timeoutSeconds)
    {
        var diagnostics = new List<LaunchDiagnostic>();
        var executable = string.IsNullOrWhiteSpace(sagaproject)
            ? Path.GetFullPath(Path.Combine("Tools", "SagaProjectKit", "sagaproject"))
            : Path.GetFullPath(sagaproject);
        if (!File.Exists(executable))
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.ProjectKit.Missing",
                "ProjectKit",
                executable,
                "SagaProjectKit executable was not found."));
            return new ProjectKitResult(false, "SagaProjectKit missing.", diagnostics);
        }

        var reportDir = Path.GetDirectoryName(Path.GetFullPath(output)) ??
            Directory.GetCurrentDirectory();
        var stdout = Path.Combine(reportDir, $"sagaproject_{command}_stdout.log");
        var stderr = Path.Combine(reportDir, $"sagaproject_{command}_stderr.log");
        var process = await new LaunchProcessRunner().RunAsync(
            executable,
            [command, "--project", project, "--out", output],
            Directory.GetCurrentDirectory(),
            stdout,
            stderr,
            timeoutSeconds);
        if (process.TimedOut)
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.ProjectKit.Timeout",
                "ProjectKit",
                executable,
                $"SagaProjectKit {command} timed out."));
        }
        else if (process.ExitCode != 0)
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.ProjectKit.Failed",
                "ProjectKit",
                executable,
                $"SagaProjectKit {command} exited with code {process.ExitCode}."));
        }
        return new ProjectKitResult(
            diagnostics.Count == 0,
            diagnostics.Count == 0 ? "" : $"SagaProjectKit {command} failed.",
            diagnostics);
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaLaunchLab CLI");
        Console.Error.WriteLine("  sagalaunch server --project <path> --launch-profile <id> --out <report> --timeout-sec <n> --bin-dir <dir>");
        Console.Error.WriteLine("  sagalaunch accept --project <path> --launch-profile <id> --out <report> --timeout-sec <n> --bin-dir <dir>");
        Console.Error.WriteLine("  sagalaunch profile-matrix --project <path> --out <report> [--bin-dir <dir>]");
        Console.Error.WriteLine("  sagalaunch source-truth-alignment --project <path> --source-truth-gate <report> --runtime-readiness <report> --out <report>");
    }

    private static bool RequireProjectProfileOut(CommandOptions options)
    {
        if (!string.IsNullOrWhiteSpace(options.ProjectPath) &&
            !string.IsNullOrWhiteSpace(options.LaunchProfile) &&
            !string.IsNullOrWhiteSpace(options.OutPath))
        {
            return true;
        }
        Console.Error.WriteLine("--project, --launch-profile, and --out are required.");
        return false;
    }

    private sealed record ProjectKitResult(
        bool Passed,
        string Reason,
        List<LaunchDiagnostic> Diagnostics);

    private sealed record CommandOptions(
        bool Ok,
        string Error,
        string ProjectPath,
        string LaunchProfile,
        string OutPath,
        int TimeoutSeconds,
        string BinDir,
        string SagaProjectPath,
        string SourceTruthGatePath,
        string RuntimeReadinessPath)
    {
        public static CommandOptions Parse(string[] args)
        {
            string project = "";
            string profile = "";
            string output = "";
            string binDir = "";
            string sagaproject = "";
            string sourceTruthGate = "";
            string runtimeReadiness = "";
            var timeout = 5;

            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--project" when i + 1 < args.Length:
                        project = args[++i];
                        break;
                    case "--launch-profile" when i + 1 < args.Length:
                        profile = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    case "--timeout-sec" when i + 1 < args.Length:
                        if (!int.TryParse(args[++i], out timeout) || timeout <= 0)
                        {
                            return Fail("--timeout-sec must be a positive integer.");
                        }
                        break;
                    case "--bin-dir" when i + 1 < args.Length:
                        binDir = args[++i];
                        break;
                    case "--sagaproject" when i + 1 < args.Length:
                        sagaproject = args[++i];
                        break;
                    case "--source-truth-gate" when i + 1 < args.Length:
                        sourceTruthGate = args[++i];
                        break;
                    case "--runtime-readiness" when i + 1 < args.Length:
                        runtimeReadiness = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            if (string.IsNullOrWhiteSpace(project))
            {
                return Fail("--project is required.");
            }
            if (string.IsNullOrWhiteSpace(output))
            {
                return Fail("--out is required.");
            }
            return new CommandOptions(true, "", project, profile, output, timeout, binDir, sagaproject, sourceTruthGate, runtimeReadiness);
        }

        private static CommandOptions Fail(string error)
        {
            return new CommandOptions(false, error, "", "", "", 5, "", "", "", "");
        }
    }
}
