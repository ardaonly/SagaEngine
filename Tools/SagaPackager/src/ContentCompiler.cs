// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;
using SagaProjectKit.Core;

namespace SagaPackager;

internal sealed record ContentCompileArtifact
{
    public string ArtifactId { get; init; } = "";
    public string ArtifactType { get; init; } = "";
    public string ContentHash { get; init; } = "";
    public string FileHash { get; init; } = "";
    public string Path { get; init; } = "";
    public string SourceHash { get; init; } = "";
}

internal sealed record ContentCompileResult
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string ToolVersion { get; init; } = "0.0.11";
    public string Command { get; init; } = "compile-content";
    public string Status { get; init; } = "Failed";
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string ManifestHash { get; init; } = "";
    public IReadOnlyList<ContentCompileArtifact> Artifacts { get; init; } = [];
    public IReadOnlyList<SagaProjectKit.Core.ProjectDiagnostic> Diagnostics { get; init; } = [];
}

internal static class ContentCompiler
{
    private const string ToolVersion = "0.0.11";

    public static ContentCompileResult Compile(string projectInput, string outputRoot, string domain,
        string scriptManifestPath = "")
    {
        var load = new ProjectLoader().Load(projectInput, checkReferences: true);
        var diagnostics = load.Diagnostics.ToList();
        if (!load.Passed || load.Manifest is null)
        {
            return Failure(domain, load, diagnostics);
        }
        var artifacts = new List<(JsonObject Document, ContentCompileArtifact Entry)>();
        var assetHashes = new Dictionary<string, string>(StringComparer.Ordinal);
        try
        {
            if (domain is "assets" or "scenes" or "all")
            {
                foreach (var reference in load.Manifest.Assets.OrderBy(item => item.Id, StringComparer.Ordinal))
                {
                    CompileAsset(load.ProjectRoot, reference, artifacts, assetHashes, diagnostics);
                }
            }
            if (domain is "scenes" or "all")
            {
                foreach (var reference in load.Manifest.Scenes.OrderBy(item => item.Id, StringComparer.Ordinal))
                {
                    CompileScene(load, reference, assetHashes, scriptManifestPath, artifacts, diagnostics);
                }
            }
            if (domain is "inputs" or "all")
            {
                foreach (var reference in load.Manifest.InputMaps.OrderBy(item => item.Id, StringComparer.Ordinal))
                {
                    CompileInput(load.ProjectRoot, reference, artifacts, diagnostics);
                }
            }
        }
        catch (Exception exception)
        {
            diagnostics.Add(Error("Content.Compiler.InternalFailure", projectInput, exception.Message));
        }
        diagnostics = Sort(diagnostics);
        if (diagnostics.Any(item => item.Severity == "Error"))
        {
            return Failure(domain, load, diagnostics);
        }

        return Publish(load, outputRoot, domain, artifacts, diagnostics);
    }

