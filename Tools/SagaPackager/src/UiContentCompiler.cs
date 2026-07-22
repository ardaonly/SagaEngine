// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using System.Text.Json.Nodes;
using SagaProjectKit.Core;

namespace SagaPackager;

/// <summary>Validates and atomically publishes the vendor-neutral runtime UI set.</summary>
internal static class UiContentCompiler
{
    private const string ToolVersion = "0.0.11";
    private static readonly HashSet<string> SupportedKinds =
        ["ui-config", "rml", "rcss", "font"];

    public static JsonObject? Compile(ProjectLoadResult load, string outputRoot,
        List<ProjectDiagnostic> diagnostics)
    {
        var manifest = load.Manifest!;
        if (manifest.UiDocuments.Count == 0) return null;

        foreach (var reference in manifest.UiDocuments)
        {
            if (!SupportedKinds.Contains(reference.Kind))
                diagnostics.Add(Error("UI.Source.KindUnsupported", reference.Path,
                    $"Unsupported UI source kind: {reference.Kind}."));
        }
        var configs = manifest.UiDocuments.Where(item => item.Kind == "ui-config").ToArray();
        if (configs.Length != 1)
        {
            diagnostics.Add(Error("UI.Source.ConfigCountInvalid", "uiDocuments",
                "Exactly one ui-config reference is required by runtime.ui-set v1."));
            return null;
        }

        var configPath = Path.GetFullPath(Path.Combine(load.ProjectRoot, configs[0].Path));
        JsonDocument source;
        try { source = JsonDocument.Parse(File.ReadAllBytes(configPath)); }
        catch (Exception exception) when (exception is IOException or JsonException)
        {
            diagnostics.Add(Error("UI.Source.ParseFailed", configs[0].Path, exception.Message));
            return null;
        }
        using (source)
        {
            var root = source.RootElement;
            RejectUnknown(root, configPath,
                ["schemaVersion", "screenId", "documents", "stylesheets", "fonts", "prompts"], diagnostics);
            if (!root.TryGetProperty("schemaVersion", out var schema) ||
                schema.ValueKind != JsonValueKind.Number || schema.GetInt32() != 1)
                diagnostics.Add(Error("UI.Source.SchemaVersionUnsupported", configs[0].Path,
                    "UI source schemaVersion must be 1."));
            var screenId = RequiredString(root, "screenId", configs[0].Path, diagnostics);
            if (screenId != configs[0].Id)
                diagnostics.Add(Error("UI.Source.ScreenIdMismatch", configs[0].Path,
                    "screenId must match the ui-config project reference id."));

            var documents = Resources(root, "documents", ["documentId", "path"],
                configPath, load.ProjectRoot, manifest.UiDocuments, diagnostics);
            var styles = Resources(root, "stylesheets", ["path"],
                configPath, load.ProjectRoot, manifest.UiDocuments, diagnostics);
            var fonts = Resources(root, "fonts", ["family", "path"],
                configPath, load.ProjectRoot, manifest.UiDocuments, diagnostics);
            var prompts = Prompts(root, configPath, diagnostics);
            if (documents.Count == 0 || styles.Count == 0 || fonts.Count == 0 || prompts.Count == 0)
                diagnostics.Add(Error("UI.Source.RequiredCollectionEmpty", configs[0].Path,
                    "runtime.ui-set requires at least one document, stylesheet, font, and prompt."));
            if (diagnostics.Any(item => item.Severity == "Error" &&
                    (item.Code.StartsWith("UI.", StringComparison.Ordinal) ||
                     item.Code == "Project.Reference.Missing"))) return null;

            var sourceRecords = manifest.UiDocuments.OrderBy(item => item.Id, StringComparer.Ordinal)
                .Select(reference =>
                {
                    var path = Path.Combine(load.ProjectRoot, reference.Path);
                    return new JsonObject
                    {
                        ["id"] = reference.Id,
                        ["kind"] = reference.Kind,
                        ["path"] = reference.Path.Replace('\\', '/'),
                        ["sourceHash"] = CanonicalJson.HashFile(path),
                    };
                }).ToArray();
            var aggregateDescriptor = new JsonObject
            {
                ["projectId"] = manifest.ProjectId,
                ["screenId"] = screenId,
                ["sources"] = new JsonArray(sourceRecords.Select(item => (JsonNode)item).ToArray()),
                ["tool"] = "sagapack",
                ["toolVersion"] = ToolVersion,
            };
            var aggregateHash = CanonicalJson.Hash(CanonicalJson.Serialize(aggregateDescriptor));
            var generationId = aggregateHash[7..];

            AddPublishedPaths(documents, generationId);
            AddPublishedPaths(styles, generationId);
            AddPublishedPaths(fonts, generationId);
            if (!PublishGeneration(outputRoot, generationId,
                    documents.Concat(styles).Concat(fonts).ToArray(), diagnostics)) return null;

            var payload = new JsonObject
            {
                ["aggregateManifestHash"] = aggregateHash,
                ["documents"] = new JsonArray(documents.Select(item => (JsonNode)item.Record).ToArray()),
                ["fonts"] = new JsonArray(fonts.Select(item => (JsonNode)item.Record).ToArray()),
                ["prompts"] = new JsonArray(prompts.Select(item => item.DeepClone()).ToArray()),
                ["screenId"] = screenId,
                ["stylesheets"] = new JsonArray(styles.Select(item => (JsonNode)item.Record).ToArray()),
                ["tool"] = new JsonObject { ["name"] = "sagapack", ["version"] = ToolVersion },
            };
            var contentHash = CanonicalJson.Hash(CanonicalJson.Serialize(payload));
            var artifact = new JsonObject
            {
                ["artifactId"] = screenId,
                ["artifactType"] = "runtime.ui-set",
                ["contentHash"] = contentHash,
                ["cooker"] = new JsonObject { ["name"] = "sagapack", ["version"] = ToolVersion },
                ["dependencies"] = new JsonArray(),
                ["payload"] = payload,
                ["provenance"] = new JsonObject { ["configSourceHash"] = CanonicalJson.HashFile(configPath) },
                ["schemaVersion"] = 1,
                ["sourceHash"] = aggregateHash,
            };
            PublishSelector(outputRoot, generationId, aggregateHash, contentHash, screenId);
            return artifact;
        }
    }

