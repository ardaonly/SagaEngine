// SPDX-License-Identifier: MPL-2.0
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace SagaScript;

internal static partial class SagaWeaverArtifacts
{
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
            node.ProjectionCompatibility == "EditableEvaluationOnly" &&
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
            "EditableByPatch" => "String literal is eligible for ReplaceStringLiteral patch evaluation/apply.",
            "Opaque" => node.OpaqueReason ?? "Construct is preserved with source links but is not safely editable.",
            "Unsupported" => "Construct is outside the current Saga-compatible authoring profile.",
            _ when node.Capability == "Deferred" => "Deferred node is disclosed as read-only and is not runtime proof.",
            _ => "Construct is projectable for inspection but not source-mutable under the current contract."
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
}