    private static void CompileAsset(string root, SagaProjectKit.Core.ProjectReference reference,
        List<(JsonObject Document, ContentCompileArtifact Entry)> artifacts,
        Dictionary<string, string> hashes, List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        var path = Path.Combine(root, reference.Path);
        using var source = Parse(path, "Asset.Source.ParseFailed", diagnostics);
        if (source is null)
        {
            return;
        }
        var value = source.RootElement;
        RejectUnknown(value, path, ["schemaVersion", "assetId", "kind", "geometry", "material", "provenance"], diagnostics);
        RequireSchemaOne(value, path, diagnostics);
        var assetId = String(value, "assetId", path, diagnostics);
        if (assetId != reference.Id)
        {
            diagnostics.Add(Error("Asset.Source.IdMismatch", path, "Asset source id must match its project reference id."));
        }
        if (String(value, "kind", path, diagnostics) != "primitive")
        {
            diagnostics.Add(Error("Asset.Source.KindUnsupported", path, "Only primitive asset sources are supported."));
        }
        if (!value.TryGetProperty("geometry", out var geometry) || geometry.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(Error("Asset.Source.GeometryMissing", path, "Asset geometry must be an object."));
            return;
        }
        RejectUnknown(geometry, path + "#geometry", ["shape", "dimensions"], diagnostics);
        var shape = String(geometry, "shape", path, diagnostics);
        var dimensions = NumberArray(geometry, "dimensions", path, diagnostics);
        if ((shape == "box" && dimensions.Count != 3) || (shape == "plane" && dimensions.Count != 2) ||
            (shape is not ("box" or "plane")))
        {
            diagnostics.Add(Error("Asset.Source.PrimitiveUnsupported", path,
                "Primitive geometry must be a box with three dimensions or a plane with two dimensions."));
            return;
        }
        if (dimensions.Any(number => number <= 0))
        {
            diagnostics.Add(Error("Asset.Source.DimensionInvalid", path, "Primitive dimensions must be positive."));
        }

        if (!value.TryGetProperty("material", out var material) || material.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(Error("Asset.Source.MaterialMissing", path, "Asset material must be an object."));
            return;
        }
        RejectUnknown(material, path + "#material", ["baseColor", "shaderDependency"], diagnostics);
        var color = NumberArray(material, "baseColor", path, diagnostics);
        var shader = String(material, "shaderDependency", path, diagnostics);
        if (color.Count != 4 || shader != "renderer.solid")
        {
            diagnostics.Add(Error("Asset.Source.MaterialUnsupported", path,
                "Primitive materials require RGBA baseColor and renderer.solid."));
        }

        if (!value.TryGetProperty("provenance", out var provenance) || provenance.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(Error("Asset.Source.ProvenanceMissing", path, "Asset provenance must be an object."));
            return;
        }
        RejectUnknown(provenance, path + "#provenance", ["license", "author", "origin"], diagnostics);
        var license = String(provenance, "license", path, diagnostics);
        var author = String(provenance, "author", path, diagnostics);
        var origin = String(provenance, "origin", path, diagnostics);
        if (license.Length == 0 || author.Length == 0 || origin.Length == 0)
        {
            diagnostics.Add(Error("Asset.Source.ProvenanceGap", path,
                "Asset provenance requires SPDX expression, author, and origin."));
        }
        if (diagnostics.Any(item => item.Path.StartsWith(path, StringComparison.Ordinal) && item.Severity == "Error"))
        {
            return;
        }

        var sourceHash = CanonicalJson.HashFile(path);
        var meshPayload = BuildMeshPayload(shape, dimensions);
        var meshId = assetId + ".mesh";
        var mesh = Artifact(meshId, "renderer.mesh", sourceHash, meshPayload, new JsonObject
        {
            ["author"] = author,
            ["license"] = license,
            ["origin"] = origin,
        }, ["renderer.solid"]);
        AddArtifact(mesh, artifacts, hashes);

        var materialPayload = new JsonObject
        {
            ["baseColor"] = new JsonArray(color.Select(item => JsonValue.Create(item)).ToArray()),
            ["shader"] = shader,
        };
        var materialId = assetId + ".material";
        var materialArtifact = Artifact(materialId, "renderer.material", sourceHash, materialPayload,
            new JsonObject { ["author"] = author, ["license"] = license, ["origin"] = origin }, [shader]);
        AddArtifact(materialArtifact, artifacts, hashes);
    }

    private sealed record SemanticBehavior(string BehaviorTypeId, string SourceFile, string SourceHash,
        IReadOnlySet<string> Actions, IReadOnlySet<string> Capabilities, string ScriptArtifactLogicalId);

