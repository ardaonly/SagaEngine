using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaPreviewGate;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
    private const int MissingInput = 3;
    private const int InternalError = 4;

    private static readonly string[] ForbiddenClaims =
    [
        "product beta",
        "release candidate",
        "production-ready MMO server",
        "production network readiness",
        "full visual scripting",
        "arbitrary C# to blocks",
        "editable graph",
        "patch apply",
        "source mutation",
        "full editor MVP",
        "production editor MVP",
        "full collaboration",
        "enterprise-ready collaboration",
        "cloud workspace",
        "full security model",
        "raw full CTest passed",
    ];

    public static async Task<int> Main(string[] args)
    {
        try
        {
            if (args.Length == 0 || args[0] is "-h" or "--help" or "help")
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
                "quickstart-check" => RunQuickstart(options),
                "accept" => await RunAccept(options),
                "build-matrix" => RunBuildMatrix(options),
                "package" => RunPackage(options),
                "close" => RunClose(options),
                _ => InvalidUsage,
            };
        }
        catch (FileNotFoundException e)
        {
            Console.Error.WriteLine(e.Message);
            return MissingInput;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagapreviewgate error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunQuickstart(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("quickstart-check requires --out.");
            return InvalidUsage;
        }

        var repo = ResolveRepoRoot(options.RepoRoot);
        var project = ResolveProject(repo, options.ProjectPath);
        var binDir = ResolveBinDir(repo, options.BinDir);
        var checks = RequiredQuickstartPaths(repo, project, binDir).ToArray();
        var diagnostics = checks
            .Where(check => check.Required && check.Status != "Present")
            .Select(check => Error("PreviewGate.Quickstart.MissingRequiredPath", "Required quickstart path is missing.", check.Path))
            .ToList();
        diagnostics.AddRange(checks
            .Where(check => !check.Required && check.Status != "Present")
            .Select(check => Warn("PreviewGate.Quickstart.OptionalPathMissing", "Optional quickstart path is unavailable.", check.Path)));
        var status = diagnostics.Any(diagnostic => diagnostic.Severity == "Error") ? "Failed" : "Passed";

        ReportWriter.Write(options.OutPath, new QuickstartReport
        {
            Status = status,
            RepoRoot = repo,
            ProjectPath = project,
            BinDir = binDir,
            Checks = checks,
            Diagnostics = diagnostics,
        });
        return status == "Passed" ? Passed : Failed;
    }

    private static async Task<int> RunAccept(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutRoot))
        {
            Console.Error.WriteLine("accept requires --out-root.");
            return InvalidUsage;
        }

        var repo = ResolveRepoRoot(options.RepoRoot);
        var project = ResolveProject(repo, options.ProjectPath);
        var binDir = ResolveBinDir(repo, options.BinDir);
        var outRoot = Path.GetFullPath(options.OutRoot);
        Directory.CreateDirectory(outRoot);
        var steps = BuildAcceptanceSteps(repo, project, binDir, outRoot).ToList();

        if (options.EvidenceOnly)
        {
            steps = steps
                .Select(step => step with
                {
                    Status = ReportStatus(Path.Combine(outRoot, step.ReportPath)),
                    Diagnostics = File.Exists(Path.Combine(outRoot, step.ReportPath))
                        ? Array.Empty<Diagnostic>()
                        : [Error("PreviewGate.Acceptance.EvidenceMissing", "Required acceptance evidence is missing.", step.ReportPath)],
                })
                .ToList();
        }
        else
        {
            var updated = new List<AcceptanceStep>();
            foreach (var step in steps)
            {
                updated.Add(await RunAcceptanceStep(repo, step, options.TimeoutSeconds));
            }
            steps = updated;
        }

        var diagnostics = steps.SelectMany(step => step.Diagnostics).ToList();
        var status = steps.All(step => step.Status == "Passed") ? "Passed" : "Failed";
        ReportWriter.Write(Path.Combine(outRoot, "mvp_acceptance_report.json"), new AcceptanceReport
        {
            Status = status,
            RepoRoot = repo,
            ProjectPath = project,
            Steps = steps,
            Diagnostics = diagnostics,
        });
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunBuildMatrix(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("build-matrix requires --out.");
            return InvalidUsage;
        }

        var repo = ResolveRepoRoot(options.RepoRoot);
        var localEvidence = options.EvidencePath;
        var entries = new List<BuildMatrixEntry>
        {
            new()
            {
                Platform = RuntimeInformation.OSDescription,
                Configuration = string.IsNullOrWhiteSpace(options.Configuration) ? "RelWithDebInfo" : options.Configuration,
                EvidenceKind = "focused-local-build",
                Status = !string.IsNullOrWhiteSpace(localEvidence) && File.Exists(Path.GetFullPath(localEvidence)) ? "Passed" : "MissingEvidence",
                EvidencePath = Normalize(repo, localEvidence),
                Notes = "Primary local platform evidence for focused Technical Preview targets.",
            },
            new()
            {
                Platform = "Windows/MSVC",
                Configuration = "RelWithDebInfo",
                EvidenceKind = "sampled-build",
                Status = "Unavailable",
                Notes = "No Windows/MSVC evidence was collected in this local Block J pass.",
            },
            new()
            {
                Platform = "Linux",
                Configuration = "Debug/Release sampled",
                EvidenceKind = "sampled-build",
                Status = "Unavailable",
                Notes = "Only focused local Technical Preview evidence is required unless separately collected.",
            },
        };
        var diagnostics = entries
            .Where(entry => entry.Status == "MissingEvidence")
            .Select(entry => Error("PreviewGate.BuildMatrix.EvidenceMissing", "Build matrix evidence file is missing.", entry.EvidencePath))
            .ToArray();
        var status = diagnostics.Length == 0 ? "Passed" : "Failed";
        ReportWriter.Write(options.OutPath, new BuildMatrixReport
        {
            Status = status,
            Entries = entries,
            Diagnostics = diagnostics,
        });
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunPackage(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutRoot))
        {
            Console.Error.WriteLine("package requires --out-root.");
            return InvalidUsage;
        }

        var repo = ResolveRepoRoot(options.RepoRoot);
        var packageRoot = Path.GetFullPath(Path.Combine(options.OutRoot, "SagaEngine-0.1-TechnicalPreview"));
        Directory.CreateDirectory(packageRoot);
        var files = TechnicalPreviewPackageFiles(repo).ToList();
        var staged = new List<PackageFile>();
        foreach (var file in files)
        {
            var source = Path.Combine(repo, file.SourcePath);
            var destination = Path.Combine(packageRoot, file.PackagePath);
            if (File.Exists(source))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                File.Copy(source, destination, overwrite: true);
                staged.Add(file with { Status = "Present" });
            }
            else
            {
                staged.Add(file with { Status = "Missing" });
            }
        }

        var diagnostics = staged
            .Where(file => file.Status != "Present")
            .Select(file => Error("PreviewGate.Package.FileMissing", "Technical Preview package source file is missing.", file.SourcePath))
            .ToArray();
        var status = diagnostics.Length == 0 ? "Passed" : "Failed";
        var report = new TechnicalPreviewPackageReport
        {
            Status = status,
            PackageRoot = packageRoot,
            Files = staged,
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(Path.Combine(packageRoot, "technical_preview_package_report.json"), report);
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunClose(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath) || string.IsNullOrWhiteSpace(options.EvidenceRoot))
        {
            Console.Error.WriteLine("close requires --evidence-root and --out.");
            return InvalidUsage;
        }

        var repo = ResolveRepoRoot(options.RepoRoot);
        var evidenceRoot = Path.GetFullPath(options.EvidenceRoot);
        var evidence = ClosureEvidence(evidenceRoot).ToArray();
        var diagnostics = evidence
            .Where(item => item.Status != "Passed")
            .Select(item => Error("PreviewGate.Closure.EvidenceMissingOrFailed", "Required Technical Preview closure evidence is missing or failed.", item.EvidencePath))
            .ToList();
        var status = diagnostics.Count == 0 ? "Accepted" : "Blocked";
        var report = new ClosureReport
        {
            Status = status,
            ImplementedMatrix = evidence,
            EvidenceMatrix = evidence,
            ForbiddenClaimsPreserved = ForbiddenClaims,
            KnownDebt =
            [
                "No product beta or release-candidate claim.",
                "No production MMO or production network readiness claim.",
                "No full visual scripting, arbitrary C# to blocks, patch apply, or source mutation.",
                "No full collaboration, cloud workspace, enterprise readiness, or full security model.",
                "No raw full CTest claim.",
            ],
            Hedef2OpeningRecommendation = status == "Accepted"
                ? "Hedef 2 may open only as Small-Team Alpha planning after Phase 65 closure."
                : "Do not open Hedef 2 until the blocker list is resolved.",
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        _ = repo;
        return status == "Accepted" ? Passed : Failed;
    }

    private static IEnumerable<PreviewCheck> RequiredQuickstartPaths(string repo, string project, string binDir)
    {
        var paths = new (string Id, string Kind, string Path, bool Required)[]
        {
            ("sagaproject", "tool", "Tools/SagaProjectKit/sagaproject", true),
            ("sagalaunch", "tool", "Tools/SagaLaunchLab/sagalaunch", true),
            ("sagaprobe", "tool", "Tools/SagaProbe/sagaprobe", true),
            ("sagapack", "tool", "Tools/SagaPackager/sagapack", true),
            ("sagascript", "tool", "Tools/SagaScript/sagascript", true),
            ("sagaviewkit", "tool", "Tools/SagaViewKit/sagaviewkit", true),
            ("sagadocguard", "tool", "Tools/SagaDocGuard/sagadocguard", true),
            ("sample-project", "sample", Normalize(repo, project), true),
            ("launch-profiles", "sample", "samples/MultiplayerSandbox/launch_profiles.json", true),
            ("package-profiles", "sample", "samples/MultiplayerSandbox/package_profiles.json", true),
            ("sample-scripts", "sample", "samples/MultiplayerSandbox/Scripts", true),
            ("headless-binary-dir", "build-output", Normalize(repo, binDir), false),
        };

        foreach (var item in paths)
        {
            var fullPath = Path.IsPathRooted(item.Path) ? item.Path : Path.Combine(repo, item.Path);
            yield return new PreviewCheck
            {
                Id = item.Id,
                Kind = item.Kind,
                Path = item.Path.Replace('\\', '/'),
                Required = item.Required,
                Status = File.Exists(fullPath) || Directory.Exists(fullPath) ? "Present" : "Missing",
            };
        }
    }

    private static IEnumerable<AcceptanceStep> BuildAcceptanceSteps(string repo, string project, string binDir, string outRoot)
    {
        var scriptOut = Path.Combine(outRoot, "SagaScript");
        var viewOut = Path.Combine(outRoot, "ViewCapabilities");
        var patchRequest = Path.Combine(scriptOut, "patch_request.json");
        var steps = new List<(string Id, string Tool, string Command, string ReportPath, string[] Args)>
        {
            ("project-validate", "sagaproject", "validate", "project_validation_report.json",
                [Tool(repo, "SagaProjectKit", "sagaproject"), "validate", "--project", project, "--out", Path.Combine(outRoot, "project_validation_report.json")]),
            ("project-resolve", "sagaproject", "resolve", "project_resolution.json",
                [Tool(repo, "SagaProjectKit", "sagaproject"), "resolve", "--project", project, "--out", Path.Combine(outRoot, "project_resolution.json")]),
            ("project-doctor", "sagaproject", "doctor", "project_doctor_report.json",
                [Tool(repo, "SagaProjectKit", "sagaproject"), "doctor", "--project", project, "--out", Path.Combine(outRoot, "project_doctor_report.json")]),
            ("sagascript-analyze", "sagascript", "analyze", "SagaScript/analysis_report.json",
                [Tool(repo, "SagaScript", "sagascript"), "analyze", "--source", Path.Combine(Path.GetDirectoryName(project)!, "Scripts"), "--out", scriptOut]),
            ("sagascript-bindings", "sagascript", "emit-bindings", "SagaScript/runtime_bindings.json",
                [Tool(repo, "SagaScript", "sagascript"), "emit-bindings", "--source", Path.Combine(Path.GetDirectoryName(project)!, "Scripts"), "--out", scriptOut]),
            ("sagascript-source-map", "sagascript", "source-map", "SagaScript/source_map.json",
                [Tool(repo, "SagaScript", "sagascript"), "source-map", "--source", Path.Combine(Path.GetDirectoryName(project)!, "Scripts"), "--out", scriptOut]),
            ("sagascript-project-blocks", "sagascript", "project-blocks", "SagaScript/projection_report.json",
                [Tool(repo, "SagaScript", "sagascript"), "project-blocks", "--source", Path.Combine(Path.GetDirectoryName(project)!, "Scripts"), "--out", scriptOut]),
            ("sagascript-patch-preview", "sagascript", "patch-preview", "SagaScript/patch_preview.json",
                [Tool(repo, "SagaScript", "sagascript"), "patch-preview", "--source", Path.Combine(Path.GetDirectoryName(project)!, "Scripts"), "--source-map", Path.Combine(scriptOut, "source_map.json"), "--request", patchRequest, "--out", Path.Combine(scriptOut, "patch_preview.json")]),
            ("view-profiles", "sagaviewkit", "emit-profiles", "ViewCapabilities/view_capability_report.json",
                [Tool(repo, "SagaViewKit", "sagaviewkit"), "emit-profiles", "--out", viewOut]),
            ("view-simple-honesty", "sagaviewkit", "check-simple", "ViewCapabilities/simple_view_honesty_report.json",
                [Tool(repo, "SagaViewKit", "sagaviewkit"), "check-simple", "--projection", Path.Combine(scriptOut, "projection_report.json"), "--profile", "simple_view", "--node-metadata", Path.Combine(scriptOut, "node_metadata.json"), "--evidence-root", outRoot, "--out", Path.Combine(viewOut, "simple_view_honesty_report.json")]),
            ("launch-accept", "sagalaunch", "accept", "acceptance_report.json",
                [Tool(repo, "SagaLaunchLab", "sagalaunch"), "accept", "--project", project, "--launch-profile", "local-server-headless", "--out", Path.Combine(outRoot, "acceptance_report.json"), "--timeout-sec", "5", "--bin-dir", binDir]),
            ("diagnostics-summary", "sagaprobe", "summarize", "diagnostics_summary.json",
                [Tool(repo, "SagaProbe", "sagaprobe"), "summarize", "--input", Path.Combine(outRoot, "acceptance_report.json"), "--out", Path.Combine(outRoot, "diagnostics_summary.json")]),
            ("package-validate", "sagapack", "validate", "package_report.json",
                [Tool(repo, "SagaPackager", "sagapack"), "validate", "--project", project, "--profile", "technical-preview-server-headless", "--out", Path.Combine(outRoot, "package_report.json")]),
            ("package-stage", "sagapack", "stage", "package_stage_report.json",
                [Tool(repo, "SagaPackager", "sagapack"), "stage", "--project", project, "--profile", "technical-preview-server-headless", "--out", Path.Combine(outRoot, "package_stage_report.json")]),
            ("publish-check", "sagapack", "publish-check", "publish_report.json",
                [Tool(repo, "SagaPackager", "sagapack"), "publish-check", "--project", project, "--profile", "technical-preview-server-headless", "--package-report", Path.Combine(outRoot, "package_stage_report.json"), "--diagnostics-summary", Path.Combine(outRoot, "diagnostics_summary.json"), "--out", Path.Combine(outRoot, "publish_report.json")]),
            ("package-smoke", "sagapack", "smoke", "package_smoke_report.json",
                [Tool(repo, "SagaPackager", "sagapack"), "smoke", "--project", project, "--profile", "technical-preview-server-headless", "--package-report", Path.Combine(outRoot, "package_stage_report.json"), "--out", Path.Combine(outRoot, "package_smoke_report.json"), "--timeout-sec", "5", "--bin-dir", binDir]),
            ("docguard-scan", "sagadocguard", "scan", "docguard_report.json",
                [Tool(repo, "SagaDocGuard", "sagadocguard"), "scan", "--docs", Path.Combine(repo, "docs"), "--evidence-root", repo, "--out", Path.Combine(outRoot, "docguard_report.json")]),
        };

        return steps.Select((step, index) => new AcceptanceStep
        {
            Sequence = index + 1,
            Id = step.Id,
            Tool = step.Tool,
            Command = step.Command,
            ReportPath = step.ReportPath,
            Process = new ProcessRecord
            {
                Executable = step.Args[0],
                Arguments = step.Args.Skip(1).ToArray(),
                WorkingDirectory = repo,
            },
        });
    }

    private static async Task<AcceptanceStep> RunAcceptanceStep(string repo, AcceptanceStep step, int timeoutSeconds)
    {
        if (step.Id == "sagascript-patch-preview")
        {
            TryWritePatchRequest(repo, step);
        }

        var process = step.Process!;
        var result = await RunProcess(process.Executable, process.Arguments, repo, timeoutSeconds);
        var status = result.ExitCode == 0 && !result.TimedOut ? "Passed" : "Failed";
        var diagnostics = status == "Passed"
            ? Array.Empty<Diagnostic>()
            : [Error("PreviewGate.Acceptance.StepFailed", $"Acceptance step failed: {step.Id}.", step.ReportPath)];

        return step with
        {
            Status = status,
            Process = result,
            Diagnostics = diagnostics,
        };
    }

    private static async Task<ProcessRecord> RunProcess(string executable, IReadOnlyList<string> args, string repo, int timeoutSeconds)
    {
        using var process = new Process();
        process.StartInfo.FileName = executable;
        foreach (var arg in args)
        {
            process.StartInfo.ArgumentList.Add(arg);
        }
        process.StartInfo.WorkingDirectory = repo;
        process.StartInfo.RedirectStandardOutput = true;
        process.StartInfo.RedirectStandardError = true;
        process.StartInfo.UseShellExecute = false;

        process.Start();
        var timeoutTask = Task.Delay(TimeSpan.FromSeconds(Math.Max(1, timeoutSeconds)));
        var exitTask = process.WaitForExitAsync();
        var timedOut = await Task.WhenAny(exitTask, timeoutTask) == timeoutTask;
        if (timedOut)
        {
            try
            {
                process.Kill(entireProcessTree: true);
            }
            catch
            {
                // Best effort cleanup for a failing evidence step.
            }
        }
        else
        {
            await exitTask;
        }

        return new ProcessRecord
        {
            Executable = executable,
            Arguments = args,
            WorkingDirectory = repo,
            ExitCode = timedOut ? -1 : process.ExitCode,
            TimedOut = timedOut,
        };
    }

    private static void TryWritePatchRequest(string repo, AcceptanceStep step)
    {
        var args = step.Process!.Arguments;
        var sourceMapPath = ArgAfter(args, "--source-map");
        var requestPath = ArgAfter(args, "--request");
        if (string.IsNullOrWhiteSpace(sourceMapPath) || string.IsNullOrWhiteSpace(requestPath) || !File.Exists(sourceMapPath))
        {
            return;
        }

        var sourceMap = JsonNode.Parse(File.ReadAllText(sourceMapPath)) as JsonObject;
        var behaviors = sourceMap?["behaviors"] as JsonArray;
        var files = sourceMap?["files"] as JsonArray;
        var hash = files?.OfType<JsonObject>().FirstOrDefault()?["sourceHash"]?.GetValue<string>() ?? "";
        var node = behaviors?
            .OfType<JsonObject>()
            .SelectMany(behavior => (behavior["nodes"] as JsonArray)?.OfType<JsonObject>() ?? [])
            .FirstOrDefault(item =>
                string.Equals(item["kind"]?.GetValue<string>(), "StringLiteral", StringComparison.Ordinal) &&
                item["readOnly"]?.GetValue<bool>() == false);
        if (node is null)
        {
            return;
        }

        var request = new JsonObject
        {
            ["operation"] = "ReplaceStringLiteral",
            ["nodeId"] = node["nodeId"]?.GetValue<string>() ?? "",
            ["baseSourceHash"] = hash,
            ["replacement"] = "technical_preview",
        };
        Directory.CreateDirectory(Path.GetDirectoryName(requestPath)!);
        File.WriteAllText(requestPath, request.ToJsonString(ReportWriter.Options) + Environment.NewLine);
        _ = repo;
    }

    private static string ReportStatus(string path)
    {
        if (!File.Exists(path))
        {
            return "Missing";
        }

        try
        {
            var json = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            return json?["status"]?.GetValue<string>() is { Length: > 0 } status ? status : "Passed";
        }
        catch
        {
            return "Failed";
        }
    }

    private static IEnumerable<PackageFile> TechnicalPreviewPackageFiles(string repo)
    {
        _ = repo;
        var files = new[]
        {
            "docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_QUICKSTART.md",
            "docs/internal/product-history/SAGA_RELEASE_VOCABULARY_AND_CLAIM_LEVELS.md",
            "docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_CLOSURE_PHASE60_65.md",
            "docs/internal/product-history/SAGA_PRODUCT_DEFINITION.md",
            "docs/product/SAGAPROJ_SCHEMA_V0.md",
            "docs/internal/product-history/MULTIPLAYER_SANDBOX_PLAYABLE_VERTICAL_SLICE_PHASE20_25.md",
            "docs/internal/product-history/PACKAGING_PUBLISH_PROOF_PHASE29_33.md",
            "docs/internal/product-history/SAGAWEAVER_SAGASCRIPT_MVP_PHASE34_43.md",
            "docs/architecture/SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT.md",
            "docs/architecture/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md",
            "samples/MultiplayerSandbox/README.md",
            "samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj",
            "samples/MultiplayerSandbox/launch_profiles.json",
            "samples/MultiplayerSandbox/package_profiles.json",
            "Tools/SagaPreviewGate/README.md",
        };
        return files.Select(file => new PackageFile { SourcePath = file, PackagePath = file });
    }

    private static IEnumerable<ClosureMatrixRow> ClosureEvidence(string evidenceRoot)
    {
        var evidence = new (string Id, string Title, string Path)[]
        {
            ("phase60", "Clean Onboarding Command", "quickstart_report.json"),
            ("phase61", "Full MVP Acceptance Script", "Acceptance/mvp_acceptance_report.json"),
            ("phase62", "Cross-Platform Build Evidence", "build_matrix_report.json"),
            ("phase63", "Known Limitations / Non-Claims Freeze", "docguard_report.json"),
            ("phase64", "SagaEngine 0.1 Technical Preview Package", "Package/SagaEngine-0.1-TechnicalPreview/technical_preview_package_report.json"),
        };
        foreach (var item in evidence)
        {
            var fullPath = Path.Combine(evidenceRoot, item.Path);
            var status = ReportStatus(fullPath);
            yield return new ClosureMatrixRow
            {
                Id = item.Id,
                Title = item.Title,
                Status = status == "Passed" || status == "Accepted" ? "Passed" : status,
                EvidencePath = item.Path,
                ClaimLevel = "Technical Preview evidence",
            };
        }
    }

    private static string ResolveRepoRoot(string repoRoot)
    {
        return Path.GetFullPath(string.IsNullOrWhiteSpace(repoRoot) ? Directory.GetCurrentDirectory() : repoRoot);
    }

    private static string ResolveProject(string repo, string project)
    {
        return Path.GetFullPath(string.IsNullOrWhiteSpace(project)
            ? Path.Combine(repo, "samples", "MultiplayerSandbox", "MultiplayerSandbox.sagaproj")
            : project);
    }

    private static string ResolveBinDir(string repo, string binDir)
    {
        return Path.GetFullPath(string.IsNullOrWhiteSpace(binDir)
            ? Path.Combine(repo, "build", "RelWithDebInfo-0.0.9", "bin")
            : binDir);
    }

    private static string Tool(string repo, string tool, string executable)
    {
        return Path.Combine(repo, "Tools", tool, executable);
    }

    private static string Normalize(string root, string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return "";
        }
        var full = Path.GetFullPath(Path.IsPathRooted(path) ? path : Path.Combine(root, path));
        return Path.GetRelativePath(root, full).Replace('\\', '/');
    }

    private static string ArgAfter(IReadOnlyList<string> args, string name)
    {
        for (var i = 0; i + 1 < args.Count; ++i)
        {
            if (args[i] == name)
            {
                return args[i + 1];
            }
        }
        return "";
    }

    private static Diagnostic Error(string code, string message, string path = "") =>
        new() { Severity = "Error", Code = code, Message = message, Path = path };

    private static Diagnostic Warn(string code, string message, string path = "") =>
        new() { Severity = "Warning", Code = code, Message = message, Path = path };

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaPreviewGate CLI");
        Console.Error.WriteLine("  sagapreviewgate quickstart-check --out <report> [--repo-root <path>] [--project <path>] [--bin-dir <dir>]");
        Console.Error.WriteLine("  sagapreviewgate accept --out-root <dir> [--repo-root <path>] [--project <path>] [--bin-dir <dir>] [--evidence-only]");
        Console.Error.WriteLine("  sagapreviewgate build-matrix --out <report> [--repo-root <path>] [--evidence <path>] [--configuration <name>]");
        Console.Error.WriteLine("  sagapreviewgate package --out-root <dir> [--repo-root <path>]");
        Console.Error.WriteLine("  sagapreviewgate close --evidence-root <dir> --out <report> [--repo-root <path>]");
    }

    private sealed record CommandOptions(
        bool Ok,
        string Error,
        string RepoRoot,
        string ProjectPath,
        string BinDir,
        string OutPath,
        string OutRoot,
        string EvidenceRoot,
        string EvidencePath,
        string Configuration,
        bool EvidenceOnly,
        int TimeoutSeconds)
    {
        public static CommandOptions Parse(string[] args)
        {
            string repoRoot = "";
            string project = "";
            string binDir = "";
            string output = "";
            string outRoot = "";
            string evidenceRoot = "";
            string evidence = "";
            string configuration = "";
            var evidenceOnly = false;
            var timeout = 30;

            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--repo-root" when i + 1 < args.Length:
                        repoRoot = args[++i];
                        break;
                    case "--project" when i + 1 < args.Length:
                        project = args[++i];
                        break;
                    case "--bin-dir" when i + 1 < args.Length:
                        binDir = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    case "--out-root" when i + 1 < args.Length:
                        outRoot = args[++i];
                        break;
                    case "--evidence-root" when i + 1 < args.Length:
                        evidenceRoot = args[++i];
                        break;
                    case "--evidence" when i + 1 < args.Length:
                        evidence = args[++i];
                        break;
                    case "--configuration" when i + 1 < args.Length:
                        configuration = args[++i];
                        break;
                    case "--timeout-sec" when i + 1 < args.Length:
                        if (!int.TryParse(args[++i], out timeout) || timeout <= 0)
                        {
                            return Fail("--timeout-sec must be a positive integer.");
                        }
                        break;
                    case "--evidence-only":
                        evidenceOnly = true;
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", repoRoot, project, binDir, output, outRoot, evidenceRoot, evidence, configuration, evidenceOnly, timeout);
        }

        private static CommandOptions Fail(string error) =>
            new(false, error, "", "", "", "", "", "", "", "", false, 30);
    }
}
