// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using SagaProjectKit.Core;

namespace SagaPackager;

internal sealed record PackageModuleCatalogEntry
{
    public string PackageId { get; init; } = "";
    public string Version { get; init; } = "";
    public string Module { get; init; } = "";
    public bool Enabled { get; init; }
    public string RuntimeReadiness { get; init; } = "";
    public IReadOnlyList<string> ArtifactKinds { get; init; } = [];
    public IReadOnlyList<string> ApiSurfaces { get; init; } = [];
    public IReadOnlyList<string> BlocksSurfaces { get; init; } = [];
    public IReadOnlyList<string> Capabilities { get; init; } = [];
    public string License { get; init; } = "";
    public string Trust { get; init; } = "";
    public string Origin { get; init; } = "";
    public bool Offline { get; init; } = true;
}

internal sealed record PackageInspectionReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "inspect";
    public string Status { get; init; } = "Failed";
    public bool Offline { get; init; } = true;
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string IntentHash { get; init; } = "";
    public string GraphHash { get; init; } = "";
    public string GenerationId { get; init; } = "";
    public string GenerationHash { get; init; } = "";
    public string GenerationManifestHash { get; init; } = "";
    public IReadOnlyList<PackageModuleCatalogEntry> Modules { get; init; } = [];
    public IReadOnlyList<ProjectDiagnostic> Diagnostics { get; init; } = [];
}