    private static void CompileScene(ProjectLoadResult load, SagaProjectKit.Core.ProjectReference reference,
        IReadOnlyDictionary<string, string> assetHashes,
        string scriptManifestPath,
        List<(JsonObject Document, ContentCompileArtifact Entry)> artifacts,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        var root = load.ProjectRoot;
        var path = Path.Combine(root, reference.Path);
        var errorCount = diagnostics.Count(item => item.Severity == "Error");
        var semantic = LoadSemanticEvidence(scriptManifestPath, load, diagnostics);
        using var source = Parse(path, "Scene.Source.ParseFailed", diagnostics);
        if (source is null)
        {
            return;
        }
        var value = source.RootElement;
        RejectUnknown(value, path, ["schemaVersion", "sceneId", "entities"], diagnostics);
        RequireSchemaOne(value, path, diagnostics);
        var sceneId = String(value, "sceneId", path, diagnostics);
        if (sceneId != reference.Id)
        {
            diagnostics.Add(Error("Scene.Source.IdMismatch", path, "Scene id must match its project reference id."));
        }
        var entities = new List<JsonObject>();
        var entityIds = new HashSet<string>(StringComparer.Ordinal);
        var parents = new Dictionary<string, string>(StringComparer.Ordinal);
        if (!value.TryGetProperty("entities", out var entityArray) || entityArray.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(Error("Scene.Source.EntitiesMissing", path, "Scene entities must be an array."));
            return;
        }
        foreach (var entity in entityArray.EnumerateArray())
        {
            RejectUnknown(entity, path + "#entity", ["entityId", "name", "parentEntityId", "components"], diagnostics);
            var entityId = String(entity, "entityId", path, diagnostics);
            var name = String(entity, "name", path, diagnostics);
            if (!entityIds.Add(entityId))
                diagnostics.Add(Error("Scene.Entity.DuplicateId", entityId, "Scene entity identities must be unique."));
            var parentId = entity.TryGetProperty("parentEntityId", out var parentValue) && parentValue.ValueKind == JsonValueKind.String
                ? parentValue.GetString() ?? "" : "";
            if (parentValue.ValueKind is not (JsonValueKind.Undefined or JsonValueKind.String))
                diagnostics.Add(Error("Scene.Entity.ParentInvalid", entityId, "parentEntityId must be a string when present."));
            if (parentId.Length > 0) parents[entityId] = parentId;
            var components = new List<JsonObject>();
            var componentTypes = new HashSet<string>(StringComparer.Ordinal);
            if (!entity.TryGetProperty("components", out var componentArray) || componentArray.ValueKind != JsonValueKind.Array)
            {
                diagnostics.Add(Error("Scene.Source.ComponentsMissing", entityId, "Entity components must be an array."));
                continue;
            }
            foreach (var component in componentArray.EnumerateArray())
            {
                var type = String(component, "type", entityId, diagnostics);
                if (!componentTypes.Add(type))
                {
                    diagnostics.Add(Error("Scene.Component.DuplicateType", entityId + "#" + type,
                        "Singular scene component types may occur only once per entity."));
                    continue;
                }
                switch (type)
                {
                    case "Transform":
                        RejectUnknown(component, entityId + "#Transform", ["type", "position", "rotation", "scale"], diagnostics);
                        if (NumberArray(component, "position", entityId, diagnostics).Count != 3 ||
                            NumberArray(component, "rotation", entityId, diagnostics).Count != 4 ||
                            NumberArray(component, "scale", entityId, diagnostics).Count != 3)
                        {
                            diagnostics.Add(Error("Scene.Component.TransformInvalid", entityId,
                                "Transform requires position[3], rotation[4], and scale[3]."));
                        }
                        components.Add(JsonNode.Parse(component.GetRawText())!.AsObject());
                        break;
                    case "Camera":
                        RejectUnknown(component, entityId + "#Camera",
                            ["type", "projection", "primary", "verticalFovDegrees", "nearClip", "farClip"], diagnostics);
                        var projection = String(component, "projection", entityId, diagnostics);
                        var fov = Number(component, "verticalFovDegrees", entityId, diagnostics);
                        var nearClip = Number(component, "nearClip", entityId, diagnostics);
                        var farClip = Number(component, "farClip", entityId, diagnostics);
                        if (projection != "Perspective" || fov <= 0 || fov >= 180 || nearClip <= 0 || farClip <= nearClip ||
                            !component.TryGetProperty("primary", out var primary) || primary.ValueKind is not (JsonValueKind.True or JsonValueKind.False))
                            diagnostics.Add(Error("Scene.Component.CameraInvalid", entityId,
                                "Camera requires Perspective, primary, 0<FOV<180, and 0<nearClip<farClip."));
                        components.Add(JsonNode.Parse(component.GetRawText())!.AsObject());
                        break;
                    case "DirectionalLight":
                        RejectUnknown(component, entityId + "#DirectionalLight",
                            ["type", "color", "intensity", "castsShadows"], diagnostics);
                        var lightColor = NumberArray(component, "color", entityId, diagnostics);
                        var intensity = Number(component, "intensity", entityId, diagnostics);
                        if (lightColor.Count != 3 || lightColor.Any(item => item < 0) || intensity < 0 ||
                            !component.TryGetProperty("castsShadows", out var shadows) || shadows.ValueKind is not (JsonValueKind.True or JsonValueKind.False))
                            diagnostics.Add(Error("Scene.Component.DirectionalLightInvalid", entityId,
                                "DirectionalLight requires non-negative color[3], intensity, and castsShadows."));
                        components.Add(JsonNode.Parse(component.GetRawText())!.AsObject());
                        break;
                    case "MeshRenderer":
                        RejectUnknown(component, entityId + "#MeshRenderer", ["type", "meshAssetId", "materialAssetId"], diagnostics);
                        var meshId = String(component, "meshAssetId", entityId, diagnostics);
                        var materialId = String(component, "materialAssetId", entityId, diagnostics);
                        if (!assetHashes.TryGetValue(meshId, out var meshHash) ||
                            !assetHashes.TryGetValue(materialId, out var materialHash))
                        {
                            diagnostics.Add(Error("Scene.Asset.Unresolved", entityId,
                                "MeshRenderer references an asset that did not compile."));
                            break;
                        }
                        components.Add(new JsonObject
                        {
                            ["materialAssetId"] = materialId,
                            ["materialHash"] = materialHash,
                            ["meshAssetId"] = meshId,
                            ["meshHash"] = meshHash,
                            ["type"] = type,
                        });
                        break;
                    case "ScriptBehavior":
                        RejectUnknown(component, entityId + "#ScriptBehavior",
                            ["type", "behaviorTypeId", "authoringSource", "instanceConfiguration", "requiredCapabilities"], diagnostics);
                        var behaviorTypeId = String(component, "behaviorTypeId", entityId, diagnostics);
                        var authoringSource = String(component, "authoringSource", entityId, diagnostics);
                        if (!ProjectLoader.IsProjectRelative(authoringSource) || authoringSource.Contains("..", StringComparison.Ordinal) ||
                            !File.Exists(Path.Combine(root, authoringSource)))
                            diagnostics.Add(Error("Scene.Script.Missing", authoringSource, "ScriptBehavior authoring source is missing or unsafe."));
                        if (!semantic.TryGetValue(behaviorTypeId, out var behavior) || behavior.SourceFile != authoringSource ||
                            behavior.SourceHash != CanonicalJson.HashFile(Path.Combine(root, authoringSource)))
                            diagnostics.Add(Error("Scene.Script.SemanticEvidenceMismatch", behaviorTypeId,
                                "Behavior type and source hash must match verified SagaScript semantic evidence."));
                        var requiredCapabilities = StringArray(component, "requiredCapabilities", entityId, diagnostics);
                        if (behavior is not null && requiredCapabilities.Any(item => !behavior.Capabilities.Contains(item)))
                            diagnostics.Add(Error("Scene.Script.CapabilityMissing", behaviorTypeId,
                                "Required capability is absent from the verified SagaScript manifest."));
                        if (!component.TryGetProperty("instanceConfiguration", out var configuration) || configuration.ValueKind != JsonValueKind.Object)
                            diagnostics.Add(Error("Scene.Script.ConfigurationInvalid", behaviorTypeId,
                                "instanceConfiguration must be an object."));
                        components.Add(new JsonObject
                        {
                            ["behaviorTypeId"] = behaviorTypeId,
                            ["instanceConfiguration"] = configuration.ValueKind == JsonValueKind.Object
                                ? JsonNode.Parse(configuration.GetRawText()) : new JsonObject(),
                            ["requiredCapabilities"] = new JsonArray(requiredCapabilities.Order(StringComparer.Ordinal)
                                .Select(item => JsonValue.Create(item)).ToArray()),
                            ["scriptArtifactLogicalId"] = behavior?.ScriptArtifactLogicalId ?? "",
                            ["type"] = type,
                        });
                        break;
                    case "ScriptActionBinding":
                        RejectUnknown(component, entityId + "#ScriptActionBinding",
                            ["type", "inputActionId", "behaviorTypeId", "namedActionId"], diagnostics);
                        var inputActionId = String(component, "inputActionId", entityId, diagnostics);
                        var bindingBehaviorId = String(component, "behaviorTypeId", entityId, diagnostics);
                        var namedActionId = String(component, "namedActionId", entityId, diagnostics);
                        if (!semantic.TryGetValue(bindingBehaviorId, out var bindingBehavior) ||
                            !bindingBehavior.Actions.Contains(namedActionId))
                            diagnostics.Add(Error("Scene.Binding.Unresolved", bindingBehaviorId + "." + namedActionId,
                                "Named action must exist and be publicly invocable in verified SagaScript semantic evidence."));
                        components.Add(new JsonObject
                        {
                            ["behaviorTypeId"] = bindingBehaviorId,
                            ["inputActionId"] = inputActionId,
                            ["namedActionId"] = namedActionId,
                            ["type"] = type,
                        });
                        break;
                    default:
                        diagnostics.Add(Error("Scene.Component.Unknown", entityId, $"Unknown scene component: {type}."));
                        break;
                }
            }
            entities.Add(new JsonObject
            {
                ["components"] = new JsonArray(components.OrderBy(item => item["type"]!.GetValue<string>(), StringComparer.Ordinal)
                    .Select(item => (JsonNode)item).ToArray()),
                ["entityId"] = entityId,
                ["name"] = name,
                ["parentEntityId"] = parentId.Length == 0 ? null : parentId,
            });
        }
        foreach (var parent in parents.Where(item => !entityIds.Contains(item.Value)))
            diagnostics.Add(Error("Scene.Entity.ParentMissing", parent.Key, $"Parent entity '{parent.Value}' does not exist."));
        foreach (var entityId in entityIds)
        {
            var visited = new HashSet<string>(StringComparer.Ordinal);
            var cursor = entityId;
            while (parents.TryGetValue(cursor, out cursor!))
            {
                if (!visited.Add(cursor))
                {
                    diagnostics.Add(Error("Scene.Entity.ParentCycle", entityId, "Scene parent hierarchy contains a cycle."));
                    break;
                }
            }
        }
        if (diagnostics.Count(item => item.Severity == "Error") > errorCount)
        {
            return;
        }
        var payload = new JsonObject
        {
            ["entities"] = new JsonArray(entities.OrderBy(item => item["entityId"]!.GetValue<string>(), StringComparer.Ordinal)
                .Select(item => (JsonNode)item).ToArray()),
            ["sceneId"] = sceneId,
        };
        AddArtifact(Artifact(sceneId, "runtime.scene", CanonicalJson.HashFile(path), payload, null, []), artifacts, null);
    }

