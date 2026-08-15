// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using System.Text.Json.Nodes;
using System.Security.Cryptography;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using SagaProjectKit.Core;

namespace SagaScript;

/// Builds immutable worker artifacts. Publication and freshness remain owned by
/// ProjectWorker so a projection can never advance current.json on its own.
internal static class ProjectWorkerArtifactBuilder
{
    public static IReadOnlyList<StagedWorkerArtifact> Build(ProjectWorkerRequest request,
        IReadOnlyList<WorkerSourceInput> sources, string sourceSetHash,
        IReadOnlyList<WorkerBehavior> behaviors, IReadOnlyList<object> allowedApis, string staging,
        string runtimeBridgeAssembly, List<WorkerDiagnostic> diagnostics)
    {
        var common = new
        {
            schemaVersion = 1,
            projectId = request.Project.ProjectId,
            projectHash = request.Project.Hash,
            sourceRevision = request.SourceRevision,
            sourceSetHash,
            packageGenerationId = request.PackageGenerationId,
            packageGenerationHash = request.PackageGenerationHash,
            packageGenerationManifestHash = request.PackageGenerationManifestHash,
            packageGraphHash = request.PackageGraphHash,
            apiSurfaceManifestHash = request.ApiSurfaceManifest.Hash,
            ownershipCatalogHash = request.OwnershipCatalog.Hash,
            tool = ToolInfo.Name,
            toolVersion = ToolInfo.Version,
            analyzerVersion = typeof(ProjectWorkerArtifactBuilder).Assembly.GetName().Version?.ToString() ?? "unknown",
            compilerVersion = typeof(CSharpCompilation).Assembly.GetName().Version?.ToString() ?? "unknown",
        };
        var artifacts = new List<StagedWorkerArtifact>();
        artifacts.Add(JsonArtifact("scripts.behaviors", "behaviorManifest", "behavior_manifest_v4.json", new
        {
            schemaVersion = 4, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            common.packageGenerationManifestHash, common.packageGraphHash, common.apiSurfaceManifestHash,
            common.ownershipCatalogHash,
            common.tool, common.toolVersion, common.analyzerVersion, common.compilerVersion, behaviors,
            diagnostics = Array.Empty<object>(),
        }));
        artifacts.Add(JsonArtifact("behavior.modules", "behaviorModuleCatalog", "behavior_modules_v1.json", new
        {
            schemaVersion = 1, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            common.packageGenerationManifestHash, common.packageGraphHash,
            common.apiSurfaceManifestHash, common.ownershipCatalogHash,
            modules = behaviors.Where(item => item.BehaviorModuleId.Length > 0).Select(item => new
            {
                behaviorModuleId = item.BehaviorModuleId,
                language = "csharp",
                sourceIdentity = new
                {
                    sourceSymbolId = item.SourceSymbolId,
                    sourcePath = item.SourceFile,
                    identityKind = item.IdentityKind,
                },
                projectId = common.projectId,
                ownership = new { kind = "project", ownerId = common.projectId },
                packageGenerationId = common.packageGenerationId,
                sourceInventory = new[] { item.SourceFile },
                requestedCapabilities = item.Capabilities,
            }).OrderBy(item => item.behaviorModuleId, StringComparer.Ordinal).ToArray(),
            legacySources = behaviors.Where(item => item.BehaviorModuleId.Length == 0).Select(item => new
            {
                language = "csharp",
                sourceSymbolId = item.SourceSymbolId,
                sourcePath = item.SourceFile,
                identityKind = item.IdentityKind,
            }).OrderBy(item => item.sourceSymbolId, StringComparer.Ordinal).ToArray(),
            diagnostics = Array.Empty<object>(),
        }));
        var actions = behaviors.SelectMany(behavior => behavior.Actions.Select(action => new
        {
            action.ActionId,
            behaviorId = behavior.BehaviorId,
            action.Method,
            action.AttributeTypeId,
            action.PubliclyInvocable,
            sourceFile = behavior.SourceFile,
            action.Span,
        })).OrderBy(item => item.ActionId, StringComparer.Ordinal).ToArray();
        artifacts.Add(JsonArtifact("scripts.named-actions", "namedActionManifest", "named_actions_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            common.packageGraphHash, common.apiSurfaceManifestHash, common.ownershipCatalogHash,
            common.tool, common.toolVersion,
            common.analyzerVersion, actions, diagnostics = Array.Empty<object>(),
        }));
        var capabilities = behaviors.SelectMany(item => item.Capabilities).Distinct(StringComparer.Ordinal)
            .Order(StringComparer.Ordinal).ToArray();
        artifacts.Add(JsonArtifact("scripts.capabilities", "capabilityManifest", "capabilities_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            common.packageGraphHash, common.apiSurfaceManifestHash, common.ownershipCatalogHash,
            common.tool, common.toolVersion,
            common.analyzerVersion, capabilities, diagnostics = Array.Empty<object>(),
        }));
        var semanticOperations = allowedApis.Select(item =>
            JsonSerializer.SerializeToElement(item, CanonicalJson.SerializerOptions))
            .Where(api => !string.IsNullOrWhiteSpace(
                api.GetProperty("operationId").GetString()))
            .Select(api => new
            {
                operationId = api.GetProperty("operationId").GetString() ?? "",
                apiId = api.GetProperty("surfaceId").GetString() ?? "",
                packageId = api.GetProperty("packageId").GetString() ?? "",
                module = api.GetProperty("module").GetString() ?? "",
                apiLevel = api.GetProperty("apiLevel").GetString() ?? "",
                effect = api.GetProperty("effect").GetString() ?? "",
                requiredCapabilities = api.GetProperty("requiredCapabilities")
                    .EnumerateArray().Select(item => item.GetString() ?? "")
                    .Order(StringComparer.Ordinal).ToArray(),
                runtimeBinding = api.GetProperty("runtimeBinding").GetString() ?? "",
                csharpBinding = api.GetProperty("apiId").GetString() ?? "",
                csharp = true,
                blocks = api.GetProperty("blockProjection").GetString() == "Supported",
            }).OrderBy(item => item.operationId, StringComparer.Ordinal).ToArray();
        artifacts.Add(JsonArtifact("api.operations", "apiOperationCatalog",
            "saga_api_operations_v1.json", new
        {
            schemaVersion = 1, common.projectId, common.packageGenerationId,
            common.packageGenerationHash, common.packageGraphHash,
            common.apiSurfaceManifestHash, operations = semanticOperations,
            diagnostics = Array.Empty<object>(),
        }));
        artifacts.Add(JsonArtifact("scripts.source-map", "sourceMap", "source_map_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            common.packageGenerationManifestHash, common.packageGraphHash, common.apiSurfaceManifestHash,
            common.ownershipCatalogHash,
            common.tool, common.toolVersion, common.analyzerVersion, sources,
            behaviors = behaviors.Select(item => new { item.BehaviorId, item.SourceFile, item.SourceHash, item.Span,
                item.LifecycleMethods, item.Actions }).ToArray(), diagnostics = Array.Empty<object>(),
        }));

        var roots = behaviors.SelectMany(behavior => behavior.Actions.Select(action => new
        {
            cardId = $"action://{behavior.BehaviorId}/{action.ActionId}",
            kind = "Event",
            displayName = $"WHEN {action.ActionId}",
            packageOrigin = request.PackageGenerationId,
            sourceFile = behavior.SourceFile,
            sourceHash = behavior.SourceHash,
            sourceSpan = action.Span,
            children = action.Steps.Select(step => new
            {
                cardId = step.StepId,
                kind = step.Kind,
                step.DisplayName,
                packageOrigin = request.PackageGenerationId,
                stateKey = step.StateKey,
                sourceFile = behavior.SourceFile,
                sourceHash = behavior.SourceHash,
                sourceSpan = step.Span,
                children = Array.Empty<object>(),
            }).ToArray(),
        })).ToArray();
        var blocks = roots.Select(root => new
        {
            blockId = root.cardId,
            blockKind = "NamedAction",
            root.displayName,
            classification = "ReadOnlySupported",
            editable = false,
            root.sourceFile,
            root.sourceHash,
            root.sourceSpan,
        }).ToArray();
        var opaque = behaviors.SelectMany(behavior => behavior.LifecycleMethods.Select(method => new
        {
            regionId = $"opaque://{behavior.BehaviorId}/{method.Name}",
            classification = "OpaqueCSharp",
            displayName = "Opaque C# region",
            editable = false,
            packageOrigin = request.PackageGenerationId,
            behavior.SourceFile,
            behavior.SourceHash,
            sourceSpan = method.Span,
            reason = "Unsupported C# remains source-linked and opaque; projection never rewrites authored C#.",
        })).ToArray();
        var editableFields = ControlledFields(request, sources, behaviors);
        artifacts.Add(JsonArtifact("scripts.block-projection", "blockProjection", "block_projection_v2.json", new
        {
            schemaVersion = 2, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            common.packageGenerationManifestHash, common.packageGraphHash, common.apiSurfaceManifestHash,
            common.ownershipCatalogHash,
            common.tool, common.toolVersion, common.analyzerVersion,
            sourcePreservation = "SourceNotMutated", readOnly = false,
            controlledEdits = true, unsupportedRegionsReadOnly = true,
            editableFields, roots, blocks,
            opaqueRegions = opaque, allowedApis, filteredApiPolicy = "ExcludeRawAndInternal",
            diagnostics = Array.Empty<object>(),
        }));

        var compiledRoot = Path.Combine(staging, "compiled");
        var assemblyPath = Directory.GetFiles(compiledRoot, "*.scripts.dll", SearchOption.TopDirectoryOnly).SingleOrDefault();
        var runtimeConfigPath = Directory.GetFiles(compiledRoot, "*.scripts.runtimeconfig.json", SearchOption.TopDirectoryOnly).SingleOrDefault();
        if (assemblyPath is null || runtimeConfigPath is null)
        {
            diagnostics.Add(new WorkerDiagnostic { Code = "Script.Worker.AssemblyMissing", Path = "compile",
                Message = "Deterministic compilation did not produce assembly outputs." });
            return artifacts;
        }
        var assembly = BinaryArtifact("scripts.assembly", "assembly", Path.GetFileName(assemblyPath), File.ReadAllBytes(assemblyPath));
        var runtimeConfig = BinaryArtifact("scripts.runtime-config", "runtimeConfig", Path.GetFileName(runtimeConfigPath), File.ReadAllBytes(runtimeConfigPath));
        artifacts.Add(assembly);
        artifacts.Add(runtimeConfig);
        var assemblyManifest = JsonArtifact("scripts.assembly-manifest", "assemblyManifest", "assembly_manifest_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            common.packageGenerationManifestHash, common.packageGraphHash, common.apiSurfaceManifestHash,
            common.ownershipCatalogHash,
            common.tool, common.toolVersion, common.analyzerVersion, common.compilerVersion,
            assemblyId = assembly.ArtifactId, assemblyHash = assembly.ArtifactHash,
            runtimeConfigId = runtimeConfig.ArtifactId, runtimeConfigHash = runtimeConfig.ArtifactHash,
            targetFramework = ToolInfo.TargetFramework, referenceManifestHash = request.ReferenceManifest.Hash,
            runtimeBridgeLogicalReferenceId = request.RuntimeBridge.LogicalReferenceId,
            runtimeBridgeDescriptorHash = request.RuntimeBridge.DescriptorHash,
            runtimeBridgeAssemblyLogicalPath = ReadBridgeLogicalPath(request.ReferenceManifest.Path,
                request.RuntimeBridge.LogicalReferenceId),
            runtimeBridgeHash = request.RuntimeBridge.AssemblyHash,
            runtimeCompatibility = request.RuntimeBridge.RuntimeCompatibility,
            trust = request.RuntimeBridge.Trust, diagnostics = Array.Empty<object>(),
        });
        artifacts.Add(assemblyManifest);
        artifacts.Add(JsonArtifact("behavior.artifacts", "behaviorArtifactCatalog", "behavior_artifacts_v1.json", new
        {
            schemaVersion = 1, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.packageGenerationId, common.packageGenerationHash,
            artifacts = new[]
            {
                new
                {
                    artifactId = "artifact://sha256/" + assembly.ArtifactHash["sha256:".Length..],
                    language = "csharp",
                    format = "saga.csharp.assembly-generation.v1",
                    artifactHash = assembly.ArtifactHash,
                    packageGenerationId = common.packageGenerationId,
                    payloadArtifacts = new[]
                    {
                        new { artifactId = assembly.ArtifactId, kind = assembly.Kind, hash = assembly.ArtifactHash },
                        new { artifactId = assemblyManifest.ArtifactId, kind = assemblyManifest.Kind, hash = assemblyManifest.ArtifactHash },
                        new { artifactId = runtimeConfig.ArtifactId, kind = runtimeConfig.Kind, hash = runtimeConfig.ArtifactHash },
                    }.OrderBy(item => item.artifactId, StringComparer.Ordinal).ToArray(),
                    behaviorModuleIds = behaviors.Where(item => item.BehaviorModuleId.Length > 0)
                        .Select(item => item.BehaviorModuleId).Order(StringComparer.Ordinal).ToArray(),
                },
            },
            diagnostics = Array.Empty<object>(),
        }));
        return artifacts.OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToArray();
    }

    private static object[] ControlledFields(ProjectWorkerRequest request,
        IReadOnlyList<WorkerSourceInput> sources, IReadOnlyList<WorkerBehavior> behaviors)
    {
        const string behaviorId = "ArenaRush.ArenaRushGameMode";
        var behavior = behaviors.SingleOrDefault(item => item.SourceSymbolId == behaviorId);
        if (behavior is null) return [];
        var source = sources.SingleOrDefault(item => item.Path == behavior.SourceFile);
        if (source is null) return [];
        var projectRoot = Path.GetDirectoryName(request.Project.Path) ?? "";
        var sourcePath = Path.GetFullPath(Path.Combine(projectRoot, source.Path));
        var text = File.ReadAllText(sourcePath, Encoding.UTF8);
        var root = CSharpSyntaxTree.ParseText(text).GetCompilationUnitRoot();
        var type = root.DescendantNodes().OfType<ClassDeclarationSyntax>()
            .SingleOrDefault(item => item.Identifier.ValueText == "ArenaRushGameMode");
        if (type is null) return [];
        var definitions = new[]
        {
            (Name: "EnergyCoreScore", Id: "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer",
                ValueType: "Int32", Minimum: "1"),
            (Name: "EnergyCoreRespawnSeconds", Id: "field://ArenaRush.ArenaRushGameMode/EnergyCoreRespawnSeconds/initializer",
                ValueType: "Double", Minimum: "0.0"),
        };
        var fields = new List<object>();
        foreach (var definition in definitions)
        {
            var variable = type.Members.OfType<FieldDeclarationSyntax>()
                .Where(item => item.Modifiers.Any(token =>
                    token.IsKind(SyntaxKind.ConstKeyword)))
                .SelectMany(item => item.Declaration.Variables)
                .SingleOrDefault(item => item.Identifier.ValueText == definition.Name);
            if (variable?.Initializer?.Value is not LiteralExpressionSyntax literal) continue;
            var startByte = Encoding.UTF8.GetByteCount(text.AsSpan(0, literal.SpanStart));
            var valueBytes = Encoding.UTF8.GetBytes(literal.ToString());
            fields.Add(new
            {
                fieldId = definition.Id,
                valueType = definition.ValueType,
                minimum = definition.Minimum,
                containingSymbol = behaviorId,
                initializerRole = "ConstantInitializerLiteral",
                sourceFile = source.Path,
                sourceHash = source.Hash,
                sourceRevision = request.SourceRevision,
                packageGenerationId = request.PackageGenerationId,
                packageGenerationHash = request.PackageGenerationHash,
                startByte,
                endByte = startByte + valueBytes.Length,
                checksum = "sha256:" + Convert.ToHexString(SHA256.HashData(valueBytes)).ToLowerInvariant(),
                value = literal.ToString(),
            });
        }
        return fields.ToArray();
    }

    private static StagedWorkerArtifact JsonArtifact(string id, string kind, string name, object payload)
    {
        var node = JsonSerializer.SerializeToNode(payload, CanonicalJson.SerializerOptions)!.AsObject();
        var contentHash = CanonicalJson.Hash(CanonicalJson.Serialize(node));
        node["contentHash"] = contentHash;
        var bytes = CanonicalJson.Serialize(node);
        return new StagedWorkerArtifact(id, kind, name, contentHash, CanonicalJson.Hash(bytes), bytes);
    }

    private static StagedWorkerArtifact BinaryArtifact(string id, string kind, string name, byte[] bytes)
    {
        var hash = CanonicalJson.Hash(bytes);
        return new StagedWorkerArtifact(id, kind, name, hash, hash, bytes);
    }

    private static string ReadBridgeLogicalPath(string referenceManifestPath, string logicalReferenceId)
    {
        using var document = JsonDocument.Parse(File.ReadAllBytes(referenceManifestPath));
        return document.RootElement.GetProperty("references").EnumerateArray()
            .Single(item => item.GetProperty("artifactId").GetString() == logicalReferenceId)
            .GetProperty("resolvedAssemblyLogicalPath").GetString() ?? "";
    }
}
