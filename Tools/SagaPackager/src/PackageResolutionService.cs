// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;
using SagaProjectKit.Core;

namespace SagaPackager;

internal sealed record BuiltinPackageManifest
{
    public int SchemaVersion { get; init; }
    public string PackageId { get; init; } = "";
    public string Version { get; init; } = "";
    public string Trust { get; init; } = "";
    public string License { get; init; } = "";
    public string Origin { get; init; } = "";
    public IReadOnlyList<string> Modules { get; init; } = [];
    public IReadOnlyList<BuiltinArtifact> Artifacts { get; init; } = [];
}

internal sealed record BuiltinArtifact
{
    public string ArtifactId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Module { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record LockedArtifact
{
    public string ArtifactId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string DistributionPath { get; init; } = "";
    public string ArtifactHash { get; init; } = "";
    public string ResolvedAssemblyLogicalPath { get; init; } = "";
    public string ResolvedAssemblyHash { get; init; } = "";
    public string TargetFramework { get; init; } = "";
    public string RuntimeCompatibility { get; init; } = "";
    public string Trust { get; init; } = "";
}

internal sealed record LockedPackage
{
    public string PackageId { get; init; } = "";
    public string Version { get; init; } = "";
    public string SourceIdentity { get; init; } = "";
    public string SourceHash { get; init; } = "";
    public string Trust { get; init; } = "";
    public string License { get; init; } = "";
    public string Origin { get; init; } = "";
    public IReadOnlyList<string> Modules { get; init; } = [];
    public IReadOnlyList<LockedArtifact> Artifacts { get; init; } = [];
}

internal sealed record SagaPackageLock
{
    public int SchemaVersion { get; init; } = 1;
    public string EngineVersion { get; init; } = "0.0.11";
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string IntentHash { get; init; } = "";
    public string GraphHash { get; init; } = "";
    public IReadOnlyList<LockedPackage> Packages { get; init; } = [];
}

internal sealed record PackageResolutionReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "resolve";
    public string Status { get; init; } = "Failed";
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string IntentHash { get; init; } = "";
    public string GraphHash { get; init; } = "";
    public string LockPath { get; init; } = "";
    public string GeneratedOutputRoot { get; init; } = "";
    public IReadOnlyList<SagaProjectKit.Core.ProjectDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed record VerifiedPackageGraph
{
    public ProjectLoadResult Project { get; init; } = new();
    public string DistributionRoot { get; init; } = "";
    public SagaPackageLock Lock { get; init; } = new();
    public VerifiedRuntimeBridge RuntimeBridge { get; init; } = new();
    public IReadOnlyList<SagaProjectKit.Core.ProjectDiagnostic> Diagnostics { get; init; } = [];
    public bool Passed => Project.Passed && Diagnostics.All(item => item.Severity != "Error");
}

internal sealed record VerifiedRuntimeBridge
{
    public string SourcePath { get; init; } = "";
    public byte[] Bytes { get; init; } = [];
    public string Hash { get; init; } = "";
}

internal sealed class PackageResolutionException(string code, string path, string message)
    : InvalidOperationException(message)
{
    public string Code { get; } = code;
    public string DiagnosticPath { get; } = path;
}

internal sealed record GenerationCurrent
{
    public int SchemaVersion { get; init; }
    public string GenerationId { get; init; } = "";
    public string GenerationHash { get; init; } = "";
    public string GraphHash { get; init; } = "";
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string GenerationManifestPath { get; init; } = "";
    public string GenerationManifestHash { get; init; } = "";
}

internal sealed record GenerationFile
{
    public string Path { get; init; } = "";
    public string Hash { get; init; } = "";
}

internal sealed record GenerationManifest
{
    public int SchemaVersion { get; init; }
    public string GenerationId { get; init; } = "";
    public string GenerationHash { get; init; } = "";
    public string GraphHash { get; init; } = "";
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public IReadOnlyList<GenerationFile> Files { get; init; } = [];
}

internal static class PackageResolutionService
{
    private const string EngineVersion = "0.0.11";
    private static readonly JsonSerializerOptions Options = CanonicalJson.SerializerOptions;

    public static PackageResolutionReport Resolve(string projectInput, string engineRoot,
        string lockPath, string generatedOutputRoot, string runtimeBridgePath)
    {
        var graph = BuildGraph(projectInput, engineRoot, runtimeBridgePath);
        if (!graph.Passed)
        {
            return Report("resolve", graph, lockPath, generatedOutputRoot, graph.Diagnostics);
        }
        try
        {
            PublishGeneration(graph, lockPath, generatedOutputRoot, runtimeBridgePath);
            return Report("resolve", graph, lockPath, generatedOutputRoot, []);
        }
        catch (PackageResolutionException exception)
        {
            return Report("resolve", graph, lockPath, generatedOutputRoot,
                [Error(exception.Code, exception.DiagnosticPath, exception.Message)]);
        }
        catch (Exception exception)
        {
            return Report("resolve", graph, lockPath, generatedOutputRoot,
                [Error("Package.Resolve.PublishFailed", lockPath, exception.Message)]);
        }
    }

    public static PackageResolutionReport Verify(string projectInput, string engineRoot, string lockPath,
        string runtimeBridgePath, string generatedOutputRoot = "")
    {
        var graph = BuildGraph(projectInput, engineRoot, runtimeBridgePath);
        var diagnostics = graph.Diagnostics.ToList();
        SagaPackageLock? existing = null;
        if (!File.Exists(lockPath))
        {
            diagnostics.Add(Error("Package.Lock.Missing", lockPath, "SagaPackage.lock is missing."));
        }
        else
        {
            try
            {
                existing = JsonSerializer.Deserialize<SagaPackageLock>(File.ReadAllBytes(lockPath), Options);
            }
            catch (JsonException exception)
            {
                diagnostics.Add(Error("Package.Lock.ParseFailed", lockPath, exception.Message));
            }
        }
        if (existing is not null)
        {
            if (existing.SchemaVersion != 1 || existing.EngineVersion != EngineVersion)
            {
                diagnostics.Add(Error("Package.Lock.SchemaOrEngineMismatch", lockPath,
                    "Lock schemaVersion must be 1 and engineVersion must be exactly 0.0.11."));
            }
            if (existing.ProjectId != graph.Lock.ProjectId || existing.ProjectHash != graph.Lock.ProjectHash ||
                existing.IntentHash != graph.Lock.IntentHash)
            {
                diagnostics.Add(Error("Package.Lock.StaleIntent", lockPath,
                    "Lock project identity, project hash, or dependency intent is stale."));
            }
            var existingArtifacts = new Dictionary<string, LockedArtifact>(StringComparer.Ordinal);
            foreach (var artifact in existing.Packages.SelectMany(package => package.Artifacts))
            {
                if (!existingArtifacts.TryAdd(artifact.ArtifactId, artifact))
                {
                    diagnostics.Add(Error("Package.Lock.DuplicateArtifact", artifact.ArtifactId,
                        "Locked artifact identities must be unique."));
                }
            }
            foreach (var expected in graph.Lock.Packages.SelectMany(package => package.Artifacts))
            {
                if (!existingArtifacts.TryGetValue(expected.ArtifactId, out var actual) ||
                    actual.ArtifactHash != expected.ArtifactHash || actual.DistributionPath != expected.DistributionPath)
                {
                    diagnostics.Add(Error("Package.Lock.ArtifactHashMismatch", expected.ArtifactId,
                        "Locked artifact path or hash does not match the trusted engine distribution."));
                }
            }
            if (existing.GraphHash != graph.Lock.GraphHash ||
                CanonicalJson.Hash(CanonicalJson.Serialize(existing.Packages)) !=
                CanonicalJson.Hash(CanonicalJson.Serialize(graph.Lock.Packages)))
            {
                diagnostics.Add(Error("Package.Lock.GraphMismatch", lockPath,
                    "Locked package graph does not match the verified dependency graph."));
            }
        }
        if (!string.IsNullOrWhiteSpace(generatedOutputRoot))
            ValidateSelectedGeneration(generatedOutputRoot, lockPath, existing, graph, diagnostics);
        return Report("lock-verify", graph, lockPath, generatedOutputRoot, Sort(diagnostics));
    }

    public static VerifiedPackageGraph BuildGraph(string projectInput, string engineRoot,
        string runtimeBridgePath = "")
    {
        var project = new ProjectLoader().Load(projectInput, checkReferences: true);
        var diagnostics = project.Diagnostics.ToList();
        var runtimeBridge = ReadRuntimeBridge(runtimeBridgePath, diagnostics);
        var distributionRoot = Path.GetFullPath(Path.Combine(ResolveEngineRoot(engineRoot), "Engine", "Distribution"));
        var packages = new List<LockedPackage>();
        if (project.Manifest is not null)
        {
            foreach (var intent in project.Manifest.Dependencies.OrderBy(item => item.PackageId, StringComparer.Ordinal))
            {
                ResolveDependency(intent, distributionRoot, runtimeBridge, packages, diagnostics);
            }
        }
        var orderedPackages = packages.OrderBy(item => item.PackageId, StringComparer.Ordinal)
            .ThenBy(item => item.Version, StringComparer.Ordinal).ToArray();
        foreach (var duplicate in orderedPackages.SelectMany(item => item.Artifacts)
                     .GroupBy(item => item.ArtifactId, StringComparer.Ordinal).Where(item => item.Count() > 1))
            diagnostics.Add(Error("Package.Resolve.DuplicateArtifact", duplicate.Key,
                "Package artifact identities must be unique across the complete resolved graph."));
        var intentPayload = project.Manifest?.Dependencies
            .OrderBy(item => item.PackageId, StringComparer.Ordinal)
            .Select(item => new
            {
                item.PackageId,
                item.Version,
                Modules = item.Modules.OrderBy(module => module, StringComparer.Ordinal).ToArray(),
            }).ToArray() ?? [];
        var intentHash = CanonicalJson.Hash(CanonicalJson.Serialize(intentPayload));
        var graphHash = CanonicalJson.Hash(CanonicalJson.Serialize(new
        {
            engineVersion = EngineVersion,
            intentHash,
            packages = orderedPackages,
        }));
        return new VerifiedPackageGraph
        {
            Project = project,
            DistributionRoot = Normalize(distributionRoot),
            Lock = new SagaPackageLock
            {
                ProjectId = project.Manifest?.ProjectId ?? "",
                ProjectHash = project.ProjectHash,
                IntentHash = intentHash,
                GraphHash = graphHash,
                Packages = orderedPackages,
            },
            RuntimeBridge = runtimeBridge,
            Diagnostics = Sort(diagnostics),
        };
    }

    private static VerifiedRuntimeBridge ReadRuntimeBridge(string runtimeBridgePath,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(runtimeBridgePath)) return new();
        try
        {
            var fullPath = Path.GetFullPath(runtimeBridgePath);
            var bytes = File.ReadAllBytes(fullPath);
            if (bytes.Length == 0)
                throw new InvalidOperationException("The resolved RuntimeBridge assembly is empty.");
            return new VerifiedRuntimeBridge
            {
                SourcePath = fullPath,
                Bytes = bytes,
                Hash = CanonicalJson.Hash(bytes),
            };
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            diagnostics.Add(Error("Package.Resolve.RuntimeBridgeInvalid", runtimeBridgePath, exception.Message));
            return new();
        }
    }

    private static void ValidateSelectedGeneration(string generatedOutputRoot, string trackedLockPath,
        SagaPackageLock? trackedLock, VerifiedPackageGraph graph,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        var root = Path.GetFullPath(generatedOutputRoot);
        var currentPath = Path.Combine(root, "manifests", "current.json");
        if (!File.Exists(currentPath))
        {
            diagnostics.Add(Error("Package.Generation.CurrentMissing", currentPath,
                "Selected package generation current.json is missing."));
            return;
        }

        GenerationCurrent? current;
        try
        {
            current = JsonSerializer.Deserialize<GenerationCurrent>(File.ReadAllBytes(currentPath), Options);
        }
        catch (JsonException exception)
        {
            diagnostics.Add(Error("Package.Generation.CurrentInvalid", currentPath, exception.Message));
            return;
        }
        if (current is null || current.SchemaVersion != 1 ||
            !IsHash(current.GenerationHash) || !IsHash(current.GraphHash) ||
            !IsHash(current.ProjectHash) || !IsHash(current.GenerationManifestHash))
        {
            diagnostics.Add(Error("Package.Generation.CurrentInvalid", currentPath,
                "current.json requires schemaVersion 1 and lowercase SHA-256 hash fields."));
            return;
        }
        if (current.GenerationId != current.GenerationHash["sha256:".Length..])
            diagnostics.Add(Error("Package.Generation.IdentityMismatch", currentPath,
                "generationId must equal the selected generationHash payload."));
        if (current.GraphHash != graph.Lock.GraphHash || current.ProjectId != graph.Lock.ProjectId ||
            current.ProjectHash != graph.Lock.ProjectHash)
            diagnostics.Add(Error("Package.Generation.IdentityMismatch", currentPath,
                "Selected generation graph or project identity does not match the verified project."));

        var expectedManifestPath = $"generations/{current.GenerationId}/generation.json";
        if (Normalize(current.GenerationManifestPath) != expectedManifestPath ||
            !TryResolveContained(root, current.GenerationManifestPath, out var manifestPath))
        {
            diagnostics.Add(Error("Package.Generation.PathUnsafe", current.GenerationManifestPath,
                "Selected generation manifest path must stay inside its immutable generation."));
            return;
        }
        if (!File.Exists(manifestPath))
        {
            diagnostics.Add(Error("Package.Generation.ManifestMissing", manifestPath,
                "Selected immutable generation manifest is missing."));
            return;
        }
        var manifestBytes = File.ReadAllBytes(manifestPath);
        if (CanonicalJson.Hash(manifestBytes) != current.GenerationManifestHash)
            diagnostics.Add(Error("Package.Generation.ManifestHashMismatch", manifestPath,
                "Selected generation manifest bytes do not match current.json."));

        GenerationManifest? manifest;
        try
        {
            manifest = JsonSerializer.Deserialize<GenerationManifest>(manifestBytes, Options);
        }
        catch (JsonException exception)
        {
            diagnostics.Add(Error("Package.Generation.ManifestInvalid", manifestPath, exception.Message));
            return;
        }
        if (manifest is null || manifest.SchemaVersion != 1 ||
            manifest.GenerationId != current.GenerationId || manifest.GenerationHash != current.GenerationHash ||
            manifest.GraphHash != current.GraphHash || manifest.ProjectId != current.ProjectId ||
            manifest.ProjectHash != current.ProjectHash)
        {
            diagnostics.Add(Error("Package.Generation.IdentityMismatch", manifestPath,
                "Generation manifest identity must match current.json."));
            return;
        }
        var orderedEntries = manifest.Files.OrderBy(item => item.Path, StringComparer.Ordinal).ToArray();
        var computedGenerationHash = CanonicalJson.Hash(CanonicalJson.Serialize(new
        {
            graphHash = manifest.GraphHash,
            files = orderedEntries.Select(item => new { path = item.Path, hash = item.Hash }).ToArray(),
        }));
        if (computedGenerationHash != manifest.GenerationHash ||
            manifest.GenerationId != computedGenerationHash["sha256:".Length..])
            diagnostics.Add(Error("Package.Generation.IdentityMismatch", manifestPath,
                "Generation ID/hash does not match its graph and file set."));

        var generationRoot = Path.GetDirectoryName(manifestPath)!;
        var entries = new Dictionary<string, GenerationFile>(StringComparer.Ordinal);
        foreach (var entry in orderedEntries)
        {
            if (!entries.TryAdd(entry.Path, entry))
            {
                diagnostics.Add(Error("Package.Generation.DuplicateFile", entry.Path,
                    "Generation file paths must be unique."));
                continue;
            }
            if (!IsHash(entry.Hash) || !TryResolveContained(generationRoot, entry.Path, out var filePath))
            {
                diagnostics.Add(Error("Package.Generation.FileInvalid", entry.Path,
                    "Generation file path or hash is invalid."));
                continue;
            }
            if (!File.Exists(filePath) || CanonicalJson.HashFile(filePath) != entry.Hash)
                diagnostics.Add(Error("Package.Generation.FileHashMismatch", entry.Path,
                    "Generation file bytes do not match the immutable manifest."));
        }

        foreach (var required in new[]
                 {
                     "SagaPackage.lock", "csharp_references_v1.json", "api_surfaces_v1.json",
                     "visual_blocks_surfaces_v1.json",
                 })
            if (!entries.ContainsKey(required))
                diagnostics.Add(Error("Package.Generation.FileMissing", required,
                    "Selected package generation is incomplete."));

        if (trackedLock is null || !entries.ContainsKey("SagaPackage.lock")) return;
        var generationLockPath = Path.Combine(generationRoot, "SagaPackage.lock");
        if (!File.Exists(generationLockPath)) return;
        var trackedBytes = File.ReadAllBytes(trackedLockPath);
        var generationLockBytes = File.ReadAllBytes(generationLockPath);
        if (!CanonicalJsonEquivalent(trackedBytes, generationLockBytes))
            diagnostics.Add(Error("Package.Generation.TrackedLockMismatch", generationLockPath,
                "Tracked and selected-generation SagaPackage.lock bytes are not canonically identical."));

        SagaPackageLock? generationLock = null;
        try
        {
            generationLock = JsonSerializer.Deserialize<SagaPackageLock>(generationLockBytes, Options);
        }
        catch (JsonException exception)
        {
            diagnostics.Add(Error("Package.Generation.LockInvalid", generationLockPath, exception.Message));
        }
        if (generationLock is null) return;
        if (generationLock.GraphHash != trackedLock.GraphHash || generationLock.ProjectId != trackedLock.ProjectId ||
            generationLock.ProjectHash != trackedLock.ProjectHash)
            diagnostics.Add(Error("Package.Generation.TrackedLockMismatch", generationLockPath,
                "Tracked and generation lock graph/project identities differ."));

        var trackedBridge = trackedLock.Packages.SelectMany(item => item.Artifacts)
            .SingleOrDefault(item => item.Kind == "managedReference");
        var generationBridge = generationLock.Packages.SelectMany(item => item.Artifacts)
            .SingleOrDefault(item => item.Kind == "managedReference");
        if (trackedBridge is null || generationBridge is null ||
            trackedBridge.ResolvedAssemblyHash != generationBridge.ResolvedAssemblyHash ||
            trackedBridge.ResolvedAssemblyLogicalPath != generationBridge.ResolvedAssemblyLogicalPath)
        {
            diagnostics.Add(Error("Package.Generation.RuntimeBridgeMismatch", generationLockPath,
                "Tracked and generation lock RuntimeBridge identities differ."));
            return;
        }
        if (!entries.TryGetValue(trackedBridge.ResolvedAssemblyLogicalPath, out var bridgeEntry) ||
            bridgeEntry.Hash != trackedBridge.ResolvedAssemblyHash)
            diagnostics.Add(Error("Package.Generation.RuntimeBridgeMismatch", trackedBridge.ResolvedAssemblyLogicalPath,
                "Selected generation RuntimeBridge hash does not match the tracked lock."));

        var referencePath = Path.Combine(generationRoot, "csharp_references_v1.json");
        try
        {
            using var references = JsonDocument.Parse(File.ReadAllBytes(referencePath));
            var reference = references.RootElement.GetProperty("references").EnumerateArray()
                .Single(item => item.GetProperty("artifactId").GetString() == trackedBridge.ArtifactId);
            if (reference.GetProperty("resolvedAssemblyHash").GetString() != trackedBridge.ResolvedAssemblyHash ||
                reference.GetProperty("resolvedAssemblyLogicalPath").GetString() != trackedBridge.ResolvedAssemblyLogicalPath)
                diagnostics.Add(Error("Package.Generation.RuntimeBridgeMismatch", referencePath,
                    "C# reference manifest RuntimeBridge identity differs from the tracked lock."));
        }
        catch (Exception exception) when (exception is IOException or JsonException or InvalidOperationException or KeyNotFoundException)
        {
            diagnostics.Add(Error("Package.Generation.ReferenceManifestInvalid", referencePath, exception.Message));
        }
    }

    private static bool CanonicalJsonEquivalent(byte[] left, byte[] right)
    {
        try
        {
            var leftNode = JsonNode.Parse(left);
            var rightNode = JsonNode.Parse(right);
            return leftNode is not null && rightNode is not null &&
                   CanonicalJson.Serialize(leftNode).SequenceEqual(CanonicalJson.Serialize(rightNode));
        }
        catch (JsonException)
        {
            return false;
        }
    }

    private static bool TryResolveContained(string root, string relative, out string fullPath)
    {
        fullPath = "";
        if (!ProjectLoader.IsProjectRelative(Normalize(relative))) return false;
        var fullRoot = Path.GetFullPath(root);
        fullPath = Path.GetFullPath(Path.Combine(fullRoot, relative));
        var prefix = fullRoot.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        return fullPath.StartsWith(prefix, StringComparison.Ordinal);
    }

    private static bool IsHash(string value) => Regex.IsMatch(value, "^sha256:[0-9a-f]{64}$");

    private static void ResolveDependency(ProjectDependency intent, string distributionRoot,
        VerifiedRuntimeBridge runtimeBridge,
        List<LockedPackage> packages, List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (!Regex.IsMatch(intent.PackageId, "^[a-z0-9]+(?:[.-][a-z0-9]+)*$"))
        {
            diagnostics.Add(Error("Package.Resolve.PackageIdInvalid", intent.PackageId,
                "Package id is not a safe built-in identity."));
            return;
        }
        if (intent.Version != EngineVersion)
        {
            diagnostics.Add(Error("Package.Resolve.VersionMismatch", intent.PackageId,
                "Built-in packages require exact engine version 0.0.11."));
            return;
        }
        var manifestPath = Path.Combine(distributionRoot, "Packages", intent.PackageId, intent.Version, "package.json");
        if (!File.Exists(manifestPath))
        {
            diagnostics.Add(Error("Package.Resolve.BuiltinNotFound", intent.PackageId,
                "Requested built-in package is not present in the engine distribution."));
            return;
        }
        BuiltinPackageManifest? manifest;
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllBytes(manifestPath));
            RejectUnknown(document.RootElement, manifestPath,
                ["schemaVersion", "packageId", "version", "trust", "license", "origin", "modules", "artifacts"], diagnostics);
            manifest = document.RootElement.Deserialize<BuiltinPackageManifest>(Options);
        }
        catch (JsonException exception)
        {
            diagnostics.Add(Error("Package.Resolve.ManifestParseFailed", manifestPath, exception.Message));
            return;
        }
        if (manifest is null || manifest.SchemaVersion != 1 || manifest.PackageId != intent.PackageId ||
            manifest.Version != intent.Version)
        {
            diagnostics.Add(Error("Package.Resolve.ManifestIdentityMismatch", manifestPath,
                "Built-in package manifest schema or identity does not match intent."));
            return;
        }
        if (manifest.Trust != "TrustedEngineDistribution")
        {
            diagnostics.Add(Error("Package.Resolve.TrustRejected", manifestPath,
                "Built-in executable artifacts require TrustedEngineDistribution."));
        }
        if (string.IsNullOrWhiteSpace(manifest.License) || string.IsNullOrWhiteSpace(manifest.Origin))
        {
            diagnostics.Add(Error("Package.Resolve.ProvenanceGap", manifestPath,
                "Built-in package needs license and origin provenance."));
        }
        if (manifest.Modules.Distinct(StringComparer.Ordinal).Count() != manifest.Modules.Count)
        {
            diagnostics.Add(Error("Package.Resolve.DuplicateModule", manifestPath,
                "Built-in package module identities must be unique."));
        }
        foreach (var module in intent.Modules.Where(module => !manifest.Modules.Contains(module, StringComparer.Ordinal)))
        {
            diagnostics.Add(Error("Package.Resolve.ModuleUnsupported", module,
                "Requested module is not supplied by the built-in package."));
        }
        var artifacts = new List<LockedArtifact>();
        foreach (var artifact in manifest.Artifacts.OrderBy(item => item.ArtifactId, StringComparer.Ordinal))
        {
            if (!string.IsNullOrWhiteSpace(artifact.Module) &&
                !intent.Modules.Contains(artifact.Module, StringComparer.Ordinal))
            {
                continue;
            }
            if (string.IsNullOrWhiteSpace(artifact.ArtifactId) ||
                artifacts.Any(item => item.ArtifactId == artifact.ArtifactId))
            {
                diagnostics.Add(Error("Package.Resolve.ArtifactIdentityInvalid", artifact.ArtifactId,
                    "Built-in package artifact identities must be non-empty and unique."));
                continue;
            }
            if (artifact.Kind is not ("managedReference" or "analyzer" or "apiSurface"))
            {
                diagnostics.Add(Error("Package.Resolve.ArtifactKindUnsupported", artifact.ArtifactId,
                    "Built-in package artifact kind is unsupported."));
                continue;
            }
            if (!ProjectLoader.IsProjectRelative(artifact.Path))
            {
                diagnostics.Add(Error("Package.Resolve.ArtifactPathUnsafe", artifact.Path,
                    "Artifact path must be distribution-relative."));
                continue;
            }
            var fullPath = Path.GetFullPath(Path.Combine(distributionRoot, artifact.Path));
            var prefix = distributionRoot.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            if (!fullPath.StartsWith(prefix, StringComparison.Ordinal) ||
                (artifact.Kind is "managedReference" or "analyzer" && manifest.Trust != "TrustedEngineDistribution"))
            {
                diagnostics.Add(Error("Package.Resolve.ExecutableTrustRejected", artifact.ArtifactId,
                    "Executable reference/analyzer resolution is restricted to the trusted engine distribution."));
                continue;
            }
            if (!File.Exists(fullPath))
            {
                diagnostics.Add(Error("Package.Resolve.ArtifactMissing", artifact.ArtifactId,
                    "Built-in package artifact is missing."));
                continue;
            }
            if (artifact.Kind == "apiSurface")
            {
                try
                {
                    using var descriptor = JsonDocument.Parse(File.ReadAllBytes(fullPath));
                    var root = descriptor.RootElement;
                    var module = root.GetProperty("module").GetString() ?? "";
                    var surfaceId = root.GetProperty("surfaceId").GetString() ?? "";
                    if (root.GetProperty("schemaVersion").GetInt32() != 1 ||
                        string.IsNullOrWhiteSpace(surfaceId) ||
                        !manifest.Modules.Contains(module, StringComparer.Ordinal) ||
                        (!string.IsNullOrWhiteSpace(artifact.Module) && artifact.Module != module))
                    {
                        throw new InvalidOperationException(
                            "API surface schema, identity, or module is invalid.");
                    }
                    if (root.TryGetProperty("runtimeReadiness", out var readiness) &&
                        readiness.GetString() is not ("RuntimeBacked" or "ContractOnly"))
                    {
                        throw new InvalidOperationException(
                            "API surface runtimeReadiness must be RuntimeBacked or ContractOnly.");
                    }
                    var identities = new HashSet<string>(StringComparer.Ordinal);
                    foreach (var api in root.GetProperty("apis").EnumerateArray())
                    {
                        var apiId = api.GetProperty("apiId").GetString() ?? "";
                        if (string.IsNullOrWhiteSpace(apiId) || !identities.Add(apiId))
                        {
                            throw new InvalidOperationException(
                                "API surface API identities must be non-empty and unique.");
                        }
                    }
                }
                catch (Exception exception) when (
                    exception is JsonException or InvalidOperationException or KeyNotFoundException)
                {
                    diagnostics.Add(Error(
                        "Package.Resolve.ApiSurfaceInvalid",
                        artifact.ArtifactId,
                        exception.Message));
                    continue;
                }
            }
            var resolvedAssemblyPath = "";
            var resolvedAssemblyHash = "";
            var targetFramework = "";
            var runtimeCompatibility = "";
            if (artifact.Kind == "managedReference")
            {
                try
                {
                    using var descriptor = JsonDocument.Parse(File.ReadAllBytes(fullPath));
                    var descriptorRoot = descriptor.RootElement;
                    targetFramework = descriptorRoot.GetProperty("targetFramework").GetString() ?? "";
                    runtimeCompatibility = descriptorRoot.GetProperty("runtimeCompatibility").GetString() ?? "";
                    var assemblyName = descriptorRoot.GetProperty("assemblyName").GetString() ?? "";
                    if (targetFramework != "net10.0" || runtimeCompatibility != EngineVersion ||
                        assemblyName != "SagaScript.RuntimeBridge")
                        throw new InvalidOperationException("Managed reference framework, runtime, or assembly identity is unsupported.");
                    if (runtimeBridge.Bytes.Length == 0 || string.IsNullOrWhiteSpace(runtimeBridge.Hash))
                        throw new InvalidOperationException("The resolved runtime bridge assembly is missing.");
                    resolvedAssemblyPath = $"references/{artifact.ArtifactId}/{assemblyName}.dll";
                    resolvedAssemblyHash = runtimeBridge.Hash;
                }
                catch (Exception exception) when (exception is JsonException or InvalidOperationException or KeyNotFoundException)
                {
                    diagnostics.Add(Error("Package.Resolve.ManagedReferenceInvalid", artifact.ArtifactId, exception.Message));
                    continue;
                }
            }
            artifacts.Add(new LockedArtifact
            {
                ArtifactId = artifact.ArtifactId,
                Kind = artifact.Kind,
                DistributionPath = Normalize(artifact.Path),
                ArtifactHash = CanonicalJson.HashFile(fullPath),
                ResolvedAssemblyLogicalPath = resolvedAssemblyPath,
                ResolvedAssemblyHash = resolvedAssemblyHash,
                TargetFramework = targetFramework,
                RuntimeCompatibility = runtimeCompatibility,
                Trust = manifest.Trust,
            });
        }
        packages.Add(new LockedPackage
        {
            PackageId = manifest.PackageId,
            Version = manifest.Version,
            SourceIdentity = $"builtin://{manifest.PackageId}@{manifest.Version}",
            SourceHash = CanonicalJson.HashFile(manifestPath),
            Trust = manifest.Trust,
            License = manifest.License,
            Origin = manifest.Origin,
            Modules = intent.Modules.OrderBy(item => item, StringComparer.Ordinal).ToArray(),
            Artifacts = artifacts.ToArray(),
        });
    }

    private static void PublishGeneration(VerifiedPackageGraph graph, string lockPath, string outputRoot,
        string runtimeBridgePath)
    {
        if (string.IsNullOrWhiteSpace(outputRoot))
            throw new InvalidOperationException("--generated-out is required for package resolution.");
        var root = Path.GetFullPath(outputRoot);
        Directory.CreateDirectory(root);
        WaitForRuntimeBridgePublicationBarrier();
        VerifyRuntimeBridgePublicationInput(graph, runtimeBridgePath);
        var staging = Path.Combine(Path.GetDirectoryName(root)!, $".{Path.GetFileName(root)}.saga-staging-{Guid.NewGuid():N}");
        var artifacts = graph.Lock.Packages.SelectMany(package => package.Artifacts).ToArray();
        var common = new
        {
            schemaVersion = 1,
            projectId = graph.Lock.ProjectId,
            projectHash = graph.Lock.ProjectHash,
            graphHash = graph.Lock.GraphHash,
        };
        var files = new Dictionary<string, byte[]>(StringComparer.Ordinal)
        {
            ["SagaPackage.lock"] = CanonicalJson.Serialize(graph.Lock),
            ["csharp_references_v1.json"] = CanonicalJson.Serialize(new
        {
            common.schemaVersion,
            common.projectId,
            common.projectHash,
            common.graphHash,
            references = artifacts.Where(item => item.Kind == "managedReference")
                .OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToArray(),
        }),
        };
        var surfaces = artifacts.Where(item => item.Kind == "apiSurface")
            .OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToArray();
        files["api_surfaces_v1.json"] = CanonicalJson.Serialize(new
        {
            common.schemaVersion,
            common.projectId,
            common.projectHash,
            common.graphHash,
            surfaces,
        });
        files["visual_blocks_surfaces_v1.json"] = CanonicalJson.Serialize(new
        {
            common.schemaVersion,
            common.projectId,
            common.projectHash,
            common.graphHash,
            surfaces,
            projectionPolicy = "FilterRawAndInternal",
        });
        var bridge = artifacts.Single(item => item.Kind == "managedReference");
        files[bridge.ResolvedAssemblyLogicalPath] = graph.RuntimeBridge.Bytes;
        var entries = files.OrderBy(item => item.Key, StringComparer.Ordinal).Select(item => new
        {
            path = item.Key,
            hash = CanonicalJson.Hash(item.Value),
        }).ToArray();
        var generationHash = CanonicalJson.Hash(CanonicalJson.Serialize(new
        {
            graphHash = graph.Lock.GraphHash,
            files = entries,
        }));
        var generationId = generationHash["sha256:".Length..];
        var generationManifest = CanonicalJson.Serialize(new
        {
            schemaVersion = 1,
            generationId,
            generationHash,
            graphHash = graph.Lock.GraphHash,
            projectId = graph.Lock.ProjectId,
            projectHash = graph.Lock.ProjectHash,
            files = entries,
        });
        files["generation.json"] = generationManifest;
        byte[]? priorLock = File.Exists(lockPath) ? File.ReadAllBytes(lockPath) : null;
        var currentPath = Path.Combine(root, "manifests", "current.json");
        byte[]? priorCurrent = File.Exists(currentPath) ? File.ReadAllBytes(currentPath) : null;
        try
        {
            Directory.CreateDirectory(staging);
            var writeIndex = 0;
            foreach (var file in files.OrderBy(item => item.Key, StringComparer.Ordinal))
            {
                var target = Path.Combine(staging, file.Key);
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                File.WriteAllBytes(target, file.Value);
                if (CanonicalJson.HashFile(target) != CanonicalJson.Hash(file.Value))
                    throw new InvalidOperationException("Staged package generation hash verification failed.");
                InjectFailure($"after-write-{++writeIndex}");
            }
            VerifyRuntimeBridgePublicationInput(graph, runtimeBridgePath);
            InjectFailure("before-generation-publish");
            var finalGeneration = Path.Combine(root, "generations", generationId);
            Directory.CreateDirectory(Path.GetDirectoryName(finalGeneration)!);
            if (!Directory.Exists(finalGeneration)) Directory.Move(staging, finalGeneration);
            else Directory.Delete(staging, recursive: true);
            AtomicWrite(lockPath, files["SagaPackage.lock"]);
            InjectFailure("before-current-publish");
            var current = CanonicalJson.Serialize(new
            {
                schemaVersion = 1,
                generationId,
                generationHash,
                graphHash = graph.Lock.GraphHash,
                projectId = graph.Lock.ProjectId,
                projectHash = graph.Lock.ProjectHash,
                generationManifestPath = $"generations/{generationId}/generation.json",
                generationManifestHash = CanonicalJson.Hash(generationManifest),
            });
            AtomicWrite(currentPath, current);
        }
        catch
        {
            Restore(lockPath, priorLock);
            Restore(currentPath, priorCurrent);
            throw;
        }
        finally
        {
            if (Directory.Exists(staging)) Directory.Delete(staging, recursive: true);
        }
    }

    private static void VerifyRuntimeBridgePublicationInput(VerifiedPackageGraph graph, string runtimeBridgePath)
    {
        var lockedBridge = graph.Lock.Packages.SelectMany(item => item.Artifacts)
            .Single(item => item.Kind == "managedReference");
        if (graph.RuntimeBridge.Bytes.Length == 0 ||
            CanonicalJson.Hash(graph.RuntimeBridge.Bytes) != lockedBridge.ResolvedAssemblyHash)
            throw new PackageResolutionException("Package.Resolve.RuntimeBridgeDependencyMismatch",
                runtimeBridgePath, "Verified RuntimeBridge bytes do not match the resolved graph.");
        string currentHash;
        try
        {
            currentHash = CanonicalJson.HashFile(runtimeBridgePath);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            throw new PackageResolutionException("Package.Resolve.RuntimeBridgeStale",
                runtimeBridgePath, exception.Message);
        }
        if (currentHash != graph.RuntimeBridge.Hash)
            throw new PackageResolutionException("Package.Resolve.RuntimeBridgeStale", runtimeBridgePath,
                "RuntimeBridge source changed after graph verification; no generation was published.");
    }

    private static void WaitForRuntimeBridgePublicationBarrier()
    {
        var barrier = Environment.GetEnvironmentVariable("SAGAPACK_RUNTIME_BRIDGE_BARRIER");
        if (string.IsNullOrWhiteSpace(barrier)) return;
        File.WriteAllText(barrier + ".reached", "reached\n");
        var deadline = DateTime.UtcNow.AddSeconds(30);
        while (!File.Exists(barrier + ".release"))
        {
            if (DateTime.UtcNow >= deadline)
                throw new PackageResolutionException("Package.Resolve.PublicationBarrierTimeout", barrier,
                    "RuntimeBridge publication test barrier timed out.");
            Thread.Sleep(10);
        }
    }

    private static void InjectFailure(string stage)
    {
        if (Environment.GetEnvironmentVariable("SAGAPACK_FAIL_STAGE") == stage)
            throw new InvalidOperationException($"Injected package publication failure at {stage}.");
    }

    private static void Restore(string path, byte[]? bytes)
    {
        if (bytes is not null) AtomicWrite(path, bytes);
        else if (File.Exists(path)) File.Delete(path);
    }

    private static PackageResolutionReport Report(string command, VerifiedPackageGraph graph,
        string lockPath, string generatedRoot, IEnumerable<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        var sorted = Sort(diagnostics);
        return new PackageResolutionReport
        {
            Command = command,
            Status = graph.Passed && sorted.All(item => item.Severity != "Error") ? "Passed" : "Failed",
            ProjectId = graph.Lock.ProjectId,
            ProjectHash = graph.Lock.ProjectHash,
            IntentHash = graph.Lock.IntentHash,
            GraphHash = graph.Lock.GraphHash,
            LockPath = Normalize(lockPath),
            GeneratedOutputRoot = Normalize(generatedRoot),
            Diagnostics = sorted,
        };
    }

    private static void AtomicWrite(string path, byte[] bytes)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        var temporary = full + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllBytes(temporary, bytes);
            File.Move(temporary, full, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    private static string ResolveEngineRoot(string input)
    {
        if (!string.IsNullOrWhiteSpace(input))
            return Path.GetFullPath(input);
        var cursor = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (cursor is not null)
        {
            if (Directory.Exists(Path.Combine(cursor.FullName, "Engine", "Distribution")))
                return cursor.FullName;
            cursor = cursor.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private static void RejectUnknown(JsonElement value, string path, IReadOnlyList<string> allowed,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (value.ValueKind != JsonValueKind.Object)
            return;
        foreach (var property in value.EnumerateObject().Where(item => !allowed.Contains(item.Name, StringComparer.Ordinal)))
            diagnostics.Add(Error("Package.Resolve.UnknownField", path + "#" + property.Name,
                "Unknown package manifest field."));
    }

    private static List<SagaProjectKit.Core.ProjectDiagnostic> Sort(
        IEnumerable<SagaProjectKit.Core.ProjectDiagnostic> diagnostics) => diagnostics
        .OrderByDescending(item => item.Severity == "Error")
        .ThenBy(item => item.Code, StringComparer.Ordinal)
        .ThenBy(item => item.Path, StringComparer.Ordinal)
        .ThenBy(item => item.Message, StringComparer.Ordinal).ToList();

    private static SagaProjectKit.Core.ProjectDiagnostic Error(string code, string path, string message) => new()
    {
        Code = code,
        Path = Normalize(path),
        Message = message,
    };

    private static string Normalize(string value) => value.Replace('\\', '/');
}