    private static IReadOnlyDictionary<string, SemanticBehavior> LoadSemanticEvidence(string currentManifestPath,
        ProjectLoadResult load, List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        var result = new Dictionary<string, SemanticBehavior>(StringComparer.Ordinal);
        if (string.IsNullOrWhiteSpace(currentManifestPath) || !File.Exists(currentManifestPath))
        {
            diagnostics.Add(Error("Scene.Script.SemanticEvidenceMissing", currentManifestPath,
                "Scene compilation requires a verified SagaScript current manifest."));
            return result;
        }
        try
        {
            using var current = JsonDocument.Parse(File.ReadAllBytes(currentManifestPath));
            var root = current.RootElement;
            if (root.GetProperty("projectId").GetString() != load.Manifest!.ProjectId ||
                root.GetProperty("projectHash").GetString() != load.ProjectHash)
                throw new InvalidOperationException("SagaScript manifest project identity/hash does not match the scene project.");
            var outputRoot = Directory.GetParent(Path.GetDirectoryName(Path.GetFullPath(currentManifestPath))!)!.FullName;
            var artifactRecords = root.GetProperty("artifacts").EnumerateArray().ToDictionary(
                item => item.GetProperty("artifactId").GetString() ?? "", item => item, StringComparer.Ordinal);
            JsonDocument Read(string id)
            {
                if (!artifactRecords.TryGetValue(id, out var record)) throw new InvalidOperationException($"Missing SagaScript artifact '{id}'.");
                var objectPath = record.GetProperty("objectPath").GetString() ?? "";
                if (!ProjectLoader.IsProjectRelative(objectPath)) throw new InvalidOperationException("Unsafe SagaScript object path.");
                var full = Path.Combine(outputRoot, objectPath);
                if (CanonicalJson.HashFile(full) != record.GetProperty("artifactHash").GetString())
                    throw new InvalidOperationException($"SagaScript artifact hash mismatch for '{id}'.");
                var bytes = File.ReadAllBytes(full);
                var node = JsonNode.Parse(bytes)?.AsObject()
                    ?? throw new InvalidOperationException($"SagaScript artifact '{id}' is not an object.");
                var embeddedContentHash = node["contentHash"]?.GetValue<string>() ?? "";
                node.Remove("contentHash");
                if (embeddedContentHash != record.GetProperty("contentHash").GetString() ||
                    embeddedContentHash != CanonicalJson.Hash(CanonicalJson.Serialize(node)))
                    throw new InvalidOperationException($"SagaScript content hash mismatch for '{id}'.");
                return JsonDocument.Parse(bytes);
            }
            using var behaviorManifest = Read("scripts.behaviors");
            using var actionManifest = Read("scripts.named-actions");
            using var capabilityManifest = Read("scripts.capabilities");
            using var assemblyManifest = Read("scripts.assembly-manifest");
            var actionRecords = actionManifest.RootElement.GetProperty("actions").EnumerateArray().ToArray();
            if (actionRecords.GroupBy(item => item.GetProperty("actionId").GetString() ?? "", StringComparer.Ordinal)
                .Any(group => group.Count() > 1))
                throw new InvalidOperationException("SagaScript semantic evidence contains duplicate named-action identities.");
            var actions = actionRecords
                .GroupBy(item => item.GetProperty("behaviorId").GetString() ?? "", StringComparer.Ordinal)
                .ToDictionary(group => group.Key,
                    group => (IReadOnlySet<string>)group.Select(item =>
                    {
                        if (!item.GetProperty("publiclyInvocable").GetBoolean() ||
                            item.GetProperty("attributeTypeId").GetString() != "SagaEngine.Scripting.SagaActionAttribute")
                            throw new InvalidOperationException("Named action is not publicly invocable through SagaActionAttribute.");
                        return item.GetProperty("actionId").GetString() ?? "";
                    })
                        .ToHashSet(StringComparer.Ordinal), StringComparer.Ordinal);
            var allCapabilities = capabilityManifest.RootElement.GetProperty("capabilities").EnumerateArray()
                .Select(item => item.GetString() ?? "").ToHashSet(StringComparer.Ordinal);
            var scriptArtifactId = assemblyManifest.RootElement.GetProperty("assemblyId").GetString() ?? "";
            foreach (var behavior in behaviorManifest.RootElement.GetProperty("behaviors").EnumerateArray())
            {
                var id = behavior.GetProperty("behaviorId").GetString() ?? "";
                var behaviorCapabilities = behavior.GetProperty("capabilities").EnumerateArray()
                    .Select(item => item.GetString() ?? "").ToHashSet(StringComparer.Ordinal);
                if (behaviorCapabilities.Any(item => !allCapabilities.Contains(item)))
                    throw new InvalidOperationException($"Behavior capability manifest mismatch for '{id}'.");
                if (!result.TryAdd(id, new SemanticBehavior(id,
                    behavior.GetProperty("sourceFile").GetString() ?? "",
                    behavior.GetProperty("sourceHash").GetString() ?? "",
                    actions.GetValueOrDefault(id) ?? new HashSet<string>(StringComparer.Ordinal),
                    behaviorCapabilities, scriptArtifactId)))
                    diagnostics.Add(Error("Scene.Script.BehaviorDuplicate", id,
                        "SagaScript semantic evidence contains duplicate behavior identities."));
            }
        }
        catch (Exception exception) when (exception is IOException or JsonException or InvalidOperationException or KeyNotFoundException)
        {
            diagnostics.Add(Error("Scene.Script.SemanticEvidenceInvalid", currentManifestPath, exception.Message));
        }
        return result;
    }

