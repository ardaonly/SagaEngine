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
                ProjectionCompatibility = compatibility == "Supported" ? "EditableEvaluationOnly" : "Opaque",
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

    private static bool DerivesFromHighLevelSagaBehavior(TypeDeclarationSyntax typeDeclaration) =>
        typeDeclaration.BaseList?.Types.Any(item =>
        {
            var name = item.Type.ToString();
            return name is "SagaBehavior" or "SagaEngine.Scripting.SagaBehavior" ||
                name.EndsWith(".SagaBehavior", StringComparison.Ordinal);
        }) == true;

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

    private static SagaDiagnostic BlockOperationError(string code, string message, string? sourceFile = null)
    {
        return new SagaDiagnostic
        {
            Severity = SagaDiagnosticSeverity.Error.ToString(),
            Code = code,
            Message = message,
            SourceFile = string.IsNullOrWhiteSpace(sourceFile) ? null : sourceFile
        };
    }
}