internal static class PackageInspectionService
{
    public static PackageInspectionReport Inspect(string projectInput,
        string engineRoot, string lockPath, string generatedOutputRoot,
        string runtimeBridgePath, string projectRoot = "")
    {
        var verification = PackageResolutionService.Verify(projectInput,
            engineRoot, lockPath, runtimeBridgePath, generatedOutputRoot,
            projectRoot);
        var graph = PackageResolutionService.BuildGraph(projectInput,
            engineRoot, runtimeBridgePath, projectRoot);
        var diagnostics = verification.Diagnostics.ToList();
        var modules = new List<PackageModuleCatalogEntry>();
        var distributionRoot = Path.Combine(ResolveEngineRoot(engineRoot),
            "Engine", "Distribution");
        var packageRoot = Path.Combine(distributionRoot, "Packages");
        if (Directory.Exists(packageRoot))
        {
            foreach (var manifestPath in Directory.GetFiles(packageRoot,
                         "package.json", SearchOption.AllDirectories)
                         .OrderBy(item => item, StringComparer.Ordinal))
            {
                try
                {
                    var manifest = JsonSerializer.Deserialize<BuiltinPackageManifest>(
                        File.ReadAllBytes(manifestPath),
                        CanonicalJson.SerializerOptions)
                        ?? throw new InvalidOperationException(
                            "Package manifest is empty.");
                    if (manifest.SchemaVersion != 1 ||
                        manifest.Trust != "TrustedEngineDistribution" ||
                        manifest.License != "MPL-2.0")
                        throw new InvalidOperationException(
                            "Available built-in package metadata is not trusted MPL-2.0 schema v1.");
                    foreach (var module in manifest.Modules
                                 .OrderBy(item => item, StringComparer.Ordinal))
                    {
                        if (module == "collectibles.lowlevel")
                            throw new InvalidOperationException(
                                "collectibles.lowlevel is not a distributable module.");
                        var enabled = graph.Lock.Packages.Any(package =>
                            package.PackageId == manifest.PackageId &&
                            package.Version == manifest.Version &&
                            package.Modules.Contains(module, StringComparer.Ordinal));
                        var artifacts = manifest.Artifacts.Where(artifact =>
                            string.IsNullOrWhiteSpace(artifact.Module) ||
                            artifact.Module == module).ToArray();
                        var surfaces = new List<string>();
                        var blocks = new List<string>();
                        var capabilities = new HashSet<string>(
                            StringComparer.Ordinal);
                        var readiness = new HashSet<string>(
                            StringComparer.Ordinal);
                        foreach (var artifact in artifacts.Where(item =>
                                     item.Kind == "apiSurface"))
                        {
                            var descriptorPath = Path.Combine(
                                distributionRoot, artifact.Path);
                            using var descriptor = JsonDocument.Parse(
                                File.ReadAllBytes(descriptorPath));
                            var root = descriptor.RootElement;
                            var surface = root.GetProperty("surfaceId")
                                .GetString() ?? "";
                            surfaces.Add(surface);
                            readiness.Add(root.GetProperty("runtimeReadiness")
                                .GetString() ?? "");
                            foreach (var api in root.GetProperty("apis")
                                         .EnumerateArray())
                            {
                                capabilities.Add(api.GetProperty("capability")
                                    .GetString() ?? "");
                                if (enabled &&
                                    api.GetProperty("blockProjection")
                                        .GetString() == "Supported")
                                    blocks.Add(surface);
                            }
                        }
                        modules.Add(new PackageModuleCatalogEntry
                        {
                            PackageId = manifest.PackageId,
                            Version = manifest.Version,
                            Module = module,
                            Enabled = enabled,
                            RuntimeReadiness = string.Join("+",
                                readiness.OrderBy(item => item,
                                    StringComparer.Ordinal)),
                            ArtifactKinds = artifacts.Select(item => item.Kind)
                                .Distinct(StringComparer.Ordinal)
                                .OrderBy(item => item, StringComparer.Ordinal)
                                .ToArray(),
                            ApiSurfaces = surfaces.Distinct(StringComparer.Ordinal)
                                .OrderBy(item => item, StringComparer.Ordinal)
                                .ToArray(),
                            BlocksSurfaces = blocks.Distinct(StringComparer.Ordinal)
                                .OrderBy(item => item, StringComparer.Ordinal)
                                .ToArray(),
                            Capabilities = capabilities.OrderBy(item => item,
                                StringComparer.Ordinal).ToArray(),
                            License = manifest.License,
                            Trust = manifest.Trust,
                            Origin = manifest.Origin,
                        });
                    }
                }
                catch (Exception exception) when (exception is IOException or
                                                   JsonException or
                                                   InvalidOperationException)
                {
                    diagnostics.Add(new ProjectDiagnostic
                    {
                        Code = "Package.Inspect.DistributionInvalid",
                        Path = ProjectLoader.Normalize(manifestPath),
                        Message = exception.Message,
                    });
                }
            }
        }
        else
        {
            diagnostics.Add(new ProjectDiagnostic
            {
                Code = "Package.Inspect.DistributionMissing",
                Path = ProjectLoader.Normalize(packageRoot),
                Message = "Engine package distribution is missing.",
            });
        }

        var generationId = "";
        var generationHash = "";
        var generationManifestHash = "";
        try
        {
            using var current = JsonDocument.Parse(File.ReadAllBytes(Path.Combine(
                generatedOutputRoot, "manifests", "current.json")));
            generationId = current.RootElement.GetProperty("generationId")
                .GetString() ?? "";
            generationHash = current.RootElement.GetProperty("generationHash")
                .GetString() ?? "";
            generationManifestHash = current.RootElement.GetProperty(
                "generationManifestHash").GetString() ?? "";
        }
        catch (Exception exception) when (exception is IOException or
                                           JsonException or KeyNotFoundException)
        {
            diagnostics.Add(new ProjectDiagnostic
            {
                Code = "Package.Inspect.GenerationInvalid",
                Path = ProjectLoader.Normalize(generatedOutputRoot),
                Message = exception.Message,
            });
        }
        var sorted = diagnostics.OrderByDescending(item =>
                item.Severity == "Error")
            .ThenBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal).ToArray();
        return new PackageInspectionReport
        {
            Status = sorted.Any(item => item.Severity == "Error")
                ? "Failed" : "Passed",
            ProjectId = graph.Lock.ProjectId,
            ProjectHash = graph.Lock.ProjectHash,
            IntentHash = graph.Lock.IntentHash,
            GraphHash = graph.Lock.GraphHash,
            GenerationId = generationId,
            GenerationHash = generationHash,
            GenerationManifestHash = generationManifestHash,
            Modules = modules.OrderBy(item => item.PackageId,
                    StringComparer.Ordinal)
                .ThenBy(item => item.Module, StringComparer.Ordinal).ToArray(),
            Diagnostics = sorted,
        };
    }

    private static string ResolveEngineRoot(string input)
    {
        if (!string.IsNullOrWhiteSpace(input))
            return Path.GetFullPath(input);
        var cursor = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (cursor is not null)
        {
            if (Directory.Exists(Path.Combine(cursor.FullName,
                    "Engine", "Distribution")))
                return cursor.FullName;
            cursor = cursor.Parent;
        }
        return Directory.GetCurrentDirectory();
    }
}
