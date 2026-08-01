// SPDX-License-Identifier: MPL-2.0

using System.Text;
using System.Text.Json;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace SagaScript;

internal static class PackageVisibilityValidator
{
    private sealed record Owner(bool Enabled, string PackageId,
        string Module, string ApiId);

    public static void Validate(ProjectSourceSelection selection,
        string runtimeBridgeAssembly, WorkerVerifiedInput ownershipInput,
        List<WorkerDiagnostic> diagnostics)
    {
        var owners = LoadOwners(ownershipInput, diagnostics);
        if (owners is null) return;
        var sourceData = selection.SourceFiles.Order(StringComparer.Ordinal)
            .Select(sourceFile =>
            {
                var relative = RelativeSource(selection.ProjectRoot, sourceFile);
                var text = File.ReadAllText(sourceFile, Encoding.UTF8);
                var tree = CSharpSyntaxTree.ParseText(text,
                    new CSharpParseOptions(LanguageVersion.Latest), relative);
                return (relative, tree);
            }).ToArray();
        var references = (AppContext.GetData(
                "TRUSTED_PLATFORM_ASSEMBLIES") as string ?? "")
            .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries)
            .Select(path => MetadataReference.CreateFromFile(path))
            .Cast<MetadataReference>().ToList();
        references.Add(MetadataReference.CreateFromFile(runtimeBridgeAssembly));
        var compilation = CSharpCompilation.Create(
            "SagaScriptPackageVisibility", sourceData.Select(item => item.tree),
            references, new CSharpCompilationOptions(
                OutputKind.DynamicallyLinkedLibrary));
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var (relative, tree) in sourceData)
        {
            var model = compilation.GetSemanticModel(tree,
                ignoreAccessibility: true);
            foreach (var node in tree.GetRoot().DescendantNodesAndSelf())
            {
                if (node.AncestorsAndSelf().OfType<UsingDirectiveSyntax>().Any())
                    continue;
                var symbol = SymbolFor(model, node);
                var documentationId =
                    symbol?.OriginalDefinition.GetDocumentationCommentId() ??
                    symbol?.GetDocumentationCommentId() ?? "";
                if (!owners.TryGetValue(documentationId, out var owner) ||
                    owner.Enabled) continue;
                var identity = relative + ":" + node.Span.Start + ":" +
                    documentationId;
                if (!seen.Add(identity)) continue;
                var line = tree.GetLineSpan(node.Span).StartLinePosition;
                diagnostics.Add(Error(
                    "Script.PackageVisibility.DisabledModule", relative,
                    $"Package-owned API '{owner.ApiId}' at {line.Line + 1}:{line.Character + 1} requires disabled module '{owner.PackageId}/{owner.Module}'."));
            }
        }
    }

    private static Dictionary<string, Owner>? LoadOwners(
        WorkerVerifiedInput input, List<WorkerDiagnostic> diagnostics)
    {
        try
        {
            using var catalog = JsonDocument.Parse(File.ReadAllBytes(input.Path));
            return catalog.RootElement.GetProperty("symbols").EnumerateArray()
                .ToDictionary(
                    symbol => symbol.GetProperty("documentationId").GetString() ?? "",
                    symbol => new Owner(
                        symbol.GetProperty("enabled").GetBoolean(),
                        symbol.GetProperty("packageId").GetString() ?? "",
                        symbol.GetProperty("module").GetString() ?? "",
                        symbol.GetProperty("apiId").GetString() ?? ""),
                    StringComparer.Ordinal);
        }
        catch (Exception exception) when (exception is IOException or
                                           JsonException or
                                           KeyNotFoundException or
                                           ArgumentException)
        {
            diagnostics.Add(Error("Script.Worker.OwnershipInvalid",
                input.Path, exception.Message));
            return null;
        }
    }

    private static ISymbol? SymbolFor(SemanticModel model, SyntaxNode node) =>
        node switch
        {
            InvocationExpressionSyntax => model.GetSymbolInfo(node).Symbol,
            ObjectCreationExpressionSyntax => model.GetTypeInfo(node).Type,
            MemberAccessExpressionSyntax member
                when member.Parent is not InvocationExpressionSyntax =>
                model.GetSymbolInfo(node).Symbol,
            TypeSyntax type when ExplicitType(type) => model.GetTypeInfo(node).Type,
            _ => null,
        };

    private static bool ExplicitType(TypeSyntax type)
    {
        if (type.IsVar) return false;
        return type.Parent switch
        {
            VariableDeclarationSyntax declaration =>
                ReferenceEquals(declaration.Type, type),
            ParameterSyntax parameter => ReferenceEquals(parameter.Type, type),
            CastExpressionSyntax cast => ReferenceEquals(cast.Type, type),
            BaseTypeSyntax baseType => ReferenceEquals(baseType.Type, type),
            DeclarationExpressionSyntax declaration =>
                ReferenceEquals(declaration.Type, type),
            TypeArgumentListSyntax => true,
            _ => false,
        };
    }

    private static string RelativeSource(string root, string path)
    {
        var relative = Path.GetRelativePath(Path.GetFullPath(root),
            Path.GetFullPath(path)).Replace(Path.DirectorySeparatorChar, '/');
        return relative.StartsWith("../", StringComparison.Ordinal)
            ? Path.GetFileName(path) : relative;
    }

    private static WorkerDiagnostic Error(string code, string path,
        string message) => new()
    {
        Code = code,
        Path = path.Replace('\\', '/'),
        Message = message,
    };
}