    private sealed record Resource(JsonObject Record, string SourcePath, string Extension, string Hash);

    private static List<Resource> Resources(JsonElement root, string property,
        IReadOnlyList<string> allowed, string configPath, string projectRoot,
        IReadOnlyList<ProjectReference> references, List<ProjectDiagnostic> diagnostics)
    {
        var result = new List<Resource>();
        if (!root.TryGetProperty(property, out var array) || array.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(Error("UI.Source.CollectionMissing", configPath + "#" + property,
                property + " must be an array."));
            return result;
        }
        var configDirectory = Path.GetDirectoryName(configPath)!;
        foreach (var item in array.EnumerateArray())
        {
            RejectUnknown(item, configPath + "#" + property, allowed, diagnostics);
            var relative = RequiredString(item, "path", configPath + "#" + property, diagnostics);
            var sourcePath = Path.GetFullPath(Path.Combine(configDirectory, relative));
            if (!IsContained(projectRoot, sourcePath) || !File.Exists(sourcePath))
            {
                diagnostics.Add(Error("UI.Source.ResourceMissing", relative,
                    "UI resource must exist below the project root."));
                continue;
            }
            var projectRelative = Path.GetRelativePath(projectRoot, sourcePath).Replace('\\', '/');
            var matchingReference = references.SingleOrDefault(reference =>
                string.Equals(reference.Path.Replace('\\', '/'), projectRelative, StringComparison.Ordinal));
            if (matchingReference is null)
            {
                diagnostics.Add(Error("UI.Source.ResourceUnreferenced", projectRelative,
                    "Every runtime UI resource must be named by uiDocuments."));
                continue;
            }
            var hash = CanonicalJson.HashFile(sourcePath);
            var record = new JsonObject
            {
                ["hash"] = hash,
                ["sourceId"] = matchingReference.Id,
            };
            foreach (var name in allowed.Where(name => name != "path"))
                record[name] = RequiredString(item, name, configPath + "#" + property, diagnostics);
            result.Add(new Resource(record, sourcePath,
                Path.GetExtension(sourcePath).ToLowerInvariant(), hash));
        }
        return result;
    }