    private static void CompileInput(string root, SagaProjectKit.Core.ProjectReference reference,
        List<(JsonObject Document, ContentCompileArtifact Entry)> artifacts,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        var path = Path.Combine(root, reference.Path);
        using var source = Parse(path, "Input.Source.ParseFailed", diagnostics);
        if (source is null)
        {
            return;
        }
        var value = source.RootElement;
        RejectUnknown(value, path, ["schemaVersion", "inputMapId", "actions"], diagnostics);
        RequireSchemaOne(value, path, diagnostics);
        var mapId = String(value, "inputMapId", path, diagnostics);
        if (mapId != reference.Id)
        {
            diagnostics.Add(Error("Input.Source.IdMismatch", path, "Input map id must match its project reference id."));
        }
        var actions = new List<JsonObject>();
        var actionIds = new HashSet<string>(StringComparer.Ordinal);
        var physicalBindings = new HashSet<string>(StringComparer.Ordinal);
        if (!value.TryGetProperty("actions", out var actionArray) || actionArray.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(Error("Input.Source.ActionsMissing", path, "Input actions must be an array."));
            return;
        }
        foreach (var action in actionArray.EnumerateArray())
        {
            RejectUnknown(action, path + "#action", ["actionId", "displayName", "runtimeRole", "bindings"], diagnostics);
            var actionId = String(action, "actionId", path, diagnostics);
            if (!actionIds.Add(actionId))
                diagnostics.Add(Error("Input.Action.DuplicateId", actionId, "Input action identities must be unique."));
            var displayName = String(action, "displayName", path, diagnostics);
            var role = String(action, "runtimeRole", path, diagnostics);
            if (role is not ("Gameplay" or "UI"))
            {
                diagnostics.Add(Error("Input.Action.RuntimeRoleUnknown", actionId, "Runtime role must be Gameplay or UI."));
            }
            var bindings = new List<JsonObject>();
            if (!action.TryGetProperty("bindings", out var bindingArray) || bindingArray.ValueKind != JsonValueKind.Array)
            {
                diagnostics.Add(Error("Input.Action.BindingsMissing", actionId, "Input action bindings must be an array."));
                continue;
            }
            foreach (var binding in bindingArray.EnumerateArray())
            {
                RejectUnknown(binding, actionId + "#binding", ["device", "control", "displayName"], diagnostics);
                var device = String(binding, "device", actionId, diagnostics);
                var control = String(binding, "control", actionId, diagnostics);
                var physicalName = String(binding, "displayName", actionId, diagnostics);
                if (!physicalBindings.Add(device + "\u001f" + control))
                    diagnostics.Add(Error("Input.Binding.DuplicatePhysical", device + " " + control,
                        "A physical binding may map to only one logical action in input source v1."));
                if (device != "Keyboard")
                {
                    diagnostics.Add(Error("Input.Binding.DeviceUnknown", actionId, $"Unknown input device: {device}."));
                }
                if (!Regex.IsMatch(control, "^[A-Z]$"))
                {
                    diagnostics.Add(Error("Input.Binding.ControlUnknown", actionId, $"Unknown keyboard control: {control}."));
                }
                if (physicalName != $"{device} {control}")
                {
                    diagnostics.Add(Error("Input.Binding.DisplayNameMismatch", actionId,
                        "Physical input display name must be '<Device> <Control>'."));
                }
                bindings.Add(new JsonObject
                {
                    ["control"] = control,
                    ["device"] = device,
                    ["displayName"] = physicalName,
                });
            }
            actions.Add(new JsonObject
            {
                ["actionId"] = actionId,
                ["bindings"] = new JsonArray(bindings.OrderBy(item => item["device"]!.GetValue<string>(), StringComparer.Ordinal)
                    .ThenBy(item => item["control"]!.GetValue<string>(), StringComparer.Ordinal).Select(item => (JsonNode)item).ToArray()),
                ["displayName"] = displayName,
                ["runtimeRole"] = role,
            });
        }
        if (diagnostics.Any(item => item.Severity == "Error" && item.Path.StartsWith(path, StringComparison.Ordinal)))
        {
            return;
        }
        var payload = new JsonObject
        {
            ["actions"] = new JsonArray(actions.OrderBy(item => item["actionId"]!.GetValue<string>(), StringComparer.Ordinal)
                .Select(item => (JsonNode)item).ToArray()),
            ["inputMapId"] = mapId,
        };
        AddArtifact(Artifact(mapId, "runtime.input-map", CanonicalJson.HashFile(path), payload, null, []), artifacts, null);
    }

