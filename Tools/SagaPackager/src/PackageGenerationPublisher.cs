// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using SagaProjectKit.Core;

namespace SagaPackager;

internal static class PackageGenerationPublisher
{
    public static void Publish(VerifiedPackageGraph graph, string lockPath,
        string outputRoot, string runtimeBridgePath, bool publishLock)
    {
        if (string.IsNullOrWhiteSpace(outputRoot))
            throw new InvalidOperationException(
                "--generated-out is required for package resolution.");
        var root = Path.GetFullPath(outputRoot);
        Directory.CreateDirectory(root);
        WaitForRuntimeBridgePublicationBarrier();
        VerifyRuntimeBridgePublicationInput(graph, runtimeBridgePath);
        var staging = Path.Combine(Path.GetDirectoryName(root)!,
            $".{Path.GetFileName(root)}.saga-staging-{Guid.NewGuid():N}");
        var artifacts = graph.Lock.Packages.SelectMany(package => package.Artifacts).ToArray();
        var common = new { schemaVersion = 1, projectId = graph.Lock.ProjectId,
            projectHash = graph.Lock.ProjectHash, graphHash = graph.Lock.GraphHash };
        var files = new Dictionary<string, byte[]>(StringComparer.Ordinal)
        {
            ["SagaPackage.lock"] = CanonicalJson.Serialize(graph.Lock),
            ["csharp_references_v1.json"] = CanonicalJson.Serialize(new
            {
                common.schemaVersion, common.projectId, common.projectHash,
                common.graphHash,
                references = artifacts.Where(item => item.Kind == "managedReference")
                    .OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToArray(),
            }),
        };
        var surfaces = artifacts.Where(item => item.Kind == "apiSurface")
            .OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToArray();
        files["api_surfaces_v1.json"] = CanonicalJson.Serialize(new
        {
            common.schemaVersion, common.projectId, common.projectHash,
            common.graphHash, surfaces,
        });
        files["visual_blocks_surfaces_v1.json"] = CanonicalJson.Serialize(new
        {
            common.schemaVersion, common.projectId, common.projectHash,
            common.graphHash, surfaces, projectionPolicy = "FilterRawAndInternal",
        });
        files["api_ownership_v1.json"] = BuildApiOwnershipCatalog(graph);
        var bridge = artifacts.Single(item => item.Kind == "managedReference");
        files[bridge.ResolvedAssemblyLogicalPath] = graph.RuntimeBridge.Bytes;
        var entries = files.OrderBy(item => item.Key, StringComparer.Ordinal)
            .Select(item => new { path = item.Key,
                hash = CanonicalJson.Hash(item.Value) }).ToArray();
        var generationHash = CanonicalJson.Hash(CanonicalJson.Serialize(new
            { graphHash = graph.Lock.GraphHash, files = entries }));
        var generationId = generationHash["sha256:".Length..];
        var generationManifest = CanonicalJson.Serialize(new
        {
            schemaVersion = 1, generationId, generationHash,
            graphHash = graph.Lock.GraphHash, projectId = graph.Lock.ProjectId,
            projectHash = graph.Lock.ProjectHash, files = entries,
        });
        files["generation.json"] = generationManifest;
        byte[]? priorLock = publishLock && File.Exists(lockPath)
            ? File.ReadAllBytes(lockPath) : null;
        var currentPath = Path.Combine(root, "manifests", "current.json");
        byte[]? priorCurrent = File.Exists(currentPath)
            ? File.ReadAllBytes(currentPath) : null;
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
                    throw new InvalidOperationException(
                        "Staged package generation hash verification failed.");
                InjectFailure($"after-write-{++writeIndex}");
            }
            VerifyRuntimeBridgePublicationInput(graph, runtimeBridgePath);
            InjectFailure("before-generation-publish");
            var finalGeneration = Path.Combine(root, "generations", generationId);
            Directory.CreateDirectory(Path.GetDirectoryName(finalGeneration)!);
            if (!Directory.Exists(finalGeneration)) Directory.Move(staging, finalGeneration);
            else Directory.Delete(staging, recursive: true);
            if (publishLock) AtomicWrite(lockPath, files["SagaPackage.lock"]);
            InjectFailure("before-current-publish");
            AtomicWrite(currentPath, CanonicalJson.Serialize(new
            {
                schemaVersion = 1, generationId, generationHash,
                graphHash = graph.Lock.GraphHash, projectId = graph.Lock.ProjectId,
                projectHash = graph.Lock.ProjectHash,
                generationManifestPath = $"generations/{generationId}/generation.json",
                generationManifestHash = CanonicalJson.Hash(generationManifest),
            }));
        }
        catch
        {
            if (publishLock) Restore(lockPath, priorLock);
            Restore(currentPath, priorCurrent);
            throw;
        }
        finally
        {
            if (Directory.Exists(staging)) Directory.Delete(staging, recursive: true);
        }
    }

    private static byte[] BuildApiOwnershipCatalog(VerifiedPackageGraph graph)
    {
        var entries = new List<object>();
        foreach (var package in graph.Lock.Packages
                     .OrderBy(item => item.PackageId, StringComparer.Ordinal))
        {
            var manifestPath = Path.Combine(graph.DistributionRoot, "Packages",
                package.PackageId, package.Version, "package.json");
            var manifest = JsonSerializer.Deserialize<BuiltinPackageManifest>(
                File.ReadAllBytes(manifestPath), CanonicalJson.SerializerOptions)
                ?? throw new PackageResolutionException(
                    "Package.Ownership.ManifestInvalid", manifestPath,
                    "Package manifest could not be loaded for API ownership.");
            foreach (var artifact in manifest.Artifacts
                         .Where(item => item.Kind == "apiSurface")
                         .OrderBy(item => item.ArtifactId, StringComparer.Ordinal))
            {
                var descriptorPath = Path.Combine(graph.DistributionRoot, artifact.Path);
                using var descriptor = JsonDocument.Parse(File.ReadAllBytes(descriptorPath));
                var root = descriptor.RootElement;
                var module = root.GetProperty("module").GetString() ?? "";
                var surface = root.GetProperty("surfaceId").GetString() ?? "";
                var enabled = package.Modules.Contains(module, StringComparer.Ordinal);
                foreach (var api in root.GetProperty("apis").EnumerateArray())
                {
                    var documentationId = api.TryGetProperty("documentationId",
                        out var documentation) ? documentation.GetString() ?? "" : "";
                    if (string.IsNullOrWhiteSpace(documentationId))
                        throw new PackageResolutionException(
                            "Package.Ownership.DocumentationIdMissing", descriptorPath,
                            "Every package-owned API requires an explicit Roslyn documentation ID.");
                    entries.Add(new
                    {
                        documentationId,
                        apiId = api.GetProperty("apiId").GetString() ?? "",
                        surface, module, packageId = package.PackageId,
                        packageVersion = package.Version,
                        capability = api.GetProperty("capability").GetString() ?? "",
                        projection = api.GetProperty("blockProjection").GetString() ?? "",
                        enabled,
                    });
                }
            }
        }
        return CanonicalJson.Serialize(new
        {
            schemaVersion = 1, projectId = graph.Lock.ProjectId,
            projectHash = graph.Lock.ProjectHash, graphHash = graph.Lock.GraphHash,
            generationIdentity = graph.Lock.GraphHash, symbols = entries,
        });
    }

    private static void VerifyRuntimeBridgePublicationInput(
        VerifiedPackageGraph graph, string runtimeBridgePath)
    {
        var locked = graph.Lock.Packages.SelectMany(item => item.Artifacts)
            .Single(item => item.Kind == "managedReference");
        if (graph.RuntimeBridge.Bytes.Length == 0 ||
            CanonicalJson.Hash(graph.RuntimeBridge.Bytes) != locked.ResolvedAssemblyHash)
            throw new PackageResolutionException(
                "Package.Resolve.RuntimeBridgeDependencyMismatch", runtimeBridgePath,
                "Verified RuntimeBridge bytes do not match the resolved graph.");
        string currentHash;
        try { currentHash = CanonicalJson.HashFile(runtimeBridgePath); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            throw new PackageResolutionException("Package.Resolve.RuntimeBridgeStale",
                runtimeBridgePath, exception.Message);
        }
        if (currentHash != graph.RuntimeBridge.Hash)
            throw new PackageResolutionException("Package.Resolve.RuntimeBridgeStale",
                runtimeBridgePath,
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
                throw new PackageResolutionException(
                    "Package.Resolve.PublicationBarrierTimeout", barrier,
                    "RuntimeBridge publication test barrier timed out.");
            Thread.Sleep(10);
        }
    }

    private static void InjectFailure(string stage)
    {
        if (Environment.GetEnvironmentVariable("SAGAPACK_FAIL_STAGE") == stage)
            throw new InvalidOperationException(
                $"Injected package publication failure at {stage}.");
    }

    private static void Restore(string path, byte[]? bytes)
    {
        if (bytes is not null) AtomicWrite(path, bytes);
        else if (File.Exists(path)) File.Delete(path);
    }

    private static void AtomicWrite(string path, byte[] bytes)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        var temporary = full + $".{Guid.NewGuid():N}.tmp";
        try
        {
            File.WriteAllBytes(temporary, bytes);
            File.Move(temporary, full, overwrite: true);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
}
