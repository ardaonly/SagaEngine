using System.Diagnostics;
using System.Text.Json.Nodes;

namespace SagaPackager;

internal static class PackagedSmokeRunner
{
    public static async Task<PackageSmokeReport> RunAsync(
        string projectPath,
        string profileId,
        string packageReportPath,
        string outPath,
        string binDir,
        int timeoutSeconds)
    {
        var resolved = new ProjectResolver().Resolve(projectPath, profileId);
        var diagnostics = resolved.Diagnostics.ToList();
        var packageReport = LoadPackageReport(packageReportPath, diagnostics);
        var stagedPackagePath = ReadString(packageReport, "stagedPackagePath");
        var packageProfile = ProjectResolver.ProfileSummary(resolved.Profile);

        var reportsDir = string.IsNullOrWhiteSpace(stagedPackagePath)
            ? Path.GetDirectoryName(Path.GetFullPath(outPath)) ?? Directory.GetCurrentDirectory()
            : Path.Combine(stagedPackagePath, resolved.Profile?.ReportsDirectory ?? "Reports");
        var diagnosticsDir = string.IsNullOrWhiteSpace(stagedPackagePath)
            ? reportsDir
            : Path.Combine(stagedPackagePath, resolved.Profile?.DiagnosticsDirectory ?? "Diagnostics");
        var headlessReport = Path.Combine(reportsDir, "headless_server_report.json");
        var stdoutPath = Path.Combine(reportsDir, "sagapack_smoke_stdout.log");
        var stderrPath = Path.Combine(reportsDir, "sagapack_smoke_stderr.log");

        ProcessInfo process = new();
        if (packageReport is null ||
            ReadString(packageReport, "tool") != "sagapack" ||
            ReadString(packageReport, "command") != "stage" ||
            ReadString(packageReport, "status") != "Passed")
        {
            diagnostics.Add(ProjectResolver.Error(
                "Smoke.PackageReport.Invalid",
                "Smoke",
                packageReportPath,
                "Smoke requires a passed sagapack stage report."));
        }
        if (string.IsNullOrWhiteSpace(stagedPackagePath) || !Directory.Exists(stagedPackagePath))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Smoke.Package.Missing",
                "Smoke",
                stagedPackagePath,
                "Staged package directory is missing."));
        }

        var executable = Path.GetFullPath(Path.Combine(binDir, "MultiplayerSandboxHeadless"));
        if (!File.Exists(executable))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Smoke.Executable.Missing",
                "Smoke",
                executable,
                "MultiplayerSandboxHeadless executable was not found."));
        }

        if (timeoutSeconds <= 0)
        {
            diagnostics.Add(ProjectResolver.Error(
                "Smoke.Timeout.Invalid",
                "Smoke",
                "--timeout-sec",
                "Smoke timeout must be positive."));
        }

        if (!ProjectResolver.HasErrors(diagnostics))
        {
            Directory.CreateDirectory(reportsDir);
            Directory.CreateDirectory(diagnosticsDir);
            var projectManifest = Path.Combine(stagedPackagePath, "Project", "MultiplayerSandbox.sagaproj");
            var args = new List<string>
            {
                "--project",
                projectManifest,
                "--report-out",
                headlessReport,
                "--diagnostics-out",
                diagnosticsDir,
            };
            process = await RunProcessAsync(
                executable,
                args,
                stagedPackagePath,
                stdoutPath,
                stderrPath,
                timeoutSeconds);
            if (process.TimedOut)
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Smoke.Process.Timeout",
                    "Smoke",
                    executable,
                    "Packaged smoke process timed out."));
            }
            else if (process.ExitCode != 0)
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Smoke.Process.ExitFailed",
                    "Smoke",
                    executable,
                    $"Packaged smoke process exited with code {process.ExitCode}."));
            }
            if (!File.Exists(headlessReport))
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Smoke.Report.Missing",
                    "Smoke",
                    headlessReport,
                    "Expected headless smoke diagnostics report was not produced."));
            }
        }

        diagnostics = ProjectResolver.Sort(diagnostics);
        return new PackageSmokeReport
        {
            Status = ProjectResolver.HasErrors(diagnostics) ? "Failed" : "Passed",
            PackageProfile = packageProfile,
            StagedPackagePath = ProjectResolver.Normalize(stagedPackagePath),
            Process = process,
            DiagnosticsReportPaths = File.Exists(headlessReport)
                ? [ProjectResolver.Normalize(headlessReport)]
                : [],
            DeferredStages =
            [
                new DeferredStage
                {
                    Name = "Client packaged launch",
                    Reason = "No accepted bounded ClientHost package evaluation/report seam exists.",
                },
            ],
            Diagnostics = diagnostics,
        };
    }

    private static async Task<ProcessInfo> RunProcessAsync(
        string executable,
        List<string> arguments,
        string workingDirectory,
        string stdoutPath,
        string stderrPath,
        int timeoutSeconds)
    {
        var start = Stopwatch.StartNew();
        await using var stdout = new StreamWriter(stdoutPath);
        await using var stderr = new StreamWriter(stderrPath);
        using var process = new Process();
        process.StartInfo.FileName = executable;
        foreach (var arg in arguments)
        {
            process.StartInfo.ArgumentList.Add(arg);
        }
        process.StartInfo.WorkingDirectory = workingDirectory;
        process.StartInfo.RedirectStandardOutput = true;
        process.StartInfo.RedirectStandardError = true;
        process.StartInfo.UseShellExecute = false;
        process.OutputDataReceived += (_, e) =>
        {
            if (e.Data is not null) stdout.WriteLine(e.Data);
        };
        process.ErrorDataReceived += (_, e) =>
        {
            if (e.Data is not null) stderr.WriteLine(e.Data);
        };
        process.Start();
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();
        var exited = await Task.Run(() => process.WaitForExit(timeoutSeconds * 1000));
        if (!exited)
        {
            try
            {
                process.Kill(entireProcessTree: true);
            }
            catch
            {
                // Process may already have exited between timeout and kill.
            }
        }
        else
        {
            process.WaitForExit();
        }
        start.Stop();
        await stdout.FlushAsync();
        await stderr.FlushAsync();
        return new ProcessInfo
        {
            Executable = ProjectResolver.Normalize(executable),
            Arguments = arguments.Select(ProjectResolver.Normalize).ToList(),
            WorkingDirectory = ProjectResolver.Normalize(workingDirectory),
            ExitCode = exited ? process.ExitCode : null,
            TimedOut = !exited,
            DurationMs = start.ElapsedMilliseconds,
        };
    }

    private static JsonObject? LoadPackageReport(
        string packageReportPath,
        List<PackageDiagnostic> diagnostics)
    {
        if (!File.Exists(packageReportPath))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Smoke.PackageReport.Missing",
                "Smoke",
                packageReportPath,
                "Package report is missing."));
            return null;
        }
        try
        {
            return JsonNode.Parse(File.ReadAllText(packageReportPath)) as JsonObject;
        }
        catch (Exception e)
        {
            diagnostics.Add(ProjectResolver.Error(
                "Smoke.PackageReport.InvalidJson",
                "Smoke",
                packageReportPath,
                $"Package report JSON is invalid: {e.Message}"));
            return null;
        }
    }

    private static string ReadString(JsonObject? obj, string key)
    {
        return obj is not null &&
            obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<string>(out var text)
                ? text
                : "";
    }
}
