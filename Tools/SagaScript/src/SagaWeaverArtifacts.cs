// SPDX-License-Identifier: MPL-2.0
// SagaWeaverArtifacts.cs
// Source-preserving SagaBehavior analysis, projection, and patch-evaluation artifacts.

using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
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

internal static partial class SagaWeaverArtifacts
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

    private static readonly HashSet<string> BlockOperationKinds = new(StringComparer.Ordinal)
    {
        "LiteralValueEdit",
        "BooleanLiteralEdit",
        "NumericLiteralEdit",
        "StringLiteralEdit",
        "MethodArgumentLiteralEdit",
        "SupportedConditionLiteralEdit"
    };

    private static readonly HashSet<string> ForbiddenBlockOperationKinds = new(StringComparer.Ordinal)
    {
        "ArbitrarySourceRewrite",
        "MethodBodyRegeneration",
        "ClassRestructure",
        "UsingRewrite",
        "FormattingRewrite",
        "UnsupportedRegionEdit",
        "OpaqueRegionEdit",
        "AdvancedCSharpLowering",
        "AsyncRewrite",
        "ReflectionRewrite",
        "UnsafeRewrite",
        "ControlFlowRestructure"
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
                if (typeMetadata is null && DerivesFromHighLevelSagaBehavior(typeDeclaration))
                {
                    typeMetadata = new SagaMetadata("High", "Gameplay", GetDeclaringType(typeDeclaration));
                }
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
                Message = "No [SagaBehavior] metadata or SagaBehavior-derived class was found in the supplied C# sources."
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
