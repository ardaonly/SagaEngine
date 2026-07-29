// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using System.Text.Json.Nodes;
using System.Diagnostics;
using SagaProjectKit.Core;

namespace SagaProjectKit;

internal sealed record TemplateFilePlan
{
    public string Path { get; init; } = "";
    public string SourceHash { get; init; } = "";
}

internal sealed record ProjectTemplatePlan
{
    public int SchemaVersion { get; init; } = 1;
    public string TemplateId { get; init; } = "starter-arena";
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string Destination { get; init; } = "";
    public string SeedPath { get; init; } = "";
    public IReadOnlyList<string> DeclaredSubstitutions { get; init; } = ["displayName", "projectId"];
    public IReadOnlyList<TemplateFilePlan> Files { get; init; } = [];
    public string PlanHash { get; init; } = "";
}

internal sealed record TemplateCreateReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagaproject";
    public string Command { get; init; } = "template-create";
    public string Status { get; init; } = "Failed";
    public string ProjectId { get; init; } = "";
    public string Destination { get; init; } = "";
    public string PlanHash { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public IReadOnlyList<SagaProjectKit.Core.ProjectDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class SagaPackProcessException(string code, string message, int? exitCode,
    string stdout, string stderr) : InvalidOperationException(message)
{
    public string Code { get; } = code;
    public int? ExitCode { get; } = exitCode;
    public string StandardOutput { get; } = stdout;
    public string StandardError { get; } = stderr;
}

internal static class ProjectTemplateService
{
    private static readonly HashSet<string> SupportedTemplateIds =
        ["starter-arena", "arena-rush"];

    public static ProjectTemplatePlan Plan(
        string destination,
        string displayName,
        string templateId,
        string seedInput)
    {
        if (!SupportedTemplateIds.Contains(templateId))
        {
            throw new InvalidOperationException($"Unsupported project template id: {templateId}");
        }
        var seed = ResolveSeed(seedInput);
        if (!Directory.Exists(seed))
        {
            throw new InvalidOperationException($"Project template seed is missing: {seed}");
        }
        var files = Directory.GetFiles(seed, "*", SearchOption.AllDirectories)
            .Select(path => new { Full = path, Relative = Normalize(Path.GetRelativePath(seed, path)) })
            .Where(item => !IsExcluded(item.Relative))
            .OrderBy(item => item.Relative, StringComparer.Ordinal)
            .Select(item => new TemplateFilePlan
            {
                Path = item.Relative,
                SourceHash = CanonicalJson.HashFile(item.Full),
            }).ToArray();
        var plan = new ProjectTemplatePlan
        {
            TemplateId = templateId,
            ProjectId = Guid.NewGuid().ToString("D").ToLowerInvariant(),
            DisplayName = string.IsNullOrWhiteSpace(displayName) ? Path.GetFileName(Path.GetFullPath(destination)) : displayName.Trim(),
            Destination = Normalize(Path.GetFullPath(destination)),
            SeedPath = Normalize(seed),
            Files = files,
        };
        return plan with { PlanHash = ComputePlanHash(plan) };
    }

    public static TemplateCreateReport Create(string planPath, string sagaPackHost,
        string sagaPackDll,
        string engineRoot, string runtimeBridgePath)
    {
        ProjectTemplatePlan? plan = null;
        string? staging = null;
        try
        {
            plan = JsonSerializer.Deserialize<ProjectTemplatePlan>(
                File.ReadAllBytes(planPath), CanonicalJson.SerializerOptions)
                ?? throw new InvalidOperationException("Template plan must be a JSON object.");
            if (plan.SchemaVersion != 1 || !SupportedTemplateIds.Contains(plan.TemplateId))
            {
                throw new InvalidOperationException("Template plan schema or template id is unsupported.");
            }
            if (!string.Equals(plan.PlanHash, ComputePlanHash(plan), StringComparison.Ordinal))
            {
                throw new InvalidOperationException("Template plan hash does not match its canonical content.");
            }
            var destination = Path.GetFullPath(plan.Destination);
            if (File.Exists(destination) || Directory.Exists(destination))
            {
                throw new InvalidOperationException("Template destination already exists.");
            }
            var parent = Path.GetDirectoryName(destination)
                ?? throw new InvalidOperationException("Template destination has no parent directory.");
            if (!Directory.Exists(parent))
            {
                throw new InvalidOperationException("Template destination parent does not exist.");
            }
            var seed = Path.GetFullPath(plan.SeedPath);
            staging = Path.Combine(parent, $".{Path.GetFileName(destination)}.saga-staging-{Guid.NewGuid():N}");
            Directory.CreateDirectory(staging);
            foreach (var file in plan.Files.OrderBy(item => item.Path, StringComparer.Ordinal))
            {
                if (!ProjectLoader.IsProjectRelative(file.Path))
                {
                    throw new InvalidOperationException($"Template file path is unsafe: {file.Path}");
                }
                var source = Path.Combine(seed, file.Path);
                if (!File.Exists(source) || CanonicalJson.HashFile(source) != file.SourceHash)
                {
                    throw new InvalidOperationException($"Template seed file is missing or changed: {file.Path}");
                }
                var target = Path.Combine(staging, file.Path);
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                File.Copy(source, target, overwrite: false);
            }
            var manifestPath = Directory.GetFiles(staging, "*.sagaproj", SearchOption.TopDirectoryOnly).Single();
            var manifest = JsonNode.Parse(File.ReadAllText(manifestPath))!.AsObject();
            manifest["projectId"] = plan.ProjectId;
            manifest["displayName"] = plan.DisplayName;
            CanonicalJson.Write(manifestPath, manifest);

            var lockPath = Path.Combine(staging, "SagaPackage.lock");
            var packageRoot = Path.Combine(staging, "Build", "Packages");
            var resolveReport = Path.Combine(staging, "Build", "Reports", "package-resolve.json");
            RunSagaPack(sagaPackHost, sagaPackDll,
                ["resolve", "--project", manifestPath, "--engine-root", engineRoot,
                 "--lock", lockPath, "--generated-out", packageRoot,
                 "--runtime-bridge", runtimeBridgePath, "--out", resolveReport]);
            if (!File.Exists(lockPath) || !File.Exists(lockPath + ".license"))
                throw new InvalidOperationException("Template package resolution did not materialize lock and license sidecar.");
            var verifyReport = Path.Combine(staging, "Build", "Reports", "package-lock-verify.json");
            RunSagaPack(sagaPackHost, sagaPackDll,
                ["lock-verify", "--project", manifestPath, "--engine-root", engineRoot,
                 "--lock", lockPath, "--generated-out", packageRoot,
                 "--runtime-bridge", runtimeBridgePath, "--out", verifyReport]);

            var load = new ProjectLoader().Load(manifestPath, checkReferences: true);
            if (!load.Passed)
            {
                return Failure(plan, load.Diagnostics);
            }
            Directory.Move(staging, destination);
            staging = null;
            return new TemplateCreateReport
            {
                Status = "Passed",
                ProjectId = plan.ProjectId,
                Destination = Normalize(destination),
                PlanHash = plan.PlanHash,
                ProjectHash = load.ProjectHash,
            };
        }
        catch (SagaPackProcessException exception)
        {
            return Failure(plan, [new SagaProjectKit.Core.ProjectDiagnostic
            {
                Code = exception.Code,
                Path = Normalize(string.IsNullOrWhiteSpace(sagaPackDll)
                    ? sagaPackHost : sagaPackDll),
                Message = exception.Message,
            }]);
        }
        catch (Exception exception)
        {
            return Failure(plan, [new SagaProjectKit.Core.ProjectDiagnostic
            {
                Code = "Project.Template.CreateFailed",
                Path = Normalize(plan?.Destination ?? planPath),
                Message = exception.Message,
            }]);
        }
        finally
        {
            if (staging is not null && Directory.Exists(staging))
            {
                Directory.Delete(staging, recursive: true);
            }
        }
    }

    public static string ComputePlanHash(ProjectTemplatePlan plan) => CanonicalJson.Hash(CanonicalJson.Serialize(new
    {
        plan.SchemaVersion,
        plan.TemplateId,
        plan.ProjectId,
        plan.DisplayName,
        plan.Destination,
        plan.DeclaredSubstitutions,
        plan.Files,
    }));

    private static TemplateCreateReport Failure(ProjectTemplatePlan? plan,
        IReadOnlyList<SagaProjectKit.Core.ProjectDiagnostic> diagnostics) => new()
    {
        Status = "Failed",
        ProjectId = plan?.ProjectId ?? "",
        Destination = Normalize(plan?.Destination ?? ""),
        PlanHash = plan?.PlanHash ?? "",
        Diagnostics = diagnostics.OrderBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal).ToArray(),
    };

    private static string ResolveSeed(string input)
    {
        if (string.IsNullOrWhiteSpace(input))
        {
            throw new InvalidOperationException(
                "Project template planning requires an explicit seed path.");
        }
        return Path.GetFullPath(input);
    }

    private static bool IsExcluded(string path) =>
        path is "SagaPackage.lock" ||
        path.StartsWith("Build/", StringComparison.Ordinal) ||
        path.StartsWith("Diagnostics/", StringComparison.Ordinal) ||
        path.Contains("/obj/", StringComparison.Ordinal) ||
        path.Contains("/bin/", StringComparison.Ordinal);

    private static void RunSagaPack(string host, string assembly,
        IReadOnlyList<string> arguments)
    {
        var timeoutMilliseconds = 120_000;
        if (int.TryParse(Environment.GetEnvironmentVariable("SAGAPROJECT_TEMPLATE_PACKAGER_TIMEOUT_MS"),
                out var configuredTimeout) && configuredTimeout > 0)
            timeoutMilliseconds = configuredTimeout;
        var start = new ProcessStartInfo
        {
            FileName = Path.GetFullPath(host),
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
        };
        if (!string.IsNullOrWhiteSpace(assembly))
            start.ArgumentList.Add(Path.GetFullPath(assembly));
        foreach (var argument in arguments) start.ArgumentList.Add(argument);
        using var process = Process.Start(start)
            ?? throw new InvalidOperationException("SagaPackager process could not be started.");
        var stdoutTask = process.StandardOutput.ReadToEndAsync();
        var stderrTask = process.StandardError.ReadToEndAsync();
        if (!process.WaitForExit(timeoutMilliseconds))
        {
            try
            {
                process.Kill(entireProcessTree: true);
            }
            catch (InvalidOperationException)
            {
                // The process exited between the timeout and kill request.
            }
            process.WaitForExit();
            Task.WaitAll([stdoutTask, stderrTask], TimeSpan.FromSeconds(10));
            var timeoutStdout = stdoutTask.IsCompletedSuccessfully ? stdoutTask.Result : "";
            var timeoutStderr = stderrTask.IsCompletedSuccessfully ? stderrTask.Result : "";
            throw new SagaPackProcessException("Project.Template.SagaPackagerTimeout",
                $"SagaPackager timed out after {timeoutMilliseconds} ms. stdout={Summarize(timeoutStdout)} stderr={Summarize(timeoutStderr)}",
                null, timeoutStdout, timeoutStderr);
        }
        Task.WaitAll([stdoutTask, stderrTask], TimeSpan.FromSeconds(10));
        var stdout = stdoutTask.IsCompletedSuccessfully ? stdoutTask.Result : "";
        var stderr = stderrTask.IsCompletedSuccessfully ? stderrTask.Result : "";
        if (process.ExitCode != 0)
            throw new SagaPackProcessException("Project.Template.SagaPackagerFailed",
                $"SagaPackager failed with exit code {process.ExitCode}. stdout={Summarize(stdout)} stderr={Summarize(stderr)}",
                process.ExitCode, stdout, stderr);
    }

    private static string Summarize(string value)
    {
        const int maximum = 4096;
        var trimmed = value.Trim();
        return trimmed.Length <= maximum ? trimmed : trimmed[..maximum] + "…";
    }

    private static string Normalize(string value) => value.Replace('\\', '/');
}