    private static ContentCompileResult Publish(ProjectLoadResult load, string outputRoot, string domain,
        IReadOnlyList<(JsonObject Document, ContentCompileArtifact Entry)> artifacts,
        IReadOnlyList<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
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
                if (!File.Exists(destination))
                {
                    File.Move(staged, destination);
                }
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
                ["toolVersion"] = ToolVersion,
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
            return Failure(domain, load, Sort(diagnostics.Append(Error(
                "Content.Publish.Failed", outputRoot, exception.Message))));
        }
        finally
        {
            if (Directory.Exists(staging))
            {
                Directory.Delete(staging, recursive: true);
            }
        }
    }

    private static JsonObject Artifact(string id, string type, string sourceHash, JsonObject payload,
        JsonObject? provenance, IReadOnlyList<string> dependencies)
    {
        var contentHash = CanonicalJson.Hash(CanonicalJson.Serialize(payload));
        return new JsonObject
        {
            ["artifactId"] = id,
            ["artifactType"] = type,
            ["contentHash"] = contentHash,
            ["cooker"] = new JsonObject { ["name"] = "sagapack", ["version"] = ToolVersion },
            ["dependencies"] = new JsonArray(dependencies.OrderBy(item => item, StringComparer.Ordinal)
                .Select(item => JsonValue.Create(item)).ToArray()),
            ["payload"] = payload,
            ["provenance"] = provenance,
            ["schemaVersion"] = 1,
            ["sourceHash"] = sourceHash,
        };
    }

    private static void AddArtifact(JsonObject document,
        List<(JsonObject Document, ContentCompileArtifact Entry)> artifacts,
        Dictionary<string, string>? hashes)
    {
        var entry = new ContentCompileArtifact
        {
            ArtifactId = document["artifactId"]!.GetValue<string>(),
            ArtifactType = document["artifactType"]!.GetValue<string>(),
            ContentHash = document["contentHash"]!.GetValue<string>(),
            SourceHash = document["sourceHash"]!.GetValue<string>(),
        };
        artifacts.Add((document, entry));
        hashes?.Add(entry.ArtifactId, entry.ContentHash);
    }

    private static JsonObject BuildMeshPayload(string shape, IReadOnlyList<double> dimensions)
    {
        if (shape == "plane")
        {
            var x = dimensions[0] / 2.0;
            var z = dimensions[1] / 2.0;
            return Mesh(shape, [[-x, 0, -z], [x, 0, -z], [x, 0, z], [-x, 0, z]], [0, 1, 2, 0, 2, 3]);
        }
        var hx = dimensions[0] / 2.0;
        var hy = dimensions[1] / 2.0;
        var hz = dimensions[2] / 2.0;
        return Mesh(shape,
            [[-hx, -hy, -hz], [hx, -hy, -hz], [hx, hy, -hz], [-hx, hy, -hz],
             [-hx, -hy, hz], [hx, -hy, hz], [hx, hy, hz], [-hx, hy, hz]],
            [0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
             3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5]);
    }

    private static JsonObject Mesh(string shape, IReadOnlyList<double[]> vertices, IReadOnlyList<int> indices) => new()
    {
        ["indices"] = new JsonArray(indices.Select(item => JsonValue.Create(item)).ToArray()),
        ["primitive"] = shape,
        ["vertices"] = new JsonArray(vertices.Select(vertex =>
            (JsonNode)new JsonArray(vertex.Select(item => JsonValue.Create(item)).ToArray())).ToArray()),
    };

    private static JsonDocument? Parse(string path, string code,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        try
        {
            return JsonDocument.Parse(File.ReadAllBytes(path));
        }
        catch (Exception exception) when (exception is IOException or JsonException)
        {
            diagnostics.Add(Error(code, path, exception.Message));
            return null;
        }
    }

    private static void RejectUnknown(JsonElement value, string path, IReadOnlyList<string> allowed,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (value.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(Error("Content.Schema.ObjectRequired", path, "Expected a JSON object."));
            return;
        }
        foreach (var property in value.EnumerateObject().Where(property => !allowed.Contains(property.Name, StringComparer.Ordinal)))
        {
            diagnostics.Add(Error("Content.Schema.UnknownField", path + "#" + property.Name,
                $"Unknown field: {property.Name}."));
        }
    }

    private static void RequireSchemaOne(JsonElement value, string path,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (!value.TryGetProperty("schemaVersion", out var schema) || !schema.TryGetInt32(out var number) || number != 1)
        {
            diagnostics.Add(Error("Content.Schema.VersionUnsupported", path, "schemaVersion must be 1."));
        }
    }

    private static string String(JsonElement value, string name, string path,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (!value.TryGetProperty(name, out var property) || property.ValueKind != JsonValueKind.String ||
            string.IsNullOrWhiteSpace(property.GetString()))
        {
            diagnostics.Add(Error("Content.Schema.StringRequired", path + "#" + name,
                $"{name} must be a non-empty string."));
            return "";
        }
        return property.GetString()!;
    }

    private static IReadOnlyList<double> NumberArray(JsonElement value, string name, string path,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (!value.TryGetProperty(name, out var property) || property.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(Error("Content.Schema.NumberArrayRequired", path + "#" + name,
                $"{name} must be a number array."));
            return [];
        }
        var result = new List<double>();
        foreach (var item in property.EnumerateArray())
        {
            if (item.ValueKind != JsonValueKind.Number || !item.TryGetDouble(out var number) || !double.IsFinite(number))
            {
                diagnostics.Add(Error("Content.Schema.NumberArrayRequired", path + "#" + name,
                    $"{name} must contain only numbers."));
                return [];
            }
            result.Add(number);
        }
        return result;
    }

    private static double Number(JsonElement value, string name, string path,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (!value.TryGetProperty(name, out var property) || property.ValueKind != JsonValueKind.Number ||
            !property.TryGetDouble(out var number) || !double.IsFinite(number))
        {
            diagnostics.Add(Error("Content.Schema.FiniteNumberRequired", path + "#" + name,
                $"{name} must be a finite number."));
            return double.NaN;
        }
        return number;
    }

    private static IReadOnlyList<string> StringArray(JsonElement value, string name, string path,
        List<SagaProjectKit.Core.ProjectDiagnostic> diagnostics)
    {
        if (!value.TryGetProperty(name, out var property) || property.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(Error("Content.Schema.StringArrayRequired", path + "#" + name,
                $"{name} must be a string array."));
            return [];
        }
        var result = new List<string>();
        foreach (var item in property.EnumerateArray())
        {
            if (item.ValueKind != JsonValueKind.String || string.IsNullOrWhiteSpace(item.GetString()))
            {
                diagnostics.Add(Error("Content.Schema.StringArrayRequired", path + "#" + name,
                    $"{name} must contain non-empty strings."));
                return [];
            }
            result.Add(item.GetString()!);
        }
        if (result.Distinct(StringComparer.Ordinal).Count() != result.Count)
            diagnostics.Add(Error("Content.Schema.StringArrayDuplicate", path + "#" + name,
                $"{name} must not contain duplicate identities."));
        return result;
    }

    private static ContentCompileResult Failure(string domain, ProjectLoadResult load,
        IReadOnlyList<SagaProjectKit.Core.ProjectDiagnostic> diagnostics) => new()
    {
        Command = "compile-" + domain,
        Status = "Failed",
        ProjectId = load.Manifest?.ProjectId ?? "",
        ProjectHash = load.ProjectHash,
        Diagnostics = diagnostics,
    };

    private static List<SagaProjectKit.Core.ProjectDiagnostic> Sort(
        IEnumerable<SagaProjectKit.Core.ProjectDiagnostic> diagnostics) => diagnostics
        .OrderByDescending(item => item.Severity == "Error")
        .ThenBy(item => item.Code, StringComparer.Ordinal)
        .ThenBy(item => item.Path, StringComparer.Ordinal)
        .ThenBy(item => item.Message, StringComparer.Ordinal).ToList();

    private static SagaProjectKit.Core.ProjectDiagnostic Error(string code, string path, string message) => new()
    {
        Code = code,
        Path = path.Replace('\\', '/'),
        Message = message,
    };
}