    private static List<JsonNode> Prompts(JsonElement root, string path,
        List<ProjectDiagnostic> diagnostics)
    {
        var result = new List<JsonNode>();
        var fields = new[] {"actionId", "behaviorId", "elementId", "entityId", "falseLabel",
            "stateKey", "trueLabel", "visibility"};
        if (!root.TryGetProperty("prompts", out var prompts) || prompts.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(Error("UI.Source.PromptsMissing", path, "prompts must be an array."));
            return result;
        }
        foreach (var prompt in prompts.EnumerateArray())
        {
            RejectUnknown(prompt, path + "#prompts", fields, diagnostics);
            foreach (var field in fields) RequiredString(prompt, field, path + "#prompts", diagnostics);
            if (prompt.TryGetProperty("visibility", out var visibility) && visibility.GetString() != "Always")
                diagnostics.Add(Error("UI.Source.VisibilityUnsupported", path,
                    "Interaction prompt visibility must be Always."));
            result.Add(JsonNode.Parse(prompt.GetRawText())!);
        }
        return result;
    }

    private static void AddPublishedPaths(IEnumerable<Resource> resources, string generationId)
    {
        foreach (var resource in resources)
            resource.Record["path"] = $"ui/generations/{generationId}/objects/{resource.Hash[7..]}{resource.Extension}";
    }

    private static bool PublishGeneration(string outputRoot, string generationId,
        IReadOnlyList<Resource> resources, List<ProjectDiagnostic> diagnostics)
    {
        var uiRoot = Path.Combine(Path.GetFullPath(outputRoot), "ui");
        var destination = Path.Combine(uiRoot, "generations", generationId);
        var staging = Path.Combine(uiRoot, ".staging", Guid.NewGuid().ToString("N"));
        try
        {
            if (Directory.Exists(destination))
            {
                foreach (var resource in resources)
                {
                    var existing = Path.Combine(destination, "objects", resource.Hash[7..] + resource.Extension);
                    if (!File.Exists(existing) || CanonicalJson.HashFile(existing) != resource.Hash)
                        throw new InvalidDataException("Existing immutable UI generation does not match its resource hashes.");
                }
                return true;
            }
            Directory.CreateDirectory(Path.Combine(staging, "objects"));
            foreach (var resource in resources)
                File.Copy(resource.SourcePath,
                    Path.Combine(staging, "objects", resource.Hash[7..] + resource.Extension));
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            Directory.Move(staging, destination);
            return true;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Error("UI.Publish.GenerationFailed", outputRoot, exception.Message));
            return false;
        }
        finally
        {
            if (Directory.Exists(staging)) Directory.Delete(staging, recursive: true);
        }
    }

    private static void PublishSelector(string outputRoot, string generationId,
        string aggregateHash, string artifactHash, string screenId)
    {
        var root = Path.Combine(Path.GetFullPath(outputRoot), "ui");
        Directory.CreateDirectory(root);
        var selector = new JsonObject
        {
            ["aggregateManifestHash"] = aggregateHash,
            ["artifactContentHash"] = artifactHash,
            ["generationId"] = generationId,
            ["schemaVersion"] = 1,
            ["screenId"] = screenId,
            ["tool"] = "sagapack",
            ["toolVersion"] = ToolVersion,
        };
        var temporary = Path.Combine(root, $"current.{Guid.NewGuid():N}.tmp");
        File.WriteAllBytes(temporary, CanonicalJson.Serialize(selector));
        File.Move(temporary, Path.Combine(root, "current.json"), overwrite: true);
    }

    private static bool IsContained(string root, string path)
    {
        var normalizedRoot = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        return Path.GetFullPath(path).StartsWith(normalizedRoot, StringComparison.Ordinal);
    }

    private static string RequiredString(JsonElement value, string name, string path,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!value.TryGetProperty(name, out var property) || property.ValueKind != JsonValueKind.String ||
            string.IsNullOrWhiteSpace(property.GetString()))
        {
            diagnostics.Add(Error("UI.Source.StringRequired", path + "#" + name,
                name + " must be a non-empty string."));
            return "";
        }
        return property.GetString()!;
    }

    private static void RejectUnknown(JsonElement value, string path, IReadOnlyList<string> allowed,
        List<ProjectDiagnostic> diagnostics)
    {
        if (value.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(Error("UI.Source.ObjectRequired", path, "Expected an object."));
            return;
        }
        foreach (var property in value.EnumerateObject().Where(item => !allowed.Contains(item.Name)))
            diagnostics.Add(Error("UI.Source.UnknownField", path + "#" + property.Name,
                "Unknown field: " + property.Name + "."));
    }

    private static ProjectDiagnostic Error(string code, string path, string message) => new()
    {
        Code = code,
        Path = path.Replace('\\', '/'),
        Message = message,
    };
}
