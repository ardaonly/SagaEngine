// SagaWeaverArtifacts.cs
// Source-preserving SagaBehavior analysis, projection, and patch-preview artifacts.

using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace SagaScript;

internal sealed record SagaWeaverArtifactSet
{
    public required IReadOnlyList<string> Inputs { get; init; }
    public required IReadOnlyDictionary<string, string> SourceTexts { get; init; }
    public required IReadOnlyDictionary<string, string> SourceHashes { get; init; }
    public required IReadOnlyList<SagaWeaverBehavior> Behaviors { get; init; }
    public required IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; }
    public required DiagnosticSummary Summary { get; init; }
    public required string SourceHash { get; init; }
}

internal static class SagaWeaverArtifacts
{
    private static readonly HashSet<string> Domains = new(StringComparer.Ordinal)
    {
        "Gameplay",
        "Runtime",
        "Server",
        "Networking",
        "UI",
        "Diagnostics",
        "Assets",
        "Packaging"
    };

    private static readonly HashSet<string> Levels = new(StringComparer.Ordinal)
    {
        "High",
        "Low"
    };

    private static readonly HashSet<string> Capabilities = new(StringComparer.Ordinal)
    {
        "RuntimeBacked",
        "ProjectionOnly",
        "Deferred"
    };

    private static readonly IReadOnlyDictionary<string, string> PolicyNodeCapabilities =
        new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["Gameplay.High.Inventory.Has"] = "ProjectionOnly",
            ["Gameplay.High.Door.Open"] = "ProjectionOnly",
            ["Gameplay.High.Score.Add"] = "ProjectionOnly",
            ["Gameplay.High.Audio.PlayEvent"] = "ProjectionOnly",
            ["Gameplay.High.Entity.SetTag"] = "ProjectionOnly",
            ["Gameplay.Low.Trigger.OnEnter"] = "Deferred",
            ["Gameplay.Low.Spawn.Entity"] = "Deferred",
            ["Gameplay.Low.Timer.Delay"] = "Deferred",
            ["Gameplay.Low.Entity.DestroyOrDeactivate"] = "Deferred",
        };

    private static readonly IReadOnlyDictionary<string, string> InvocationNodeIds =
        new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["Inventory.Has"] = "Gameplay.High.Inventory.Has",
            ["Door.Open"] = "Gameplay.High.Door.Open",
            ["Score.Add"] = "Gameplay.High.Score.Add",
            ["Audio.PlayEvent"] = "Gameplay.High.Audio.PlayEvent",
            ["Entity.SetTag"] = "Gameplay.High.Entity.SetTag",
            ["Trigger.OnEnter"] = "Gameplay.Low.Trigger.OnEnter",
            ["Spawn.Entity"] = "Gameplay.Low.Spawn.Entity",
            ["Timer.Delay"] = "Gameplay.Low.Timer.Delay",
            ["Entity.DestroyOrDeactivate"] = "Gameplay.Low.Entity.DestroyOrDeactivate",
        };

    public static SagaWeaverArtifactSet Analyze(IReadOnlyList<string> sourceFiles, bool strict = false)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var behaviors = new List<SagaWeaverBehavior>();
        var sourceTexts = new Dictionary<string, string>(StringComparer.Ordinal);
        var sourceHashes = new Dictionary<string, string>(StringComparer.Ordinal);

        foreach (var sourceFile in sourceFiles.Order(StringComparer.Ordinal))
        {
            var sourceText = File.ReadAllText(sourceFile, Encoding.UTF8);
            var sourceHash = ComputeHash(sourceText);
            sourceTexts[sourceFile] = sourceText;
            sourceHashes[sourceFile] = sourceHash;

            var tree = CSharpSyntaxTree.ParseText(sourceText, path: sourceFile);
            var root = tree.GetCompilationUnitRoot();
            var parseErrors = tree.GetDiagnostics()
                .Where(diagnostic => diagnostic.Severity == DiagnosticSeverity.Error)
                .ToArray();
            foreach (var parseError in parseErrors)
            {
                diagnostics.Add(new SagaDiagnostic
                {
                    Severity = SagaDiagnosticSeverity.Error.ToString(),
                    Code = "Script.ParseFailed",
                    Message = parseError.ToString(),
                    SourceFile = sourceFile,
                    SourceRange = ToRange(tree, parseError.Location),
                });
            }

            if (parseErrors.Length > 0)
            {
                continue;
            }

            var namespaceUsages = FindSagaNamespaceUsages(root);
            foreach (var typeDeclaration in root.DescendantNodes().OfType<TypeDeclarationSyntax>())
            {
                var typeMetadata = ReadMetadata(AttributeMap.From(typeDeclaration.AttributeLists), "SagaBehavior");
                var methodBehaviors = typeDeclaration.Members
                    .OfType<MethodDeclarationSyntax>()
                    .Where(method => ReadMetadata(AttributeMap.From(method.AttributeLists), "SagaBehavior") is not null)
                    .ToArray();

                if (typeMetadata is null && methodBehaviors.Length == 0)
                {
                    continue;
                }

                if (typeMetadata is not null)
                {
                    behaviors.Add(BuildBehavior(
                        sourceFile,
                        sourceText,
                        sourceHash,
                        tree,
                        typeDeclaration,
                        null,
                        typeMetadata.Value,
                        namespaceUsages,
                        diagnostics,
                        strict));
                }

                foreach (var method in methodBehaviors)
                {
                    var metadata = ReadMetadata(AttributeMap.From(method.AttributeLists), "SagaBehavior");
                    if (metadata is not null)
                    {
                        behaviors.Add(BuildBehavior(
                            sourceFile,
                            sourceText,
                            sourceHash,
                            tree,
                            typeDeclaration,
                            method,
                            metadata.Value,
                            namespaceUsages,
                            diagnostics,
                            strict));
                    }
                }
            }
        }

        if (sourceFiles.Count > 0 && behaviors.Count == 0)
        {
            diagnostics.Add(new SagaDiagnostic
            {
                Severity = strict ? SagaDiagnosticSeverity.Error.ToString() : SagaDiagnosticSeverity.Warning.ToString(),
                Code = "Script.Metadata.MissingBehavior",
                Message = "No [SagaBehavior] metadata was found in the supplied C# sources."
            });
        }

        behaviors = behaviors
            .OrderBy(behavior => behavior.BehaviorId, StringComparer.Ordinal)
            .ThenBy(behavior => behavior.SourceFile, StringComparer.Ordinal)
            .ToList();
        diagnostics = SortDiagnostics(diagnostics).ToList();
        var summary = SagaScriptAnalyzer.BuildSummary(diagnostics);

        return new SagaWeaverArtifactSet
        {
            Inputs = sourceFiles.Order(StringComparer.Ordinal).ToArray(),
            SourceTexts = sourceTexts,
            SourceHashes = sourceHashes,
            Behaviors = behaviors,
            Diagnostics = diagnostics,
            Summary = summary,
            SourceHash = ComputeHash(string.Join('\n', sourceHashes
                .OrderBy(pair => pair.Key, StringComparer.Ordinal)
                .Select(pair => $"{pair.Key}:{pair.Value}")))
        };
    }

    public static SagaWeaverAnalysisReport BuildAnalysisReport(SagaWeaverArtifactSet artifacts, string command = "analyze")
    {
        return new SagaWeaverAnalysisReport
        {
            Command = command,
            Status = artifacts.Summary.HasBlockingDiagnostics ? "Failed" : "Passed",
            SourceHash = artifacts.SourceHash,
            Behaviors = artifacts.Behaviors,
            Summary = artifacts.Summary,
            Diagnostics = artifacts.Diagnostics
        };
    }

    public static NodeLibraryReport ExtractNodeLibrary(IReadOnlyList<string> sourceFiles)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var libraries = new List<NodeLibraryEntry>();
        var nodes = new List<NodeLibraryNodeEntry>();
        var sourceTexts = new Dictionary<string, string>(StringComparer.Ordinal);
        var sourceHashes = new Dictionary<string, string>(StringComparer.Ordinal);

        foreach (var sourceFile in sourceFiles.Order(StringComparer.Ordinal))
        {
            var sourceText = File.ReadAllText(sourceFile, Encoding.UTF8);
            var sourceHash = ComputeHash(sourceText);
            sourceTexts[sourceFile] = sourceText;
            sourceHashes[sourceFile] = sourceHash;

            var tree = CSharpSyntaxTree.ParseText(sourceText, path: sourceFile);
            var root = tree.GetCompilationUnitRoot();
            var parseErrors = tree.GetDiagnostics()
                .Where(diagnostic => diagnostic.Severity == DiagnosticSeverity.Error)
                .ToArray();
            foreach (var parseError in parseErrors)
            {
                diagnostics.Add(new SagaDiagnostic
                {
                    Severity = SagaDiagnosticSeverity.Error.ToString(),
                    Code = "Script.ParseFailed",
                    Message = parseError.ToString(),
                    SourceFile = sourceFile,
                    SourceRange = ToRange(tree, parseError.Location),
                });
            }
            if (parseErrors.Length > 0)
            {
                continue;
            }

            foreach (var typeDeclaration in root.DescendantNodes().OfType<TypeDeclarationSyntax>())
            {
                var typeAttributes = AttributeMap.From(typeDeclaration.AttributeLists);
                var libraryMetadata = ReadNodeMetadata(typeAttributes, "SagaLibrary");
                NodeLibraryEntry? library = null;
                if (libraryMetadata is not null)
                {
                    var localDiagnostics = ValidateLibraryMetadata(
                        libraryMetadata.Value,
                        sourceFile,
                        tree,
                        typeDeclaration);
                    diagnostics.AddRange(localDiagnostics);
                    library = new NodeLibraryEntry
                    {
                        LibraryId = libraryMetadata.Value.Id,
                        DisplayName = DisplayNameOrFallback(libraryMetadata.Value.DisplayName, typeDeclaration.Identifier.ValueText),
                        ApiDomain = libraryMetadata.Value.Domain,
                        ApiLevel = libraryMetadata.Value.Level,
                        SourceFile = sourceFile,
                        SourceSpan = ToSpan(sourceText, tree, typeDeclaration.GetLocation()),
                        Diagnostics = localDiagnostics,
                    };
                    libraries.Add(library);
                }

                foreach (var method in typeDeclaration.Members.OfType<MethodDeclarationSyntax>())
                {
                    var nodeMetadata = ReadNodeMetadata(AttributeMap.From(method.AttributeLists), "SagaNode");
                    if (nodeMetadata is null)
                    {
                        continue;
                    }

                    var localDiagnostics = ValidateNodeMetadata(
                        nodeMetadata.Value,
                        library,
                        sourceFile,
                        tree,
                        method);
                    diagnostics.AddRange(localDiagnostics);
                    nodes.Add(new NodeLibraryNodeEntry
                    {
                        NodeId = nodeMetadata.Value.Id,
                        DisplayName = DisplayNameOrFallback(nodeMetadata.Value.DisplayName, method.Identifier.ValueText),
                        ApiDomain = nodeMetadata.Value.Domain,
                        ApiLevel = nodeMetadata.Value.Level,
                        Capability = ResolveCapability(nodeMetadata.Value),
                        SourceFile = sourceFile,
                        SourceSpan = ToSpan(sourceText, tree, method.GetLocation()),
                        Compatibility = localDiagnostics.Any(SagaScriptAnalyzer.IsBlocking) ? "Invalid" : "MetadataOnly",
                        Diagnostics = localDiagnostics,
                    });
                }
            }
        }

        foreach (var duplicate in nodes
            .Where(node => !string.IsNullOrWhiteSpace(node.NodeId))
            .GroupBy(node => node.NodeId, StringComparer.Ordinal)
            .Where(group => group.Count() > 1)
            .OrderBy(group => group.Key, StringComparer.Ordinal))
        {
            diagnostics.Add(new SagaDiagnostic
            {
                Severity = SagaDiagnosticSeverity.Error.ToString(),
                Code = "Script.Node.DuplicateId",
                Message = $"Duplicate SagaNode id '{duplicate.Key}'.",
                ScriptId = duplicate.Key,
            });
        }

        if (sourceFiles.Count > 0 && libraries.Count == 0 && nodes.Count == 0)
        {
            diagnostics.Add(new SagaDiagnostic
            {
                Severity = SagaDiagnosticSeverity.Warning.ToString(),
                Code = "Script.NodeLibrary.MissingMetadata",
                Message = "No [SagaLibrary] or [SagaNode] metadata was found in the supplied C# sources."
            });
        }

        diagnostics = SortDiagnostics(diagnostics).ToList();
        var summary = SagaScriptAnalyzer.BuildSummary(diagnostics);
        var aggregateSourceHash = ComputeHash(string.Join('\n', sourceHashes
            .OrderBy(pair => pair.Key, StringComparer.Ordinal)
            .Select(pair => $"{pair.Key}:{pair.Value}")));

        return new NodeLibraryReport
        {
            Status = summary.HasBlockingDiagnostics ? "Failed" : "Passed",
            SourceHash = aggregateSourceHash,
            SourceFiles = sourceHashes
                .OrderBy(pair => pair.Key, StringComparer.Ordinal)
                .Select(pair => new SourceMapFile
                {
                    SourceFile = pair.Key,
                    SourceHash = pair.Value,
                    ByteLength = Encoding.UTF8.GetByteCount(sourceTexts[pair.Key])
                })
                .ToArray(),
            Libraries = libraries
                .OrderBy(library => library.LibraryId, StringComparer.Ordinal)
                .ThenBy(library => library.SourceFile, StringComparer.Ordinal)
                .ToArray(),
            Nodes = nodes
                .OrderBy(node => node.NodeId, StringComparer.Ordinal)
                .ThenBy(node => node.SourceFile, StringComparer.Ordinal)
                .ToArray(),
            Summary = summary,
            Diagnostics = diagnostics,
        };
    }

    public static DiagnosticReport BuildDiagnosticReport(SagaWeaverArtifactSet artifacts)
    {
        return new DiagnosticReport
        {
            Inputs = artifacts.Inputs,
            Summary = artifacts.Summary,
            Diagnostics = artifacts.Diagnostics
        };
    }

    public static RuntimeBindingReport BuildRuntimeBindings(SagaWeaverArtifactSet artifacts)
    {
        return new RuntimeBindingReport
        {
            Status = artifacts.Summary.HasBlockingDiagnostics ? "Failed" : "Passed",
            SourceHash = artifacts.SourceHash,
            Bindings = artifacts.Behaviors
                .Where(behavior => behavior.Compatibility == "Supported")
                .Select(behavior => new RuntimeBindingEntry
                {
                    BehaviorId = behavior.BehaviorId,
                    ApiLevel = behavior.ApiLevel,
                    ApiDomain = behavior.ApiDomain,
                    DeclaringType = behavior.DeclaringType,
                    MethodName = behavior.MethodName,
                    SourceFile = behavior.SourceFile,
                    SourceHash = behavior.SourceHash,
                    SourceSpan = behavior.SourceSpan,
                    Compatibility = behavior.Compatibility,
                    Nodes = behavior.Nodes
                        .Select(node => new RuntimeBindingNode
                        {
                            NodeId = node.NodeId,
                            Kind = node.Kind,
                            ApiDomain = node.ApiDomain,
                            ApiLevel = node.ApiLevel,
                            Capability = node.Capability,
                            ProjectionCompatibility = node.ProjectionCompatibility,
                            CompatibilityClassification = ClassifyProjectionNode(node),
                            RuntimeProof = RuntimeProofFor(node.Capability)
                        })
                        .OrderBy(node => node.NodeId, StringComparer.Ordinal)
                        .ToArray(),
                    LibraryIds = behavior.Nodes
                        .Select(node => LibraryIdForNode(node.DisplayName))
                        .Where(libraryId => !string.IsNullOrWhiteSpace(libraryId))
                        .Distinct(StringComparer.Ordinal)
                        .Order(StringComparer.Ordinal)
                        .ToArray()
                })
                .OrderBy(binding => binding.BehaviorId, StringComparer.Ordinal)
                .ToArray(),
            Summary = artifacts.Summary,
            Diagnostics = artifacts.Diagnostics
        };
    }

    public static CompatibilityProfileReport BuildCompatibilityProfile(IReadOnlyList<string> sourceFiles)
    {
        var artifacts = Analyze(sourceFiles);
        var diagnostics = artifacts.Diagnostics.ToList();
        var constructs = new List<CompatibilityConstruct>();

        foreach (var behavior in artifacts.Behaviors)
        {
            constructs.Add(new CompatibilityConstruct
            {
                ConstructId = behavior.BehaviorId,
                Kind = "SagaBehavior",
                Classification = behavior.Compatibility == "Supported" ? "ReadOnlyProjectable" : "Invalid",
                ApiDomain = behavior.ApiDomain,
                ApiLevel = behavior.ApiLevel,
                Capability = "ProjectionOnly",
                SourceFile = behavior.SourceFile,
                SourceSpan = behavior.SourceSpan,
                Editable = false,
                Reason = behavior.Compatibility == "Supported"
                    ? "SagaBehavior metadata is projectable as read-only profile evidence."
                    : "SagaBehavior metadata has blocking diagnostics."
            });

            foreach (var node in behavior.Nodes)
            {
                constructs.Add(ConstructFromNode(behavior, node));
            }
        }

        AddExplicitCompatibilityScans(sourceFiles, artifacts.SourceTexts, diagnostics, constructs);

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        constructs = constructs
            .OrderBy(construct => construct.SourceFile, StringComparer.Ordinal)
            .ThenBy(construct => construct.SourceSpan?.StartByte ?? 0)
            .ThenBy(construct => construct.ConstructId, StringComparer.Ordinal)
            .ToList();
        var summary = BuildCompatibilitySummary(constructs, sortedDiagnostics);
        var status = sortedDiagnostics.Any(SagaScriptAnalyzer.IsBlocking) ||
            constructs.Any(construct => construct.Classification == "Invalid")
                ? "Failed"
                : "Passed";

        return new CompatibilityProfileReport
        {
            Status = status,
            SourceFiles = artifacts.SourceHashes
                .OrderBy(pair => pair.Key, StringComparer.Ordinal)
                .Select(pair => new SourceMapFile
                {
                    SourceFile = pair.Key,
                    SourceHash = pair.Value,
                    ByteLength = Encoding.UTF8.GetByteCount(artifacts.SourceTexts[pair.Key])
                })
                .ToArray(),
            SourceHashes = artifacts.SourceHashes
                .OrderBy(pair => pair.Key, StringComparer.Ordinal)
                .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal),
            Constructs = constructs,
            Summary = summary,
            Diagnostics = sortedDiagnostics
        };
    }

    public static VisualBlocksProjectionReport BuildVisualBlocksProjection(CompatibilityProfileReport profile)
    {
        var blocks = profile.Constructs
            .Select(construct => new VisualBlockEntry
            {
                BlockId = BuildVisualBlockId(construct),
                BlockKind = BlockKindFor(construct),
                DisplayName = DisplayNameFor(construct),
                Classification = construct.Classification,
                Editable = false,
                SourceFile = construct.SourceFile,
                SourceSpan = construct.SourceSpan,
                SourceHash = SourceHashFor(profile, construct.SourceFile),
                Children = Array.Empty<string>(),
                Diagnostics = construct.Diagnostics
            })
            .OrderBy(block => block.SourceFile, StringComparer.Ordinal)
            .ThenBy(block => block.SourceSpan?.StartByte ?? 0)
            .ThenBy(block => block.BlockId, StringComparer.Ordinal)
            .ToArray();

        var opaqueRegions = profile.Constructs
            .Where(construct => construct.Classification is "Opaque" or "Unsupported" or "Invalid")
            .Select(construct => new VisualOpaqueRegionEntry
            {
                BlockId = BuildVisualBlockId(construct),
                BlockKind = construct.Classification == "Opaque"
                    ? "OpaqueSourceRegionBlock"
                    : "UnsupportedDiagnosticBlock",
                DisplayName = DisplayNameFor(construct),
                Classification = construct.Classification,
                Editable = false,
                SourceFile = construct.SourceFile,
                SourceSpan = construct.SourceSpan,
                SourceHash = SourceHashFor(profile, construct.SourceFile),
                Reason = construct.Reason,
                Diagnostics = construct.Diagnostics
            })
            .OrderBy(region => region.SourceFile, StringComparer.Ordinal)
            .ThenBy(region => region.SourceSpan?.StartByte ?? 0)
            .ThenBy(region => region.BlockId, StringComparer.Ordinal)
            .ToArray();

        return new VisualBlocksProjectionReport
        {
            ProjectionStatus = profile.Status,
            SourceFiles = profile.SourceFiles,
            Blocks = blocks,
            OpaqueRegions = opaqueRegions,
            Diagnostics = profile.Diagnostics,
            NonClaims =
            [
                "MetadataOnly",
                "ReadOnlyProjection",
                "NoVisualBlocksEditor",
                "NoBlockEditing",
                "NoSourceMutation",
                "NoArbitraryCSharpToBlocks",
                "NoRuntimeGraphInterpretation"
            ]
        };
    }

    public static ScriptArtifactValidationReport ValidateArtifacts(string artifactRoot)
    {
        var root = Path.GetFullPath(artifactRoot);
        var diagnostics = new List<SagaDiagnostic>();
        var checkedArtifacts = new List<CheckedScriptArtifact>();

        var nodeLibrary = CheckArtifact(root, "nodeLibrary", "node_library_report.json", required: false, diagnostics: diagnostics);
        var nodeMetadata = CheckArtifact(root, "nodeMetadata", "node_metadata.json", required: false, diagnostics: diagnostics);
        var runtimeBindings = CheckArtifact(root, "runtimeBindings", "runtime_bindings.json", required: false, diagnostics: diagnostics);
        var compatibilityProfile = CheckArtifact(root, "compatibilityProfile", "csharp_compatibility_profile_v2.json", required: true, diagnostics: diagnostics);

        checkedArtifacts.AddRange([nodeLibrary.Check, nodeMetadata.Check, runtimeBindings.Check, compatibilityProfile.Check]);
        var patchFiles = new (string File, string Command, bool MutatesSource)[]
        {
            ("patch_preview.json", "patch-preview", false),
            ("patch_apply_report.json", "patch-apply", true),
            ("patch_diff_report.json", "patch-diff", false),
            ("patch_review_report.json", "patch-review", false),
            ("patch_rollback_report.json", "patch-rollback", true),
        };
        foreach (var patch in patchFiles)
        {
            var checkedPatch = CheckArtifact(root, "patchReport", patch.File, required: false, diagnostics: diagnostics);
            checkedArtifacts.Add(checkedPatch.Check);
            if (checkedPatch.Json is null)
            {
                continue;
            }
            if (ReadString(checkedPatch.Json, "command") != patch.Command)
            {
                diagnostics.Add(Error("Script.Artifact.PatchCommandMismatch", $"{patch.File} must be a {patch.Command} report.", checkedPatch.Path));
            }
            if (ReadBool(checkedPatch.Json, "mutatesSource") != patch.MutatesSource)
            {
                diagnostics.Add(Error("Script.Artifact.PatchMutationMismatch", $"{patch.File} has an unexpected mutatesSource value.", checkedPatch.Path));
            }
        }

        if (nodeMetadata.Json is not null && runtimeBindings.Json is not null)
        {
            ValidateNodeCapabilityConsistency(nodeMetadata.Json, runtimeBindings.Json, diagnostics);
        }
        if (nodeMetadata.Json is not null && nodeLibrary.Json is null)
        {
            diagnostics.Add(Error("Script.Artifact.NodeLibraryMissing", "node_metadata.json is present without node_library_report.json evidence.", nodeLibrary.Path));
        }
        if (nodeLibrary.Json is not null && ReadString(nodeLibrary.Json, "status") != "Passed")
        {
            diagnostics.Add(Error("Script.Artifact.NodeLibraryFailed", "node_library_report.json must have status Passed.", nodeLibrary.Path));
        }
        if (compatibilityProfile.Json is not null && ReadString(compatibilityProfile.Json, "status") != "Passed")
        {
            diagnostics.Add(Error("Script.Artifact.CompatibilityProfileFailed", "csharp_compatibility_profile_v2.json must have status Passed.", compatibilityProfile.Path));
        }

        var runtimeProofSummary = SummarizeRuntimeProof(runtimeBindings.Json, diagnostics, runtimeBindings.Path);
        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var status = sortedDiagnostics.Any(SagaScriptAnalyzer.IsBlocking) ? "Failed" : "Passed";

        return new ScriptArtifactValidationReport
        {
            Status = status,
            ArtifactRoot = root,
            CheckedArtifacts = checkedArtifacts
                .OrderBy(item => item.Kind, StringComparer.Ordinal)
                .ThenBy(item => item.Path, StringComparer.Ordinal)
                .ToArray(),
            RuntimeProofSummary = runtimeProofSummary,
            Summary = new Dictionary<string, int>(StringComparer.Ordinal)
            {
                ["checkedArtifactCount"] = checkedArtifacts.Count,
                ["diagnosticCount"] = sortedDiagnostics.Length
            },
            Diagnostics = sortedDiagnostics
        };
    }

    public static SourceMapReport BuildSourceMap(SagaWeaverArtifactSet artifacts, string command = "source-map")
    {
        return new SourceMapReport
        {
            Command = command,
            Status = artifacts.Summary.HasBlockingDiagnostics ? "Failed" : "Passed",
            SourceHash = artifacts.SourceHash,
            Files = artifacts.SourceHashes
                .OrderBy(pair => pair.Key, StringComparer.Ordinal)
                .Select(pair => new SourceMapFile
                {
                    SourceFile = pair.Key,
                    SourceHash = pair.Value,
                    ByteLength = Encoding.UTF8.GetByteCount(artifacts.SourceTexts[pair.Key])
                })
                .ToArray(),
            Behaviors = artifacts.Behaviors
                .Select(behavior => new SourceMapBehavior
                {
                    BehaviorId = behavior.BehaviorId,
                    ApiLevel = behavior.ApiLevel,
                    ApiDomain = behavior.ApiDomain,
                    SourceFile = behavior.SourceFile,
                    SourceHash = behavior.SourceHash,
                    SourceSpan = behavior.SourceSpan,
                    Nodes = behavior.Nodes
                })
                .ToArray(),
            Summary = artifacts.Summary,
            Diagnostics = artifacts.Diagnostics
        };
    }

    public static ProjectionReport BuildProjection(SagaWeaverArtifactSet artifacts)
    {
        return new ProjectionReport
        {
            Status = artifacts.Summary.HasBlockingDiagnostics ? "Failed" : "Passed",
            ApiLevel = SummarizeAxis(artifacts.Behaviors.Select(behavior => behavior.ApiLevel)),
            ApiDomain = SummarizeAxis(artifacts.Behaviors.Select(behavior => behavior.ApiDomain)),
            SourceHash = artifacts.SourceHash,
            Behaviors = artifacts.Behaviors,
            Summary = artifacts.Summary,
            Diagnostics = artifacts.Diagnostics
        };
    }

    public static NodeMetadataReport BuildNodeMetadata(SagaWeaverArtifactSet artifacts)
    {
        return new NodeMetadataReport
        {
            Status = artifacts.Summary.HasBlockingDiagnostics ? "Failed" : "Passed",
            SourceHash = artifacts.SourceHash,
            Nodes = artifacts.Behaviors
                .SelectMany(behavior => behavior.Nodes.Select(node => new NodeMetadataEntry
                {
                    NodeId = node.NodeId,
                    DisplayName = node.DisplayName,
                    ApiDomain = node.ApiDomain,
                    ApiLevel = node.ApiLevel,
                    Capability = node.Capability,
                    ProjectionCompatibility = node.ProjectionCompatibility,
                    Level = node.ApiLevel == "High" ? "HighLevel" :
                        node.ApiLevel == "Low" ? "LowLevel" : "Unsupported",
                    Domain = node.ApiDomain,
                    Capabilities = [node.Capability],
                    SourceFile = behavior.SourceFile,
                    SourceSpan = node.SourceSpan,
                    ReadOnly = node.ReadOnly
                }))
                .OrderBy(node => node.NodeId, StringComparer.Ordinal)
                .ToArray(),
            Summary = artifacts.Summary,
            Diagnostics = artifacts.Diagnostics
        };
    }

    public static JsonObject BuildPatchPreview(
        IReadOnlyList<string> sourceFiles,
        string sourceMapPath,
        string patchRequestPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var sourceMap = JsonNode.Parse(File.ReadAllText(sourceMapPath)) as JsonObject
            ?? throw new InvalidOperationException("source-map must be a JSON object.");
        var request = JsonNode.Parse(File.ReadAllText(patchRequestPath)) as JsonObject
            ?? throw new InvalidOperationException("patch request must be a JSON object.");

        var operation = ReadString(request, "operation");
        var nodeId = ReadString(request, "nodeId");
        var baseSourceHash = ReadString(request, "baseSourceHash");
        var replacement = ReadString(request, "replacement");

        if (operation != "ReplaceStringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedOperation", $"Unsupported patch operation '{operation}'."));
        }

        var node = FindNode(sourceMap, nodeId);
        var status = "Passed";
        var sourceFile = "";
        var oldText = "";
        SourceSpan? span = null;
        if (node is null)
        {
            diagnostics.Add(Error("Script.Patch.NodeMissing", $"Patch target node was not found: {nodeId}."));
        }
        else
        {
            var kind = ReadString(node, "kind");
            if (kind != "StringLiteral")
            {
                diagnostics.Add(Error("Script.Patch.UnsupportedNode", "ReplaceStringLiteral requires a StringLiteral source-map node."));
            }
            if (ReadBool(node, "readOnly"))
            {
                diagnostics.Add(Error("Script.Patch.ReadOnlyNode", "Patch target is read-only or opaque."));
            }

            sourceFile = FindBehaviorSourceFile(sourceMap, nodeId);
            span = ReadSpan(node["sourceSpan"] as JsonObject);
            if (sourceFile.Length == 0 || span is null)
            {
                diagnostics.Add(Error("Script.Patch.SourceSpanMissing", "Patch target does not include a source span."));
            }
            else
            {
                var fullSource = sourceFiles
                    .Select(Path.GetFullPath)
                    .FirstOrDefault(path => string.Equals(path, sourceFile, StringComparison.Ordinal)) ?? sourceFile;
                var sourceText = File.ReadAllText(fullSource, Encoding.UTF8);
                var actualHash = ComputeHash(sourceText);
                if (!actualHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                {
                    diagnostics.Add(Error("Script.Patch.SourceHashMismatch", "Patch base source hash does not match current source."));
                }
                if (span.StartByte >= 0 && span.EndByte <= Encoding.UTF8.GetByteCount(sourceText) && span.EndByte >= span.StartByte)
                {
                    oldText = SliceUtf8(sourceText, span.StartByte, span.EndByte);
                    if (!oldText.StartsWith("\"", StringComparison.Ordinal) || !oldText.EndsWith("\"", StringComparison.Ordinal))
                    {
                        diagnostics.Add(Error("Script.Patch.TargetNotStringLiteral", "Patch target span is not a string literal."));
                    }
                }
            }
        }

        var summary = SagaScriptAnalyzer.BuildSummary(SortDiagnostics(diagnostics).ToArray());
        if (summary.HasBlockingDiagnostics)
        {
            status = "Failed";
        }

        var payload = new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-preview",
            ["status"] = status,
            ["operation"] = operation,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["baseSourceHash"] = baseSourceHash,
            ["mutatesSource"] = false,
            ["preview"] = status == "Passed" && span is not null
                ? new JsonObject
                {
                    ["startByte"] = span.StartByte,
                    ["endByte"] = span.EndByte,
                    ["oldText"] = oldText,
                    ["newText"] = "\"" + replacement + "\""
                }
                : null,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(SortDiagnostics(diagnostics))
        };
        return payload;
    }

    public static JsonObject BuildPatchApply(
        IReadOnlyList<string> sourceFiles,
        IReadOnlyList<string> sourceInputs,
        string sourceMapPath,
        string patchRequestPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var sourceMap = ReadJsonObjectFile(sourceMapPath, "source-map", diagnostics);
        var request = ReadJsonObjectFile(patchRequestPath, "patch request", diagnostics);

        var operation = ReadString(request, "operation");
        var nodeId = ReadString(request, "nodeId");
        var baseSourceHash = ReadString(request, "baseSourceHash");
        var expectedOldText = ReadString(request, "expectedOldText");
        var replacement = ReadString(request, "replacement");
        var operationId = BuildOperationId(operation, nodeId, baseSourceHash, expectedOldText, replacement);
        var newText = "\"" + replacement + "\"";
        var sourceFile = "";
        var backupPath = "";
        var tempPath = "";
        var rollbackStatus = "NotNeeded";
        var oldText = "";
        var newSourceHash = "";
        SourceSpan? span = null;

        if (request is not null)
        {
            foreach (var required in new[] { "operation", "nodeId", "baseSourceHash", "expectedOldText", "replacement" })
            {
                if (!HasStringField(request, required))
                {
                    diagnostics.Add(Error("Script.Patch.MissingField", $"patch-apply requires '{required}'."));
                }
            }
            if (request.TryGetPropertyValue("operations", out _))
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", "patch-apply accepts exactly one target operation."));
            }
        }

        if (operation != "ReplaceStringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedOperation", $"Unsupported patch operation '{operation}'."));
        }

        JsonObject? node = null;
        if (sourceMap is not null)
        {
            var matches = FindNodeMatches(sourceMap, nodeId).ToArray();
            if (matches.Length == 0)
            {
                diagnostics.Add(Error("Script.Patch.NodeMissing", $"Patch target node was not found: {nodeId}."));
            }
            else if (matches.Length > 1)
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", $"Patch target node id is ambiguous: {nodeId}."));
            }
            else
            {
                node = matches[0].Node;
                sourceFile = matches[0].SourceFile;
            }
        }

        byte[] originalBytes = [];
        byte[] replacementBytes = Encoding.UTF8.GetBytes(newText);
        if (node is not null)
        {
            ValidatePatchNode(node, diagnostics);
            span = ReadSpan(node["sourceSpan"] as JsonObject);
            if (sourceFile.Length == 0 || span is null)
            {
                diagnostics.Add(Error("Script.Patch.SourceSpanMissing", "Patch target does not include a source span.", sourceFile));
            }
            else
            {
                var resolvedSource = ResolvePatchSourceFile(sourceFiles, sourceInputs, sourceFile);
                if (resolvedSource.Length == 0)
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileOutsideInput", "Patch target source file is not part of the supplied --source inputs.", sourceFile));
                }
                else if (!File.Exists(resolvedSource))
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileMissing", "Patch target source file does not exist.", sourceFile));
                }
                else
                {
                    sourceFile = resolvedSource;
                    originalBytes = File.ReadAllBytes(sourceFile);
                    var sourceText = File.ReadAllText(sourceFile, Encoding.UTF8);
                    var actualHash = ComputeHash(sourceText);
                    if (!actualHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.Patch.SourceHashMismatch", "Patch base source hash does not match current source.", sourceFile));
                    }

                    if (!IsValidByteSpan(span, originalBytes.Length))
                    {
                        diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span has invalid UTF-8 byte bounds.", sourceFile));
                    }
                    else
                    {
                        try
                        {
                            oldText = DecodeUtf8(originalBytes, span.StartByte, span.EndByte - span.StartByte);
                            if (oldText != expectedOldText)
                            {
                                diagnostics.Add(Error("Script.Patch.ExpectedOldTextMismatch", "Patch expectedOldText does not match current source span.", sourceFile));
                            }
                            if (!IsQuotedStringLiteral(oldText))
                            {
                                diagnostics.Add(Error("Script.Patch.TargetNotStringLiteral", "Patch target span is not a string literal.", sourceFile));
                            }
                        }
                        catch (DecoderFallbackException)
                        {
                            diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span is not valid UTF-8.", sourceFile));
                        }
                    }
                }
            }
        }

        if (!IsQuotedStringLiteral(expectedOldText))
        {
            diagnostics.Add(Error("Script.Patch.ExpectedOldTextInvalid", "patch-apply expectedOldText must be a quoted string literal."));
        }
        if (!IsQuotedStringLiteral(newText))
        {
            diagnostics.Add(Error("Script.Patch.ReplacementInvalid", "patch-apply replacement must form a quoted string literal."));
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";

        if (status == "Passed" && span is not null)
        {
            backupPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-backup.{operationId}");
            tempPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-tmp.{operationId}");

            try
            {
                var patchedBytes = ReplaceBytes(originalBytes, span.StartByte, span.EndByte, replacementBytes);
                File.Copy(sourceFile, backupPath, overwrite: true);
                File.WriteAllBytes(tempPath, patchedBytes);
                File.Move(tempPath, sourceFile, overwrite: true);

                var afterBytes = File.ReadAllBytes(sourceFile);
                var forcePostApplyFailure = ReadBool(request, "testOnlyForcePostApplyVerificationFailure");
                if (forcePostApplyFailure || !VerifySingleSpanReplacement(originalBytes, afterBytes, span.StartByte, span.EndByte, replacementBytes))
                {
                    diagnostics.Add(Error("Script.Patch.PostApplyVerificationFailed", "Patch apply post-check failed; attempting rollback.", sourceFile));
                    rollbackStatus = RestoreBackup(backupPath, sourceFile) ? "Restored" : "Failed";
                    status = "Failed";
                }
                else
                {
                    newSourceHash = ComputeHash(File.ReadAllText(sourceFile, Encoding.UTF8));
                }
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                diagnostics.Add(Error("Script.Patch.WriteFailed", $"Patch apply write failed: {ex.Message}", sourceFile));
                TryDeleteFile(tempPath);
                if (File.Exists(backupPath) && SourceBytesChanged(sourceFile, originalBytes))
                {
                    rollbackStatus = RestoreBackup(backupPath, sourceFile) ? "Restored" : "Failed";
                }
                status = "Failed";
            }

            sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
            summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
            if (summary.HasBlockingDiagnostics)
            {
                status = "Failed";
            }
        }

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-apply",
            ["status"] = status,
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["operation"] = operation,
            ["operationId"] = operationId,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["baseSourceHash"] = baseSourceHash,
            ["newSourceHash"] = newSourceHash,
            ["sourceSpan"] = span is null ? null : SpanJson(span),
            ["oldText"] = oldText,
            ["newText"] = newText,
            ["backupPath"] = backupPath,
            ["tempPath"] = tempPath,
            ["rollbackStatus"] = rollbackStatus,
            ["mutatesSource"] = true,
            ["staleArtifacts"] = StaleArtifactArray(),
            ["artifactRegeneration"] = "NotPerformed"
        };
    }

    public static JsonObject BuildPatchDiff(
        IReadOnlyList<string> sourceFiles,
        IReadOnlyList<string> sourceInputs,
        string sourceMapPath,
        string patchRequestPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var sourceMap = ReadJsonObjectFile(sourceMapPath, "source-map", diagnostics);
        var request = ReadJsonObjectFile(patchRequestPath, "patch request", diagnostics);

        var operation = ReadString(request, "operation");
        var nodeId = ReadString(request, "nodeId");
        var baseSourceHash = ReadString(request, "baseSourceHash");
        var expectedOldText = ReadString(request, "expectedOldText");
        var replacement = ReadString(request, "replacement");
        var operationId = BuildOperationId(operation, nodeId, baseSourceHash, expectedOldText, replacement);
        var newText = "\"" + replacement + "\"";
        var sourceFile = "";
        var oldText = "";
        var proposedSourceHash = "";
        SourceSpan? span = null;
        byte[] originalBytes = [];
        byte[] replacementBytes = Encoding.UTF8.GetBytes(newText);
        byte[] proposedBytes = [];
        var originalText = "";
        var proposedText = "";

        if (request is not null)
        {
            foreach (var required in new[] { "operation", "nodeId", "baseSourceHash", "expectedOldText", "replacement" })
            {
                if (!HasStringField(request, required))
                {
                    diagnostics.Add(Error("Script.Patch.MissingField", $"patch-diff requires '{required}'."));
                }
            }
            if (request.TryGetPropertyValue("operations", out _))
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", "patch-diff accepts exactly one target operation."));
            }
        }

        if (operation != "ReplaceStringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedOperation", $"Unsupported patch operation '{operation}'."));
        }

        JsonObject? node = null;
        if (sourceMap is not null)
        {
            var matches = FindNodeMatches(sourceMap, nodeId).ToArray();
            if (matches.Length == 0)
            {
                diagnostics.Add(Error("Script.Patch.NodeMissing", $"Patch target node was not found: {nodeId}."));
            }
            else if (matches.Length > 1)
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", $"Patch target node id is ambiguous: {nodeId}."));
            }
            else
            {
                node = matches[0].Node;
                sourceFile = matches[0].SourceFile;
            }
        }

        if (node is not null)
        {
            ValidatePatchNode(node, diagnostics);
            span = ReadSpan(node["sourceSpan"] as JsonObject);
            if (sourceFile.Length == 0 || span is null)
            {
                diagnostics.Add(Error("Script.Patch.SourceSpanMissing", "Patch target does not include a source span.", sourceFile));
            }
            else
            {
                var resolvedSource = ResolvePatchSourceFile(sourceFiles, sourceInputs, sourceFile);
                if (resolvedSource.Length == 0)
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileOutsideInput", "Patch target source file is not part of the supplied --source inputs.", sourceFile));
                }
                else if (!File.Exists(resolvedSource))
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileMissing", "Patch target source file does not exist.", sourceFile));
                }
                else
                {
                    sourceFile = resolvedSource;
                    originalBytes = File.ReadAllBytes(sourceFile);
                    originalText = File.ReadAllText(sourceFile, Encoding.UTF8);
                    var actualHash = ComputeHash(originalText);
                    if (!actualHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.Patch.SourceHashMismatch", "Patch base source hash does not match current source.", sourceFile));
                    }

                    if (!IsValidByteSpan(span, originalBytes.Length))
                    {
                        diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span has invalid UTF-8 byte bounds.", sourceFile));
                    }
                    else
                    {
                        try
                        {
                            oldText = DecodeUtf8(originalBytes, span.StartByte, span.EndByte - span.StartByte);
                            if (oldText != expectedOldText)
                            {
                                diagnostics.Add(Error("Script.Patch.ExpectedOldTextMismatch", "Patch expectedOldText does not match current source span.", sourceFile));
                            }
                            if (!IsQuotedStringLiteral(oldText))
                            {
                                diagnostics.Add(Error("Script.Patch.TargetNotStringLiteral", "Patch target span is not a string literal.", sourceFile));
                            }
                            proposedBytes = ReplaceBytes(originalBytes, span.StartByte, span.EndByte, replacementBytes);
                            proposedText = DecodeUtf8(proposedBytes, 0, proposedBytes.Length);
                            proposedSourceHash = ComputeHash(proposedText);
                        }
                        catch (DecoderFallbackException)
                        {
                            diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span is not valid UTF-8.", sourceFile));
                        }
                    }
                }
            }
        }

        if (!IsQuotedStringLiteral(expectedOldText))
        {
            diagnostics.Add(Error("Script.Patch.ExpectedOldTextInvalid", "patch-diff expectedOldText must be a quoted string literal."));
        }
        if (!IsQuotedStringLiteral(newText))
        {
            diagnostics.Add(Error("Script.Patch.ReplacementInvalid", "patch-diff replacement must form a quoted string literal."));
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";
        var newEndByte = span is null ? 0 : span.StartByte + replacementBytes.Length;

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-diff",
            ["status"] = status,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["operation"] = operation,
            ["operationId"] = operationId,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["baseSourceHash"] = baseSourceHash,
            ["proposedSourceHash"] = status == "Passed" ? proposedSourceHash : "",
            ["sourceSpan"] = span is null ? null : SpanJson(span),
            ["oldText"] = oldText,
            ["newText"] = newText,
            ["byteDiff"] = span is null
                ? null
                : new JsonObject
                {
                    ["startByte"] = span.StartByte,
                    ["oldEndByte"] = span.EndByte,
                    ["newEndByte"] = newEndByte,
                    ["oldByteLength"] = span.EndByte - span.StartByte,
                    ["newByteLength"] = replacementBytes.Length
                },
            ["textDiff"] = status == "Passed"
                ? BuildUnifiedDiff(originalText, proposedText)
                : null,
            ["mutatesSource"] = false,
            ["artifactRegeneration"] = "NotPerformed"
        };
    }

    public static JsonObject BuildPatchReview(
        string diffReportPath,
        string decision,
        string reviewer)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var diffReport = ReadJsonObjectFile(diffReportPath, "patch diff report", diagnostics);

        if (decision is not ("Approved" or "Rejected"))
        {
            diagnostics.Add(Error("Script.PatchReview.InvalidDecision", "patch-review decision must be Approved or Rejected."));
        }
        if (string.IsNullOrWhiteSpace(reviewer))
        {
            diagnostics.Add(Error("Script.PatchReview.MissingReviewer", "patch-review requires a reviewer."));
        }

        if (diffReport is not null)
        {
            if (ReadString(diffReport, "command") != "patch-diff")
            {
                diagnostics.Add(Error("Script.PatchReview.InvalidDiffReport", "patch-review requires a patch-diff report."));
            }
            if (ReadString(diffReport, "status") != "Passed")
            {
                diagnostics.Add(Error("Script.PatchReview.FailedDiffReport", "patch-review requires a passed patch-diff report."));
            }
            if (ReadBool(diffReport, "mutatesSource"))
            {
                diagnostics.Add(Error("Script.PatchReview.MutatingDiffReport", "patch-review requires a non-mutating diff report."));
            }
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-review",
            ["status"] = status,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["decision"] = decision,
            ["reviewer"] = reviewer,
            ["reviewedCommand"] = ReadString(diffReport, "command"),
            ["diffReport"] = Path.GetFullPath(diffReportPath),
            ["operation"] = ReadString(diffReport, "operation"),
            ["operationId"] = ReadString(diffReport, "operationId"),
            ["nodeId"] = ReadString(diffReport, "nodeId"),
            ["sourceFile"] = ReadString(diffReport, "sourceFile"),
            ["baseSourceHash"] = ReadString(diffReport, "baseSourceHash"),
            ["proposedSourceHash"] = ReadString(diffReport, "proposedSourceHash"),
            ["mutatesSource"] = false,
            ["appliesPatch"] = false,
            ["artifactRegeneration"] = "NotPerformed"
        };
    }

    public static JsonObject BuildPatchRollback(string applyReportPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var applyReport = ReadJsonObjectFile(applyReportPath, "patch apply report", diagnostics);
        var operation = ReadString(applyReport, "operation");
        var operationId = ReadString(applyReport, "operationId");
        var nodeId = ReadString(applyReport, "nodeId");
        var sourceFile = ReadString(applyReport, "sourceFile");
        var backupPath = ReadString(applyReport, "backupPath");
        var baseSourceHash = ReadString(applyReport, "baseSourceHash");
        var newSourceHash = ReadString(applyReport, "newSourceHash");
        var preRollbackSourceHash = "";
        var postRollbackSourceHash = "";
        var rollbackStatus = "NotPerformed";
        var restoresExactPreviousBytes = false;

        if (applyReport is not null)
        {
            if (ReadString(applyReport, "command") != "patch-apply")
            {
                diagnostics.Add(Error("Script.PatchRollback.InvalidApplyReport", "patch-rollback requires a patch-apply report."));
            }
            if (ReadString(applyReport, "status") != "Passed")
            {
                diagnostics.Add(Error("Script.PatchRollback.FailedApplyReport", "patch-rollback requires a passed patch-apply report."));
            }
            if (!ReadBool(applyReport, "mutatesSource"))
            {
                diagnostics.Add(Error("Script.PatchRollback.NonMutatingApplyReport", "patch-rollback requires a mutating patch-apply report."));
            }
            foreach (var required in new[] { "sourceFile", "backupPath", "baseSourceHash", "newSourceHash" })
            {
                if (!HasStringField(applyReport, required))
                {
                    diagnostics.Add(Error("Script.PatchRollback.MissingField", $"patch-rollback requires apply report field '{required}'."));
                }
            }
        }

        byte[] currentBytes = [];
        byte[] backupBytes = [];
        if (!string.IsNullOrWhiteSpace(sourceFile))
        {
            sourceFile = Path.GetFullPath(sourceFile);
            if (!File.Exists(sourceFile))
            {
                diagnostics.Add(Error("Script.PatchRollback.SourceFileMissing", "Patch rollback source file does not exist.", sourceFile));
            }
            else
            {
                currentBytes = File.ReadAllBytes(sourceFile);
                try
                {
                    preRollbackSourceHash = ComputeHash(DecodeUtf8(currentBytes, 0, currentBytes.Length));
                    if (!preRollbackSourceHash.Equals(newSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.PatchRollback.SourceHashMismatch", "Current source hash does not match patch_apply_report.newSourceHash.", sourceFile));
                    }
                }
                catch (DecoderFallbackException)
                {
                    diagnostics.Add(Error("Script.PatchRollback.InvalidSourceEncoding", "Current source is not valid UTF-8.", sourceFile));
                }
            }
        }

        if (!string.IsNullOrWhiteSpace(backupPath))
        {
            backupPath = Path.GetFullPath(backupPath);
            if (!File.Exists(backupPath))
            {
                diagnostics.Add(Error("Script.PatchRollback.BackupMissing", "Patch rollback backup file does not exist.", backupPath));
            }
            else
            {
                backupBytes = File.ReadAllBytes(backupPath);
                try
                {
                    var backupHash = ComputeHash(DecodeUtf8(backupBytes, 0, backupBytes.Length));
                    if (!backupHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.PatchRollback.BackupHashMismatch", "Backup hash does not match patch_apply_report.baseSourceHash.", backupPath));
                    }
                }
                catch (DecoderFallbackException)
                {
                    diagnostics.Add(Error("Script.PatchRollback.InvalidBackupEncoding", "Patch rollback backup is not valid UTF-8.", backupPath));
                }
            }
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";

        if (status == "Passed")
        {
            var tempPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-rollback-tmp.{operationId}");
            var safeguardPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-rollback-safeguard.{operationId}");

            try
            {
                ReplaceFileWithBytes(sourceFile, tempPath, backupBytes);
                postRollbackSourceHash = ComputeHash(File.ReadAllText(sourceFile, Encoding.UTF8));
                var forcePostRollbackFailure = ReadBool(applyReport, "testOnlyForceRollbackVerificationFailure");
                if (forcePostRollbackFailure ||
                    !postRollbackSourceHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase) ||
                    !File.ReadAllBytes(sourceFile).SequenceEqual(backupBytes))
                {
                    diagnostics.Add(Error("Script.PatchRollback.PostRollbackVerificationFailed", "Patch rollback post-check failed; restoring pre-rollback source.", sourceFile));
                    ReplaceFileWithBytes(sourceFile, safeguardPath, currentBytes);
                    rollbackStatus = "Failed";
                    status = "Failed";
                }
                else
                {
                    rollbackStatus = "Restored";
                    restoresExactPreviousBytes = true;
                }
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                diagnostics.Add(Error("Script.PatchRollback.WriteFailed", $"Patch rollback write failed: {ex.Message}", sourceFile));
                TryDeleteFile(tempPath);
                try
                {
                    if (File.Exists(sourceFile) && !File.ReadAllBytes(sourceFile).SequenceEqual(currentBytes))
                    {
                        ReplaceFileWithBytes(sourceFile, safeguardPath, currentBytes);
                    }
                }
                catch (Exception restoreEx) when (restoreEx is IOException or UnauthorizedAccessException)
                {
                    diagnostics.Add(Error("Script.PatchRollback.RestoreCurrentFailed", $"Patch rollback could not restore current source: {restoreEx.Message}", sourceFile));
                }
                rollbackStatus = "Failed";
                status = "Failed";
            }

            sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
            summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
            if (summary.HasBlockingDiagnostics)
            {
                status = "Failed";
            }
        }

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-rollback",
            ["status"] = status,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["applyReport"] = Path.GetFullPath(applyReportPath),
            ["operation"] = operation,
            ["operationId"] = operationId,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["backupPath"] = backupPath,
            ["baseSourceHash"] = baseSourceHash,
            ["preRollbackSourceHash"] = preRollbackSourceHash,
            ["postRollbackSourceHash"] = postRollbackSourceHash,
            ["rollbackStatus"] = rollbackStatus,
            ["mutatesSource"] = true,
            ["restoresExactPreviousBytes"] = restoresExactPreviousBytes,
            ["artifactRegeneration"] = "NotPerformed",
            ["staleArtifacts"] = StaleArtifactArray()
        };
    }

    private static CompatibilityConstruct ConstructFromNode(SagaWeaverBehavior behavior, SagaWeaverNode node)
    {
        var classification = ClassifyProjectionNode(node);
        var editable = classification == "EditableByPatch";
        return new CompatibilityConstruct
        {
            ConstructId = node.NodeId,
            Kind = ProfileKindForNode(node),
            Classification = classification,
            ApiDomain = node.ApiDomain,
            ApiLevel = node.ApiLevel,
            Capability = node.Capability,
            SourceFile = behavior.SourceFile,
            SourceSpan = node.SourceSpan,
            Editable = editable,
            PatchOperation = editable ? "ReplaceStringLiteral" : "",
            Reason = ClassificationReason(node, classification)
        };
    }

    private static string ProfileKindForNode(SagaWeaverNode node)
    {
        return node.Kind switch
        {
            "Event" => "BehaviorMethod",
            "If" => "IfStatement",
            "Call" or "Operation" => "Invocation",
            "Opaque" => "OpaqueRegion",
            "StringLiteral" => "StringLiteral",
            _ => node.Kind
        };
    }

    private static string ClassifyProjectionNode(SagaWeaverNode node)
    {
        if (node.Kind == "StringLiteral" &&
            !node.ReadOnly &&
            node.ProjectionCompatibility == "EditablePreviewOnly" &&
            node.Capability != "Deferred" &&
            node.SourceSpan is not null)
        {
            return "EditableByPatch";
        }
        if (node.ProjectionCompatibility == "Opaque" || node.Kind == "Opaque")
        {
            return "Opaque";
        }
        if (node.ProjectionCompatibility == "Unsupported" || node.Capability == "Unsupported")
        {
            return "Unsupported";
        }
        return "ReadOnlyProjectable";
    }

    private static string ClassificationReason(SagaWeaverNode node, string classification)
    {
        return classification switch
        {
            "EditableByPatch" => "String literal is eligible for ReplaceStringLiteral patch preview/apply.",
            "Opaque" => node.OpaqueReason ?? "Construct is preserved with source links but is not safely editable.",
            "Unsupported" => "Construct is outside the current Saga-compatible authoring profile.",
            _ when node.Capability == "Deferred" => "Deferred node is disclosed as read-only and is not runtime proof.",
            _ => "Construct is projectable for inspection but not source-mutable in this phase."
        };
    }

    private static string BuildVisualBlockId(CompatibilityConstruct construct)
    {
        var raw = string.IsNullOrWhiteSpace(construct.ConstructId)
            ? $"{construct.Kind}:{construct.SourceFile}:{construct.SourceSpan?.StartByte ?? 0}"
            : construct.ConstructId;
        return "visual-block://" + raw
            .Replace('\\', '/')
            .Replace(" ", "%20", StringComparison.Ordinal);
    }

    private static string BlockKindFor(CompatibilityConstruct construct)
    {
        return construct.Classification switch
        {
            "Opaque" => "OpaqueSourceRegionBlock",
            "Unsupported" or "Invalid" => "UnsupportedDiagnosticBlock",
            _ => construct.Kind switch
            {
                "SagaBehavior" => "ScriptClassBlock",
                "BehaviorMethod" => "CallableMethodBlock",
                "SagaNode" or "SagaLibrary" => "AttributeBlock",
                "StringLiteral" or "Literal" => "ParameterBlock",
                "ReturnStatement" => "ReturnBlock",
                _ => "CallableMethodBlock"
            }
        };
    }

    private static string DisplayNameFor(CompatibilityConstruct construct)
    {
        return construct.Kind switch
        {
            "SagaBehavior" => "Saga behavior",
            "BehaviorMethod" => "Callable method",
            "SagaNode" => "Saga node metadata",
            "SagaLibrary" => "Saga library metadata",
            "StringLiteral" => "String literal",
            "Literal" => "Literal",
            "ReturnStatement" => "Return statement",
            "OpaqueRegion" => "Opaque C# region",
            "UnsupportedConstruct" => "Unsupported C# construct",
            "InvalidMetadata" => "Invalid metadata",
            "InvalidSyntax" => "Invalid C# syntax",
            _ => construct.Kind
        };
    }

    private static string SourceHashFor(CompatibilityProfileReport profile, string sourceFile)
    {
        return profile.SourceHashes.TryGetValue(sourceFile, out var hash) ? hash : "";
    }

    private static string RuntimeProofFor(string capability)
    {
        return capability switch
        {
            "ProjectionOnly" => "NotRuntimeBacked",
            "Deferred" => "Deferred",
            "RuntimeBacked" => "RuntimeBackedWithEvidenceMissing",
            _ => "RuntimeBackedWithEvidenceMissing"
        };
    }

    private static string LibraryIdForNode(string displayName)
    {
        var nodeId = InvocationNodeIds.TryGetValue(displayName, out var mappedNodeId) ? mappedNodeId : "";
        if (nodeId.StartsWith("Gameplay.High.", StringComparison.Ordinal))
        {
            return "library://saga/gameplay/high";
        }
        if (nodeId.StartsWith("Gameplay.Low.", StringComparison.Ordinal))
        {
            return "library://saga/gameplay/low";
        }
        return "";
    }

    private static void AddExplicitCompatibilityScans(
        IReadOnlyList<string> sourceFiles,
        IReadOnlyDictionary<string, string> sourceTexts,
        List<SagaDiagnostic> diagnostics,
        List<CompatibilityConstruct> constructs)
    {
        var nodeIds = new List<(string Id, string SourceFile, SyntaxTree Tree, SyntaxNode SourceNode)>();
        foreach (var sourceFile in sourceFiles.Order(StringComparer.Ordinal))
        {
            var sourceText = sourceTexts.TryGetValue(sourceFile, out var cachedText)
                ? cachedText
                : File.ReadAllText(sourceFile, Encoding.UTF8);
            var tree = CSharpSyntaxTree.ParseText(sourceText, path: sourceFile);
            var root = tree.GetCompilationUnitRoot();
            var parseErrors = tree.GetDiagnostics()
                .Where(diagnostic => diagnostic.Severity == DiagnosticSeverity.Error)
                .ToArray();
            foreach (var parseError in parseErrors)
            {
                constructs.Add(new CompatibilityConstruct
                {
                    ConstructId = $"invalid-syntax:{sourceFile}:{parseError.Location.SourceSpan.Start}",
                    Kind = "InvalidSyntax",
                    Classification = "Invalid",
                    ApiDomain = "Unsupported",
                    ApiLevel = "Unsupported",
                    Capability = "Unsupported",
                    SourceFile = sourceFile,
                    SourceSpan = ToSpan(sourceText, tree, parseError.Location),
                    Reason = "C# syntax could not be parsed.",
                    Diagnostics =
                    [
                        new SagaDiagnostic
                        {
                            Severity = SagaDiagnosticSeverity.Error.ToString(),
                            Code = "Script.ParseFailed",
                            Message = parseError.ToString(),
                            SourceFile = sourceFile,
                            SourceRange = ToRange(tree, parseError.Location)
                        }
                    ]
                });
            }
            if (parseErrors.Length > 0)
            {
                continue;
            }

            foreach (var typeDeclaration in root.DescendantNodes().OfType<TypeDeclarationSyntax>())
            {
                var typeAttributes = AttributeMap.From(typeDeclaration.AttributeLists);
                var libraryMetadata = ReadNodeMetadata(typeAttributes, "SagaLibrary");
                NodeLibraryEntry? library = null;
                if (libraryMetadata is not null)
                {
                    var localDiagnostics = ValidateLibraryMetadata(libraryMetadata.Value, sourceFile, tree, typeDeclaration);
                    library = new NodeLibraryEntry
                    {
                        LibraryId = libraryMetadata.Value.Id,
                        ApiDomain = libraryMetadata.Value.Domain,
                        ApiLevel = libraryMetadata.Value.Level
                    };
                    constructs.Add(MetadataConstruct(
                        string.IsNullOrWhiteSpace(libraryMetadata.Value.Id)
                            ? $"library:{sourceFile}:{typeDeclaration.SpanStart}"
                            : libraryMetadata.Value.Id,
                        "SagaLibrary",
                        libraryMetadata.Value,
                        "ProjectionOnly",
                        sourceFile,
                        ToSpan(sourceText, tree, typeDeclaration.GetLocation()),
                        localDiagnostics));
                }

                foreach (var method in typeDeclaration.Members.OfType<MethodDeclarationSyntax>())
                {
                    var nodeMetadata = ReadNodeMetadata(AttributeMap.From(method.AttributeLists), "SagaNode");
                    if (nodeMetadata is not null)
                    {
                        var localDiagnostics = ValidateNodeMetadata(nodeMetadata.Value, library, sourceFile, tree, method);
                        nodeIds.Add((nodeMetadata.Value.Id, sourceFile, tree, method));
                        constructs.Add(MetadataConstruct(
                            string.IsNullOrWhiteSpace(nodeMetadata.Value.Id)
                                ? $"node:{sourceFile}:{method.SpanStart}"
                                : nodeMetadata.Value.Id,
                            "SagaNode",
                            nodeMetadata.Value,
                            ResolveCapability(nodeMetadata.Value),
                            sourceFile,
                            ToSpan(sourceText, tree, method.GetLocation()),
                            localDiagnostics));
                    }
                }
            }

            AddSyntaxProfileConstructs(sourceFile, sourceText, tree, root, constructs);
        }

        foreach (var duplicate in nodeIds
            .Where(node => !string.IsNullOrWhiteSpace(node.Id))
            .GroupBy(node => node.Id, StringComparer.Ordinal)
            .Where(group => group.Count() > 1)
            .OrderBy(group => group.Key, StringComparer.Ordinal))
        {
            var first = duplicate.First();
            diagnostics.Add(new SagaDiagnostic
            {
                Severity = SagaDiagnosticSeverity.Error.ToString(),
                Code = "Script.Node.DuplicateId",
                Message = $"Duplicate SagaNode id '{duplicate.Key}'.",
                ScriptId = duplicate.Key,
                SourceFile = first.SourceFile,
                SourceRange = ToRange(first.Tree, first.SourceNode.GetLocation())
            });
            constructs.Add(new CompatibilityConstruct
            {
                ConstructId = $"duplicate-node:{duplicate.Key}",
                Kind = "InvalidMetadata",
                Classification = "Invalid",
                ApiDomain = "Unsupported",
                ApiLevel = "Unsupported",
                Capability = "Unsupported",
                SourceFile = first.SourceFile,
                SourceSpan = ToSpan(sourceTexts[first.SourceFile], first.Tree, first.SourceNode.GetLocation()),
                Reason = $"Duplicate SagaNode id '{duplicate.Key}'."
            });
        }
    }

    private static CompatibilityConstruct MetadataConstruct(
        string constructId,
        string kind,
        SagaNodeMetadata metadata,
        string capability,
        string sourceFile,
        SourceSpan sourceSpan,
        IReadOnlyList<SagaDiagnostic> diagnostics)
    {
        var hasBlockingDiagnostics = diagnostics.Any(SagaScriptAnalyzer.IsBlocking);
        return new CompatibilityConstruct
        {
            ConstructId = constructId,
            Kind = hasBlockingDiagnostics ? "InvalidMetadata" : kind,
            Classification = hasBlockingDiagnostics ? "Invalid" : "ReadOnlyProjectable",
            ApiDomain = Domains.Contains(metadata.Domain) ? metadata.Domain : "Unsupported",
            ApiLevel = Levels.Contains(metadata.Level) ? metadata.Level : "Unsupported",
            Capability = Capabilities.Contains(capability) ? capability : "Unsupported",
            SourceFile = sourceFile,
            SourceSpan = sourceSpan,
            Editable = false,
            Reason = hasBlockingDiagnostics
                ? "Saga metadata is malformed or inconsistent."
                : $"{kind} metadata is projectable as read-only evidence.",
            Diagnostics = diagnostics
        };
    }

    private static void AddSyntaxProfileConstructs(
        string sourceFile,
        string sourceText,
        SyntaxTree tree,
        CompilationUnitSyntax root,
        List<CompatibilityConstruct> constructs)
    {
        foreach (var literal in root.DescendantNodes().OfType<LiteralExpressionSyntax>()
            .Where(literal => literal.IsKind(SyntaxKind.NumericLiteralExpression) ||
                literal.IsKind(SyntaxKind.TrueLiteralExpression) ||
                literal.IsKind(SyntaxKind.FalseLiteralExpression)))
        {
            AddSyntaxConstruct(constructs, sourceFile, sourceText, tree, literal, "Literal", "ReadOnlyProjectable", "ProjectionOnly", "Numeric and boolean literals are inspectable but not editable by patch.");
        }

        foreach (var syntax in root.DescendantNodes())
        {
            switch (syntax)
            {
                case LambdaExpressionSyntax:
                case AnonymousFunctionExpressionSyntax:
                case QueryExpressionSyntax:
                case AwaitExpressionSyntax:
                case SwitchExpressionSyntax:
                case SwitchStatementSyntax:
                case IsPatternExpressionSyntax:
                case LocalFunctionStatementSyntax:
                    AddSyntaxConstruct(constructs, sourceFile, sourceText, tree, syntax, "OpaqueRegion", "Opaque", "ProjectionOnly", "Advanced C# region is preserved with source links but not safely projected or edited.");
                    break;
                case ForStatementSyntax:
                case ForEachStatementSyntax:
                case WhileStatementSyntax:
                case DoStatementSyntax:
                case TryStatementSyntax:
                    AddSyntaxConstruct(constructs, sourceFile, sourceText, tree, syntax, "UnsupportedConstruct", "Unsupported", "Unsupported", "Control-flow construct is outside the current Saga-compatible authoring profile.");
                    break;
                case MethodDeclarationSyntax method when method.Modifiers.Any(SyntaxKind.AsyncKeyword):
                    AddSyntaxConstruct(constructs, sourceFile, sourceText, tree, method, "OpaqueRegion", "Opaque", "ProjectionOnly", "Async methods are preserved with source links but not safely projected or edited.");
                    break;
                case InvocationExpressionSyntax invocation when !InvocationNodeIds.ContainsKey(invocation.Expression.ToString()):
                    AddSyntaxConstruct(constructs, sourceFile, sourceText, tree, invocation, "OpaqueRegion", "Opaque", "ProjectionOnly", "Unknown invocation shape is preserved with source links but not safely projected or edited.");
                    break;
            }
        }
    }

    private static void AddSyntaxConstruct(
        List<CompatibilityConstruct> constructs,
        string sourceFile,
        string sourceText,
        SyntaxTree tree,
        SyntaxNode syntax,
        string kind,
        string classification,
        string capability,
        string reason)
    {
        constructs.Add(new CompatibilityConstruct
        {
            ConstructId = $"{kind}:{sourceFile}:{syntax.SpanStart}",
            Kind = kind,
            Classification = classification,
            ApiDomain = "Unsupported",
            ApiLevel = "Unsupported",
            Capability = capability,
            SourceFile = sourceFile,
            SourceSpan = ToSpan(sourceText, tree, syntax.GetLocation()),
            Editable = false,
            Reason = reason
        });
    }

    private static IReadOnlyDictionary<string, int> BuildCompatibilitySummary(
        IReadOnlyList<CompatibilityConstruct> constructs,
        IReadOnlyList<SagaDiagnostic> diagnostics)
    {
        var summary = new Dictionary<string, int>(StringComparer.Ordinal)
        {
            ["constructCount"] = constructs.Count,
            ["diagnosticCount"] = diagnostics.Count,
            ["blockingDiagnosticCount"] = diagnostics.Count(SagaScriptAnalyzer.IsBlocking)
        };
        foreach (var group in constructs.GroupBy(construct => construct.Classification, StringComparer.Ordinal))
        {
            summary[$"{char.ToLowerInvariant(group.Key[0])}{group.Key[1..]}Count"] = group.Count();
        }
        return summary;
    }

    private static (CheckedScriptArtifact Check, JsonObject? Json, string Path) CheckArtifact(
        string root,
        string kind,
        string fileName,
        bool required,
        List<SagaDiagnostic> diagnostics)
    {
        var path = Path.Combine(root, fileName);
        if (!File.Exists(path))
        {
            if (required)
            {
                diagnostics.Add(Error("Script.Artifact.Missing", $"Required artifact is missing: {fileName}.", path));
            }
            return (new CheckedScriptArtifact
            {
                Kind = kind,
                Path = path,
                Status = required ? "Missing" : "NotApplicable"
            }, null, path);
        }

        try
        {
            var json = JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8)) as JsonObject;
            if (json is null)
            {
                diagnostics.Add(Error("Script.Artifact.InvalidJson", $"{fileName} must be a JSON object.", path));
                return (new CheckedScriptArtifact { Kind = kind, Path = path, Status = "Failed" }, null, path);
            }
            ValidateEnvelope(json, fileName, path, diagnostics);
            ValidateSourceHashes(json, fileName, path, diagnostics);
            var status = ReadString(json, "status") == "Failed" ? "Failed" : "Passed";
            return (new CheckedScriptArtifact { Kind = kind, Path = path, Status = status }, json, path);
        }
        catch (Exception ex) when (ex is IOException or System.Text.Json.JsonException)
        {
            diagnostics.Add(Error("Script.Artifact.InvalidJson", $"Unable to read {fileName}: {ex.Message}", path));
            return (new CheckedScriptArtifact { Kind = kind, Path = path, Status = "Failed" }, null, path);
        }
    }

    private static void ValidateEnvelope(JsonObject json, string fileName, string path, List<SagaDiagnostic> diagnostics)
    {
        foreach (var required in new[] { "schemaVersion", "tool", "command", "status" })
        {
            if (!json.ContainsKey(required))
            {
                diagnostics.Add(Error("Script.Artifact.MissingEnvelopeField", $"{fileName} is missing envelope field '{required}'.", path));
            }
        }
    }

    private static void ValidateSourceHashes(JsonObject json, string fileName, string path, List<SagaDiagnostic> diagnostics)
    {
        if (json.ContainsKey("sourceFile") && string.IsNullOrWhiteSpace(ReadString(json, "sourceHash")) &&
            string.IsNullOrWhiteSpace(ReadString(json, "baseSourceHash")))
        {
            diagnostics.Add(Error("Script.Artifact.SourceHashMissing", $"{fileName} exposes a source file without a source hash.", path));
        }
        if (json["sourceFiles"] is JsonArray sourceFiles)
        {
            foreach (var sourceFile in sourceFiles.OfType<JsonObject>())
            {
                if (string.IsNullOrWhiteSpace(ReadString(sourceFile, "sourceHash")))
                {
                    diagnostics.Add(Error("Script.Artifact.SourceHashMissing", $"{fileName} sourceFiles entry is missing sourceHash.", path));
                }
            }
        }
    }

    private static void ValidateNodeCapabilityConsistency(JsonObject nodeMetadata, JsonObject runtimeBindings, List<SagaDiagnostic> diagnostics)
    {
        var metadata = (nodeMetadata["nodes"] as JsonArray)?.OfType<JsonObject>()
            .Where(node => !string.IsNullOrWhiteSpace(ReadString(node, "nodeId")))
            .ToDictionary(node => ReadString(node, "nodeId"), node => ReadString(node, "capability"), StringComparer.Ordinal)
            ?? new Dictionary<string, string>(StringComparer.Ordinal);
        var bindings = runtimeBindings["bindings"] as JsonArray;
        if (bindings is null)
        {
            return;
        }
        foreach (var binding in bindings.OfType<JsonObject>())
        {
            var nodes = binding["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }
            foreach (var node in nodes.OfType<JsonObject>())
            {
                var nodeId = ReadString(node, "nodeId");
                if (metadata.TryGetValue(nodeId, out var expectedCapability) &&
                    expectedCapability != ReadString(node, "capability"))
                {
                    diagnostics.Add(Error("Script.Artifact.NodeCapabilityMismatch", $"Runtime binding node '{nodeId}' capability does not match node metadata."));
                }
            }
        }
    }

    private static RuntimeProofSummary SummarizeRuntimeProof(JsonObject? runtimeBindings, List<SagaDiagnostic> diagnostics, string path)
    {
        var runtimeBackedWithEvidence = 0;
        var runtimeBackedMissingEvidence = 0;
        var projectionOnly = 0;
        var deferred = 0;
        var bindings = runtimeBindings?["bindings"] as JsonArray;
        if (bindings is not null)
        {
            foreach (var binding in bindings.OfType<JsonObject>())
            {
                var nodes = binding["nodes"] as JsonArray;
                if (nodes is null)
                {
                    continue;
                }
                foreach (var node in nodes.OfType<JsonObject>())
                {
                    switch (ReadString(node, "runtimeProof"))
                    {
                        case "RuntimeBackedWithEvidence":
                            runtimeBackedWithEvidence++;
                            break;
                        case "RuntimeBackedWithEvidenceMissing":
                            runtimeBackedMissingEvidence++;
                            diagnostics.Add(Error("Script.Artifact.RuntimeProofMissing", "RuntimeBacked node is missing explicit runtime evidence.", path));
                            break;
                        case "Deferred":
                            deferred++;
                            break;
                        default:
                            projectionOnly++;
                            break;
                    }
                }
            }
        }
        return new RuntimeProofSummary
        {
            RuntimeBackedWithEvidence = runtimeBackedWithEvidence,
            RuntimeBackedMissingEvidence = runtimeBackedMissingEvidence,
            ProjectionOnly = projectionOnly,
            Deferred = deferred
        };
    }

    private static SagaWeaverBehavior BuildBehavior(
        string sourceFile,
        string sourceText,
        string sourceHash,
        SyntaxTree tree,
        TypeDeclarationSyntax typeDeclaration,
        MethodDeclarationSyntax? method,
        SagaMetadata metadata,
        IReadOnlyList<string> namespaceUsages,
        List<SagaDiagnostic> diagnostics,
        bool strict)
    {
        var sourceNode = (SyntaxNode?)method ?? typeDeclaration;
        var declaringType = GetDeclaringType(typeDeclaration);
        var methodName = method?.Identifier.ValueText ?? typeDeclaration.Members
            .OfType<MethodDeclarationSyntax>()
            .FirstOrDefault()?.Identifier.ValueText ?? "";
        var behaviorId = string.IsNullOrWhiteSpace(metadata.Id)
            ? $"behavior://missing/{declaringType}.{methodName}".TrimEnd('.')
            : metadata.Id;
        var compatibility = "Supported";

        if (!Levels.Contains(metadata.Level) || !Domains.Contains(metadata.Domain))
        {
            diagnostics.Add(new SagaDiagnostic
            {
                Severity = SagaDiagnosticSeverity.Error.ToString(),
                Code = "Script.Metadata.InvalidBehavior",
                Message = "SagaBehavior requires valid Level and Domain metadata.",
                ScriptId = behaviorId,
                SourceFile = sourceFile,
                SourceRange = ToRange(tree, sourceNode.GetLocation())
            });
            compatibility = "Unsupported";
        }

        foreach (var usage in namespaceUsages)
        {
            var usageMetadata = ParseNamespaceUsage(usage);
            if (usageMetadata is null)
            {
                continue;
            }
            if (usageMetadata.Value.Level != metadata.Level)
            {
                diagnostics.Add(new SagaDiagnostic
                {
                    Severity = SagaDiagnosticSeverity.Error.ToString(),
                    Code = "Script.Metadata.MixedApiLevel",
                    Message = $"SagaBehavior level '{metadata.Level}' conflicts with namespace usage '{usage}'.",
                    ScriptId = behaviorId,
                    SourceFile = sourceFile,
                    SourceRange = ToRange(tree, sourceNode.GetLocation())
                });
                compatibility = "Mixed";
            }
            if (usageMetadata.Value.Domain != metadata.Domain)
            {
                diagnostics.Add(new SagaDiagnostic
                {
                    Severity = SagaDiagnosticSeverity.Error.ToString(),
                    Code = "Script.Metadata.MixedApiDomain",
                    Message = $"SagaBehavior domain '{metadata.Domain}' conflicts with namespace usage '{usage}'.",
                    ScriptId = behaviorId,
                    SourceFile = sourceFile,
                    SourceRange = ToRange(tree, sourceNode.GetLocation())
                });
                compatibility = "Mixed";
            }
        }

        if (strict && metadata.Domain is not "Gameplay" and not "Server" and not "Diagnostics")
        {
            diagnostics.Add(new SagaDiagnostic
            {
                Severity = SagaDiagnosticSeverity.Warning.ToString(),
                Code = "Script.UnsupportedConstruct",
                Message = $"Domain '{metadata.Domain}' is schema-defined but not implemented by the Block F MVP projection.",
                ScriptId = behaviorId,
                SourceFile = sourceFile,
                SourceRange = ToRange(tree, sourceNode.GetLocation())
            });
        }

        var nodes = BuildNodes(sourceText, tree, sourceNode, behaviorId, metadata, compatibility);
        return new SagaWeaverBehavior
        {
            BehaviorId = behaviorId,
            ApiLevel = metadata.Level,
            ApiDomain = metadata.Domain,
            Compatibility = compatibility,
            DeclaringType = declaringType,
            MethodName = methodName,
            SourceFile = sourceFile,
            SourceHash = sourceHash,
            SourceSpan = ToSpan(sourceText, tree, sourceNode.GetLocation()),
            NamespaceUsages = namespaceUsages,
            Nodes = nodes
        };
    }

    private static IReadOnlyList<SagaWeaverNode> BuildNodes(
        string sourceText,
        SyntaxTree tree,
        SyntaxNode sourceNode,
        string behaviorId,
        SagaMetadata metadata,
        string compatibility)
    {
        var nodes = new List<SagaWeaverNode>();
        var index = 0;
        var simpleViewAdvanced = metadata.Level != "High" || metadata.Domain != "Gameplay";
        var simpleViewDisclosure = simpleViewAdvanced
            ? "Advanced gameplay node is disclosed as read-only for Simple View."
            : null;
        nodes.Add(new SagaWeaverNode
        {
            NodeId = $"{behaviorId}::event:{index++}",
            Kind = "Event",
            DisplayName = sourceNode is MethodDeclarationSyntax method ? method.Identifier.ValueText : "Behavior",
            ApiLevel = metadata.Level,
            ApiDomain = metadata.Domain,
            Capability = "ProjectionOnly",
            ProjectionCompatibility = "ReadOnly",
            ReadOnly = true,
            SourceSpan = ToSpan(sourceText, tree, sourceNode.GetLocation()),
            OpaqueReason = simpleViewDisclosure
        });

        foreach (var ifStatement in sourceNode.DescendantNodes().OfType<IfStatementSyntax>())
        {
            nodes.Add(new SagaWeaverNode
            {
                NodeId = $"{behaviorId}::if:{index++}",
                Kind = "If",
                DisplayName = "If",
                ApiLevel = metadata.Level,
                ApiDomain = metadata.Domain,
                Capability = "ProjectionOnly",
                ProjectionCompatibility = compatibility == "Supported" ? "ReadOnly" : "Opaque",
                ReadOnly = true,
                SourceSpan = ToSpan(sourceText, tree, ifStatement.Condition.GetLocation()),
                OpaqueReason = simpleViewDisclosure
            });
        }

        foreach (var invocation in sourceNode.DescendantNodes().OfType<InvocationExpressionSyntax>())
        {
            var policyNodeId = InvocationNodeIds.TryGetValue(invocation.Expression.ToString(), out var mappedNodeId)
                ? mappedNodeId
                : "";
            var capability = ResolvePolicyCapability(policyNodeId);
            var projectionCompatibility = capability == "Deferred"
                ? "Deferred"
                : compatibility == "Supported" ? "ReadOnly" : "Opaque";
            nodes.Add(new SagaWeaverNode
            {
                NodeId = $"{behaviorId}::call:{index++}",
                Kind = metadata.Level == "High" ? "Call" : "Operation",
                DisplayName = invocation.Expression.ToString(),
                ApiLevel = metadata.Level,
                ApiDomain = metadata.Domain,
                Capability = capability,
                ProjectionCompatibility = projectionCompatibility,
                ReadOnly = true,
                SourceSpan = ToSpan(sourceText, tree, invocation.GetLocation()),
                OpaqueReason = capability == "Deferred" ? "Deferred gameplay node is disclosed as read-only." : simpleViewDisclosure
            });
        }

        foreach (var literal in sourceNode.DescendantNodes().OfType<LiteralExpressionSyntax>()
                     .Where(literal => literal.IsKind(SyntaxKind.StringLiteralExpression)))
        {
            nodes.Add(new SagaWeaverNode
            {
                NodeId = $"{behaviorId}::literal:{index++}",
                Kind = "StringLiteral",
                DisplayName = literal.Token.ValueText,
                ApiLevel = metadata.Level,
                ApiDomain = metadata.Domain,
                Capability = "ProjectionOnly",
                ProjectionCompatibility = compatibility == "Supported" ? "EditablePreviewOnly" : "Opaque",
                ReadOnly = simpleViewAdvanced || compatibility != "Supported",
                SourceSpan = ToSpan(sourceText, tree, literal.GetLocation()),
                LiteralValue = literal.Token.ValueText,
                OpaqueReason = simpleViewDisclosure ?? (compatibility == "Supported" ? null : "Mixed or unsupported behavior metadata.")
            });
        }

        foreach (var unsupported in sourceNode.DescendantNodes().Where(node =>
                     node is LambdaExpressionSyntax or QueryExpressionSyntax or AnonymousFunctionExpressionSyntax))
        {
            nodes.Add(new SagaWeaverNode
            {
                NodeId = $"{behaviorId}::opaque:{index++}",
                Kind = "Opaque",
                DisplayName = unsupported.Kind().ToString(),
                ApiLevel = metadata.Level,
                ApiDomain = metadata.Domain,
                Capability = "ProjectionOnly",
                ProjectionCompatibility = "Opaque",
                ReadOnly = true,
                SourceSpan = ToSpan(sourceText, tree, unsupported.GetLocation()),
                OpaqueReason = "Unsupported C# region is projected as read-only."
            });
        }

        return nodes
            .OrderBy(node => node.SourceSpan?.StartByte ?? 0)
            .ThenBy(node => node.NodeId, StringComparer.Ordinal)
            .ToArray();
    }

    private static IReadOnlyList<string> FindSagaNamespaceUsages(CompilationUnitSyntax root)
    {
        return root.Usings
            .Select(usingDirective => usingDirective.Name?.ToString() ?? "")
            .Where(name => name.StartsWith("Saga.", StringComparison.Ordinal))
            .Distinct(StringComparer.Ordinal)
            .Order(StringComparer.Ordinal)
            .ToArray();
    }

    private static SagaMetadata? ReadMetadata(AttributeMap attributes, string attributeName)
    {
        if (!attributes.Has(attributeName))
        {
            return null;
        }

        return new SagaMetadata(
            attributes.NamedEnum(attributeName, "Level") ?? "Unsupported",
            attributes.NamedEnum(attributeName, "Domain") ?? "Unsupported",
            attributes.NamedString(attributeName, "Id") ?? attributes.FirstString(attributeName) ?? "");
    }

    private static SagaNodeMetadata? ReadNodeMetadata(AttributeMap attributes, string attributeName)
    {
        if (!attributes.Has(attributeName))
        {
            return null;
        }

        return new SagaNodeMetadata(
            attributes.NamedEnum(attributeName, "Level") ?? "Unsupported",
            attributes.NamedEnum(attributeName, "Domain") ?? "Unsupported",
            attributes.NamedString(attributeName, "Id") ?? attributes.FirstString(attributeName) ?? "",
            attributes.NamedString(attributeName, "DisplayName") ?? "",
            attributes.NamedString(attributeName, "Capability") ?? attributes.NamedEnum(attributeName, "Capability") ?? "");
    }

    private static IReadOnlyList<SagaDiagnostic> ValidateLibraryMetadata(
        SagaNodeMetadata metadata,
        string sourceFile,
        SyntaxTree tree,
        SyntaxNode sourceNode)
    {
        var diagnostics = new List<SagaDiagnostic>();
        if (string.IsNullOrWhiteSpace(metadata.Id))
        {
            diagnostics.Add(MetadataError(
                "Script.NodeLibrary.MissingLibraryId",
                "SagaLibrary requires an Id.",
                sourceFile,
                tree,
                sourceNode));
        }
        ValidateAxes(metadata, "Script.NodeLibrary.InvalidMetadata", "SagaLibrary", sourceFile, tree, sourceNode, diagnostics);
        return diagnostics;
    }

    private static IReadOnlyList<SagaDiagnostic> ValidateNodeMetadata(
        SagaNodeMetadata metadata,
        NodeLibraryEntry? library,
        string sourceFile,
        SyntaxTree tree,
        SyntaxNode sourceNode)
    {
        var diagnostics = new List<SagaDiagnostic>();
        if (string.IsNullOrWhiteSpace(metadata.Id))
        {
            diagnostics.Add(MetadataError(
                "Script.Node.MissingNodeId",
                "SagaNode requires an Id.",
                sourceFile,
                tree,
                sourceNode));
        }
        ValidateAxes(metadata, "Script.Node.InvalidMetadata", "SagaNode", sourceFile, tree, sourceNode, diagnostics);
        if (library is not null && library.ApiDomain != metadata.Domain)
        {
            diagnostics.Add(MetadataError(
                "Script.Node.MixedApiDomain",
                $"SagaNode domain '{metadata.Domain}' conflicts with SagaLibrary domain '{library.ApiDomain}'.",
                sourceFile,
                tree,
                sourceNode));
        }
        if (library is not null && library.ApiLevel != metadata.Level)
        {
            diagnostics.Add(MetadataError(
                "Script.Node.MixedApiLevel",
                $"SagaNode level '{metadata.Level}' conflicts with SagaLibrary level '{library.ApiLevel}'.",
                sourceFile,
                tree,
                sourceNode));
        }

        var capability = ResolveCapability(metadata);
        if (!Capabilities.Contains(capability))
        {
            diagnostics.Add(MetadataError(
                "Script.Node.InvalidCapability",
                $"SagaNode capability '{capability}' is not supported.",
                sourceFile,
                tree,
                sourceNode));
        }
        if (capability == "RuntimeBacked")
        {
            diagnostics.Add(MetadataError(
                "Script.Node.RuntimeProofMissing",
                "RuntimeBacked nodes require focused runtime proof; metadata alone is not runtime behavior proof.",
                sourceFile,
                tree,
                sourceNode));
        }
        if (PolicyNodeCapabilities.TryGetValue(metadata.Id, out var policyCapability) &&
            capability != policyCapability)
        {
            diagnostics.Add(MetadataError(
                "Script.Node.PolicyCapabilityMismatch",
                $"SagaNode capability '{capability}' conflicts with policy capability '{policyCapability}'.",
                sourceFile,
                tree,
                sourceNode));
        }

        return diagnostics;
    }

    private static void ValidateAxes(
        SagaNodeMetadata metadata,
        string code,
        string label,
        string sourceFile,
        SyntaxTree tree,
        SyntaxNode sourceNode,
        List<SagaDiagnostic> diagnostics)
    {
        if (!Levels.Contains(metadata.Level) || !Domains.Contains(metadata.Domain))
        {
            diagnostics.Add(MetadataError(
                code,
                $"{label} requires valid Level and Domain metadata.",
                sourceFile,
                tree,
                sourceNode));
        }
    }

    private static SagaDiagnostic MetadataError(
        string code,
        string message,
        string sourceFile,
        SyntaxTree tree,
        SyntaxNode sourceNode)
    {
        return new SagaDiagnostic
        {
            Severity = SagaDiagnosticSeverity.Error.ToString(),
            Code = code,
            Message = message,
            SourceFile = sourceFile,
            SourceRange = ToRange(tree, sourceNode.GetLocation())
        };
    }

    private static string ResolveCapability(SagaNodeMetadata metadata)
    {
        if (!string.IsNullOrWhiteSpace(metadata.Capability))
        {
            return metadata.Capability;
        }
        return ResolvePolicyCapability(metadata.Id);
    }

    private static string ResolvePolicyCapability(string nodeId)
    {
        return PolicyNodeCapabilities.TryGetValue(nodeId, out var capability)
            ? capability
            : "ProjectionOnly";
    }

    private static string DisplayNameOrFallback(string displayName, string fallback)
    {
        return string.IsNullOrWhiteSpace(displayName) ? fallback : displayName;
    }

    private static (string Domain, string Level)? ParseNamespaceUsage(string usage)
    {
        var parts = usage.Split('.', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 3 || parts[0] != "Saga")
        {
            return null;
        }
        var level = parts[2] switch
        {
            "HighLevel" => "High",
            "LowLevel" => "Low",
            _ => ""
        };
        return Domains.Contains(parts[1]) && Levels.Contains(level)
            ? (parts[1], level)
            : null;
    }

    private static string GetDeclaringType(TypeDeclarationSyntax typeDeclaration)
    {
        var names = new Stack<string>();
        SyntaxNode? current = typeDeclaration;
        while (current is not null)
        {
            switch (current)
            {
                case TypeDeclarationSyntax type:
                    names.Push(type.Identifier.ValueText);
                    break;
                case NamespaceDeclarationSyntax ns:
                    names.Push(ns.Name.ToString());
                    break;
                case FileScopedNamespaceDeclarationSyntax fileNs:
                    names.Push(fileNs.Name.ToString());
                    break;
            }
            current = current.Parent;
        }
        return string.Join('.', names);
    }

    private static SourceSpan ToSpan(string sourceText, SyntaxTree tree, Location location)
    {
        var span = tree.GetLineSpan(location.SourceSpan);
        return new SourceSpan(
            span.StartLinePosition.Line + 1,
            span.StartLinePosition.Character + 1,
            span.EndLinePosition.Line + 1,
            span.EndLinePosition.Character + 1,
            ByteOffset(sourceText, location.SourceSpan.Start),
            ByteOffset(sourceText, location.SourceSpan.End));
    }

    private static SourceRange? ToRange(SyntaxTree tree, Location location)
    {
        if (!location.IsInSource)
        {
            return null;
        }
        var span = tree.GetLineSpan(location.SourceSpan);
        return new SourceRange(
            span.StartLinePosition.Line + 1,
            span.StartLinePosition.Character + 1,
            span.EndLinePosition.Line + 1,
            span.EndLinePosition.Character + 1);
    }

    private static int ByteOffset(string text, int charOffset)
    {
        return Encoding.UTF8.GetByteCount(text.AsSpan(0, Math.Clamp(charOffset, 0, text.Length)));
    }

    private static string ComputeHash(string text)
    {
        return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(text))).ToLowerInvariant();
    }

    private static IEnumerable<SagaDiagnostic> SortDiagnostics(IEnumerable<SagaDiagnostic> diagnostics)
    {
        return diagnostics
            .OrderByDescending(diagnostic => SagaScriptAnalyzer.IsBlocking(diagnostic))
            .ThenBy(diagnostic => diagnostic.Code, StringComparer.Ordinal)
            .ThenBy(diagnostic => diagnostic.SourceFile, StringComparer.Ordinal)
            .ThenBy(diagnostic => diagnostic.Message, StringComparer.Ordinal);
    }

    private static string SummarizeAxis(IEnumerable<string> values)
    {
        var distinct = values
            .Where(value => !string.IsNullOrWhiteSpace(value))
            .Distinct(StringComparer.Ordinal)
            .Order(StringComparer.Ordinal)
            .ToArray();
        return distinct.Length switch
        {
            0 => "Unsupported",
            1 => distinct[0],
            _ => "Mixed"
        };
    }

    private static SagaDiagnostic Error(string code, string message)
    {
        return new SagaDiagnostic
        {
            Severity = SagaDiagnosticSeverity.Error.ToString(),
            Code = code,
            Message = message
        };
    }

    private static SagaDiagnostic Error(string code, string message, string? sourceFile)
    {
        return new SagaDiagnostic
        {
            Severity = SagaDiagnosticSeverity.Error.ToString(),
            Code = code,
            Message = message,
            SourceFile = string.IsNullOrWhiteSpace(sourceFile) ? null : sourceFile
        };
    }

    private static JsonObject? ReadJsonObjectFile(string path, string label, List<SagaDiagnostic> diagnostics)
    {
        try
        {
            if (!File.Exists(path))
            {
                diagnostics.Add(Error("Script.Patch.InputMissing", $"Required {label} file does not exist: {path}."));
                return null;
            }

            return JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8)) as JsonObject
                ?? throw new InvalidOperationException($"{label} must be a JSON object.");
        }
        catch (Exception ex) when (ex is IOException or InvalidOperationException or System.Text.Json.JsonException)
        {
            diagnostics.Add(Error("Script.Patch.InputInvalid", $"Unable to read {label}: {ex.Message}"));
            return null;
        }
    }

    private static bool HasStringField(JsonObject obj, string key)
    {
        return obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<string>(out _);
    }

    private static string BuildOperationId(
        string operation,
        string nodeId,
        string baseSourceHash,
        string expectedOldText,
        string replacement)
    {
        var material = string.Join('\n', operation, nodeId, baseSourceHash, expectedOldText, replacement);
        return ComputeHash(material)[..16];
    }

    private static IEnumerable<(JsonObject Node, string SourceFile)> FindNodeMatches(JsonObject sourceMap, string nodeId)
    {
        var behaviors = sourceMap["behaviors"] as JsonArray;
        if (behaviors is null)
        {
            yield break;
        }

        foreach (var behavior in behaviors.OfType<JsonObject>())
        {
            var sourceFile = ReadString(behavior, "sourceFile");
            var nodes = behavior["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }

            foreach (var node in nodes.OfType<JsonObject>())
            {
                if (ReadString(node, "nodeId") == nodeId)
                {
                    yield return (node, sourceFile);
                }
            }
        }
    }

    private static void ValidatePatchNode(JsonObject node, List<SagaDiagnostic> diagnostics)
    {
        if (ReadString(node, "kind") != "StringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedNode", "ReplaceStringLiteral requires a StringLiteral source-map node."));
        }
        if (ReadBool(node, "readOnly"))
        {
            diagnostics.Add(Error("Script.Patch.ReadOnlyNode", "Patch target is read-only."));
        }
        if (ReadString(node, "capability") == "Deferred")
        {
            diagnostics.Add(Error("Script.Patch.DeferredNode", "Deferred patch targets are not editable."));
        }

        var projectionCompatibility = ReadString(node, "projectionCompatibility");
        if (projectionCompatibility != "EditablePreviewOnly")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedProjectionCompatibility", $"Patch target projection compatibility is '{projectionCompatibility}'."));
        }
        if (!string.IsNullOrWhiteSpace(ReadString(node, "opaqueReason")))
        {
            diagnostics.Add(Error("Script.Patch.OpaqueNode", "Opaque patch targets are not editable."));
        }
    }

    private static string ResolvePatchSourceFile(
        IReadOnlyList<string> sourceFiles,
        IReadOnlyList<string> sourceInputs,
        string sourceFile)
    {
        var fullSource = Path.GetFullPath(sourceFile);
        var discovered = sourceFiles
            .Select(Path.GetFullPath)
            .FirstOrDefault(path => string.Equals(path, fullSource, StringComparison.Ordinal));
        if (!string.IsNullOrWhiteSpace(discovered))
        {
            return discovered;
        }

        return IsUnderAnySourceInput(fullSource, sourceInputs) ? fullSource : "";
    }

    private static bool IsUnderAnySourceInput(string fullSource, IReadOnlyList<string> sourceInputs)
    {
        foreach (var input in sourceInputs)
        {
            var fullInput = Path.GetFullPath(input);
            if (File.Exists(fullInput))
            {
                if (string.Equals(fullInput, fullSource, StringComparison.Ordinal))
                {
                    return true;
                }
                continue;
            }

            var directory = Directory.Exists(fullInput)
                ? fullInput
                : fullInput;
            var normalized = Path.TrimEndingDirectorySeparator(directory) + Path.DirectorySeparatorChar;
            if (fullSource.StartsWith(normalized, StringComparison.Ordinal))
            {
                return true;
            }
        }

        return false;
    }

    private static bool IsValidByteSpan(SourceSpan span, int byteLength)
    {
        return span.StartByte >= 0 &&
            span.EndByte >= span.StartByte &&
            span.EndByte <= byteLength;
    }

    private static string DecodeUtf8(byte[] bytes, int start, int count)
    {
        return new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true)
            .GetString(bytes, start, count);
    }

    private static bool IsQuotedStringLiteral(string text)
    {
        if (!text.StartsWith("\"", StringComparison.Ordinal) ||
            !text.EndsWith("\"", StringComparison.Ordinal))
        {
            return false;
        }

        var tree = CSharpSyntaxTree.ParseText("class C { void M() { _ = " + text + "; } }");
        if (tree.GetDiagnostics().Any(diagnostic => diagnostic.Severity == DiagnosticSeverity.Error))
        {
            return false;
        }

        var literal = tree.GetCompilationUnitRoot()
            .DescendantNodes()
            .OfType<LiteralExpressionSyntax>()
            .SingleOrDefault(node => node.IsKind(SyntaxKind.StringLiteralExpression));
        return literal is not null && literal.ToString() == text;
    }

    private static byte[] ReplaceBytes(byte[] originalBytes, int startByte, int endByte, byte[] replacementBytes)
    {
        var prefixLength = startByte;
        var suffixLength = originalBytes.Length - endByte;
        var result = new byte[prefixLength + replacementBytes.Length + suffixLength];
        Buffer.BlockCopy(originalBytes, 0, result, 0, prefixLength);
        Buffer.BlockCopy(replacementBytes, 0, result, prefixLength, replacementBytes.Length);
        Buffer.BlockCopy(originalBytes, endByte, result, prefixLength + replacementBytes.Length, suffixLength);
        return result;
    }

    private static bool VerifySingleSpanReplacement(
        byte[] originalBytes,
        byte[] afterBytes,
        int startByte,
        int endByte,
        byte[] replacementBytes)
    {
        var expectedLength = originalBytes.Length - (endByte - startByte) + replacementBytes.Length;
        if (afterBytes.Length != expectedLength)
        {
            return false;
        }

        return originalBytes.AsSpan(0, startByte).SequenceEqual(afterBytes.AsSpan(0, startByte)) &&
            replacementBytes.AsSpan().SequenceEqual(afterBytes.AsSpan(startByte, replacementBytes.Length)) &&
            originalBytes.AsSpan(endByte).SequenceEqual(afterBytes.AsSpan(startByte + replacementBytes.Length));
    }

    private static bool RestoreBackup(string backupPath, string sourceFile)
    {
        try
        {
            if (!File.Exists(backupPath))
            {
                return false;
            }

            File.Copy(backupPath, sourceFile, overwrite: true);
            return true;
        }
        catch (IOException)
        {
            return false;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
    }

    private static bool SourceBytesChanged(string sourceFile, byte[] originalBytes)
    {
        try
        {
            return File.Exists(sourceFile) && !File.ReadAllBytes(sourceFile).SequenceEqual(originalBytes);
        }
        catch (IOException)
        {
            return true;
        }
        catch (UnauthorizedAccessException)
        {
            return true;
        }
    }

    private static void TryDeleteFile(string path)
    {
        try
        {
            if (!string.IsNullOrWhiteSpace(path) && File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static JsonObject SpanJson(SourceSpan span)
    {
        return new JsonObject
        {
            ["startLine"] = span.StartLine,
            ["startColumn"] = span.StartColumn,
            ["endLine"] = span.EndLine,
            ["endColumn"] = span.EndColumn,
            ["startByte"] = span.StartByte,
            ["endByte"] = span.EndByte
        };
    }

    private static JsonArray StaleArtifactArray()
    {
        var array = new JsonArray();
        array.Add("source_map.json");
        array.Add("projection_report.json");
        array.Add("node_metadata.json");
        array.Add("runtime_bindings.json");
        return array;
    }

    private static JsonObject BuildUnifiedDiff(string oldText, string newText)
    {
        var oldLines = NormalizeDiffLines(oldText);
        var newLines = NormalizeDiffLines(newText);
        var prefix = 0;
        while (prefix < oldLines.Length &&
               prefix < newLines.Length &&
               string.Equals(oldLines[prefix], newLines[prefix], StringComparison.Ordinal))
        {
            prefix++;
        }

        var suffix = 0;
        while (suffix < oldLines.Length - prefix &&
               suffix < newLines.Length - prefix &&
               string.Equals(oldLines[oldLines.Length - 1 - suffix], newLines[newLines.Length - 1 - suffix], StringComparison.Ordinal))
        {
            suffix++;
        }

        var hunkOldStart = Math.Max(0, prefix - 3);
        var hunkNewStart = Math.Max(0, prefix - 3);
        var oldChangeEnd = oldLines.Length - suffix;
        var newChangeEnd = newLines.Length - suffix;
        var hunkOldEnd = Math.Min(oldLines.Length, oldChangeEnd + 3);
        var hunkNewEnd = Math.Min(newLines.Length, newChangeEnd + 3);
        var lines = new JsonArray();

        for (var i = hunkOldStart; i < prefix; i++)
        {
            lines.Add(" " + oldLines[i]);
        }
        for (var i = prefix; i < oldChangeEnd; i++)
        {
            lines.Add("-" + oldLines[i]);
        }
        for (var i = prefix; i < newChangeEnd; i++)
        {
            lines.Add("+" + newLines[i]);
        }
        for (var i = oldChangeEnd; i < hunkOldEnd; i++)
        {
            lines.Add(" " + oldLines[i]);
        }

        var hunks = new JsonArray
        {
            new JsonObject
            {
                ["oldStartLine"] = hunkOldStart + 1,
                ["oldLineCount"] = hunkOldEnd - hunkOldStart,
                ["newStartLine"] = hunkNewStart + 1,
                ["newLineCount"] = hunkNewEnd - hunkNewStart,
                ["lines"] = lines
            }
        };

        return new JsonObject
        {
            ["format"] = "unified",
            ["oldLabel"] = "source@base",
            ["newLabel"] = "source@proposed",
            ["hunks"] = hunks
        };
    }

    private static string[] NormalizeDiffLines(string text)
    {
        return text.Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n')
            .Split('\n');
    }

    private static void ReplaceFileWithBytes(string sourceFile, string tempPath, byte[] bytes)
    {
        File.WriteAllBytes(tempPath, bytes);
        File.Move(tempPath, sourceFile, overwrite: true);
    }

    private static JsonObject? FindNode(JsonObject sourceMap, string nodeId)
    {
        var behaviors = sourceMap["behaviors"] as JsonArray;
        if (behaviors is null)
        {
            return null;
        }
        foreach (var behavior in behaviors.OfType<JsonObject>())
        {
            var nodes = behavior["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }
            foreach (var node in nodes.OfType<JsonObject>())
            {
                if (ReadString(node, "nodeId") == nodeId)
                {
                    return node;
                }
            }
        }
        return null;
    }

    private static string FindBehaviorSourceFile(JsonObject sourceMap, string nodeId)
    {
        var behaviors = sourceMap["behaviors"] as JsonArray;
        if (behaviors is null)
        {
            return "";
        }
        foreach (var behavior in behaviors.OfType<JsonObject>())
        {
            var nodes = behavior["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }
            if (nodes.OfType<JsonObject>().Any(node => ReadString(node, "nodeId") == nodeId))
            {
                return ReadString(behavior, "sourceFile");
            }
        }
        return "";
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

    private static bool ReadBool(JsonObject? obj, string key)
    {
        return obj is not null &&
            obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<bool>(out var result) &&
            result;
    }

    private static SourceSpan? ReadSpan(JsonObject? obj)
    {
        if (obj is null)
        {
            return null;
        }
        return new SourceSpan(
            ReadInt(obj, "startLine"),
            ReadInt(obj, "startColumn"),
            ReadInt(obj, "endLine"),
            ReadInt(obj, "endColumn"),
            ReadInt(obj, "startByte"),
            ReadInt(obj, "endByte"));
    }

    private static int ReadInt(JsonObject obj, string key)
    {
        return obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<int>(out var result)
                ? result
                : 0;
    }

    private static string SliceUtf8(string sourceText, int startByte, int endByte)
    {
        var bytes = Encoding.UTF8.GetBytes(sourceText);
        return Encoding.UTF8.GetString(bytes, startByte, endByte - startByte);
    }

    private readonly record struct SagaMetadata(string Level, string Domain, string Id);
    private readonly record struct SagaNodeMetadata(
        string Level,
        string Domain,
        string Id,
        string DisplayName,
        string Capability);
}

internal static class JsonSerializerNode
{
    public static JsonObject Summary(DiagnosticSummary summary)
    {
        return new JsonObject
        {
            ["infoCount"] = summary.InfoCount,
            ["warningCount"] = summary.WarningCount,
            ["errorCount"] = summary.ErrorCount,
            ["securityCount"] = summary.SecurityCount,
            ["hasBlockingDiagnostics"] = summary.HasBlockingDiagnostics
        };
    }

    public static JsonArray Diagnostics(IEnumerable<SagaDiagnostic> diagnostics)
    {
        var array = new JsonArray();
        foreach (var diagnostic in diagnostics)
        {
            array.Add(new JsonObject
            {
                ["severity"] = diagnostic.Severity,
                ["code"] = diagnostic.Code,
                ["message"] = diagnostic.Message,
                ["scriptId"] = diagnostic.ScriptId,
                ["sourceFile"] = diagnostic.SourceFile,
                ["bindingId"] = diagnostic.BindingId,
                ["capability"] = diagnostic.Capability
            });
        }
        return array;
    }
}
