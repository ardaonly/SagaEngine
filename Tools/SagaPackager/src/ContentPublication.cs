// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using System.Text.Json.Nodes;
using SagaProjectKit.Core;

namespace SagaPackager;

/// <summary>Publishes content-addressed objects before atomically replacing the selector.</summary>
internal static class ContentPublication
{
    public static ContentCompileResult Publish(ProjectLoadResult load, string outputRoot, string domain,
        IReadOnlyList<(JsonObject Document, ContentCompileArtifact Entry)> artifacts,
        IReadOnlyList<ProjectDiagnostic> diagnostics)
    {
        var root = Path.GetFullPath(outputRoot);
        var staging = Path.Combine(root, ".staging", Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(staging);
            var published = new List<ContentCompileArtifact>();
            foreach (var artifact in artifacts.OrderBy(item => item.Entry.ArtifactId, StringComparer.Ordinal))
            {
                var bytes = CanonicalJson.Serialize(artifact.Document);
                var fileHash = CanonicalJson.Hash(bytes);
                var name = fileHash[7..] + ".json";
                var staged = Path.Combine(staging, name);
                File.WriteAllBytes(staged, bytes);
                var relative = "objects/" + name;
                var destination = Path.Combine(root, "objects", name);
                Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                if (!File.Exists(destination)) File.Move(staged, destination);
                published.Add(artifact.Entry with { FileHash = fileHash, Path = relative });
            }

            var manifest = new JsonObject
            {
                ["artifacts"] = JsonSerializer.SerializeToNode(published, CanonicalJson.SerializerOptions),
                ["domain"] = domain,
                ["projectHash"] = load.ProjectHash,
                ["projectId"] = load.Manifest!.ProjectId,
                ["schemaVersion"] = 1,
                ["tool"] = "sagapack",
                ["toolVersion"] = "0.0.11",
            };
            var manifestBytes = CanonicalJson.Serialize(manifest);
            var manifestHash = CanonicalJson.Hash(manifestBytes);
            Directory.CreateDirectory(Path.Combine(root, "manifests"));
            var temporary = Path.Combine(root, "manifests", $"current.{Guid.NewGuid():N}.tmp");
            File.WriteAllBytes(temporary, manifestBytes);
            File.Move(temporary, Path.Combine(root, "manifests", "current.json"), overwrite: true);
            return new ContentCompileResult
            {
                Command = "compile-" + domain,
                Status = "Passed",
                ProjectId = load.Manifest.ProjectId,
                ProjectHash = load.ProjectHash,
                ManifestHash = manifestHash,
                Artifacts = published,
                Diagnostics = diagnostics,
            };
        }
        catch (Exception exception)
        {
            var failureDiagnostics = diagnostics.Append(new ProjectDiagnostic
            {
                Code = "Content.Publish.Failed",
                Path = outputRoot.Replace('\\', '/'),
                Message = exception.Message,
            }).OrderByDescending(item => item.Severity == "Error")
              .ThenBy(item => item.Code, StringComparer.Ordinal)
              .ThenBy(item => item.Path, StringComparer.Ordinal).ToArray();
            return new ContentCompileResult
            {
                Command = "compile-" + domain,
                ProjectId = load.Manifest?.ProjectId ?? "",
                ProjectHash = load.ProjectHash,
                Diagnostics = failureDiagnostics,
            };
        }
        finally
        {
            if (Directory.Exists(staging)) Directory.Delete(staging, recursive: true);
        }
    }
}
