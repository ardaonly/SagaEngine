// SPDX-License-Identifier: MPL-2.0

using System.Text;
using System.Text.Json;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using SagaProjectKit.Core;

namespace SagaScript;

internal sealed record ModuleUsageSource
{
    public string Path { get; init; } = "";
    public string Hash { get; init; } = "";
}

internal sealed record ModuleUsageReference
{
    public string Path { get; init; } = "";
    public string Hash { get; init; } = "";
}

internal sealed record ModuleUsageRequest
{
    public int SchemaVersion { get; init; }
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string LockHash { get; init; } = "";
    public string GraphHash { get; init; } = "";
    public string GenerationId { get; init; } = "";
    public string GenerationHash { get; init; } = "";
    public string GenerationManifestHash { get; init; } = "";
    public string ReferenceHash { get; init; } = "";
    public string OwnershipCatalogPath { get; init; } = "";
    public string OwnershipCatalogHash { get; init; } = "";
    public string PackageId { get; init; } = "";
    public string Module { get; init; } = "";
    public IReadOnlyList<ModuleUsageSource> Sources { get; init; } = [];
    public IReadOnlyList<ModuleUsageReference> References { get; init; } = [];
}

internal sealed record ModuleUsageEntry
{
    public string SourcePath { get; init; } = "";
    public int StartByte { get; init; }
    public int EndByte { get; init; }
    public int StartLine { get; init; }
    public int StartColumn { get; init; }
    public int EndLine { get; init; }
    public int EndColumn { get; init; }
    public string ContainingType { get; init; } = "";
    public string ContainingBehavior { get; init; } = "";
    public string DocumentationId { get; init; } = "";
    public string ApiId { get; init; } = "";
    public string Surface { get; init; } = "";
    public string Module { get; init; } = "";
    public string PackageId { get; init; } = "";
}

internal sealed record ModuleUsageReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "module-usage";
    public string Status { get; init; } = "Failed";
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string LockHash { get; init; } = "";
    public string GraphHash { get; init; } = "";
    public string GenerationId { get; init; } = "";
    public string GenerationHash { get; init; } = "";
    public string GenerationManifestHash { get; init; } = "";
    public string ReferenceHash { get; init; } = "";
    public string OwnershipCatalogHash { get; init; } = "";
    public string PackageId { get; init; } = "";
    public string Module { get; init; } = "";
    public int UsageCount => Usages.Count;
    public IReadOnlyList<ModuleUsageEntry> Usages { get; init; } = [];
    public IReadOnlyList<string> Diagnostics { get; init; } = [];
}

internal static class ModuleUsageAnalysis
{
    private sealed record Ownership(string DocumentationId, string ApiId,
        string Surface, string Module, string PackageId);

    public static ModuleUsageReport Analyze(string requestPath)
    {
        ModuleUsageRequest request;
        try
        {
            request = JsonSerializer.Deserialize<ModuleUsageRequest>(
                File.ReadAllBytes(requestPath),
                CanonicalJson.SerializerOptions)
                ?? throw new InvalidOperationException("Request is empty.");
        }
        catch (Exception exception) when (exception is IOException or
                                           JsonException or
                                           InvalidOperationException)
        {
            return new ModuleUsageReport
            {
                Diagnostics = [$"Script.ModuleUsage.InvalidRequest: {exception.Message}"],
            };
        }
        var diagnostics = new List<string>();
        if (request.SchemaVersion != 1 || request.ProjectId.Length == 0 ||
            request.PackageId.Length == 0 || request.Module.Length == 0)
            diagnostics.Add("Script.ModuleUsage.InvalidRequest: schema and identities are required.");
        VerifyFile(request.OwnershipCatalogPath,
            request.OwnershipCatalogHash, "ownership catalog", diagnostics);
        foreach (var source in request.Sources)
            VerifyFile(source.Path, source.Hash, "saved source", diagnostics);
        foreach (var reference in request.References)
            VerifyFile(reference.Path, reference.Hash, "reference", diagnostics);
        var computedReferenceHash = CanonicalJson.Hash(CanonicalJson.Serialize(
            request.References.OrderBy(item => item.Path, StringComparer.Ordinal)
                .Select(item => new { path = Normalize(item.Path), item.Hash })
                .ToArray()));
        if (computedReferenceHash != request.ReferenceHash)
            diagnostics.Add("Script.ModuleUsage.StaleReferences: reference inventory hash changed.");

        var ownership = new Dictionary<string, Ownership>(StringComparer.Ordinal);
        if (diagnostics.Count == 0)
        {
            try
            {
                using var catalog = JsonDocument.Parse(
                    File.ReadAllBytes(request.OwnershipCatalogPath));
                var root = catalog.RootElement;
                if (root.GetProperty("schemaVersion").GetInt32() != 1 ||
                    root.GetProperty("projectId").GetString() !=
                        request.ProjectId ||
                    root.GetProperty("projectHash").GetString() !=
                        request.ProjectHash ||
                    root.GetProperty("graphHash").GetString() !=
                        request.GraphHash)
                    throw new InvalidOperationException(
                        "Ownership catalog identities are stale.");
                foreach (var symbol in root.GetProperty("symbols").EnumerateArray())
                {
                    if (symbol.GetProperty("packageId").GetString() !=
                            request.PackageId ||
                        symbol.GetProperty("module").GetString() != request.Module)
                        continue;
                    var item = new Ownership(
                        symbol.GetProperty("documentationId").GetString() ?? "",
                        symbol.GetProperty("apiId").GetString() ?? "",
                        symbol.GetProperty("surface").GetString() ?? "",
                        request.Module, request.PackageId);
                    ownership[item.DocumentationId] = item;
                }
            }
            catch (Exception exception) when (exception is JsonException or
                                               IOException or
                                               InvalidOperationException or
                                               KeyNotFoundException)
            {
                diagnostics.Add(
                    $"Script.ModuleUsage.StaleOwnership: {exception.Message}");
            }
        }

        var usages = new List<ModuleUsageEntry>();
        if (diagnostics.Count == 0)
        {
            var sourceData = request.Sources.OrderBy(item => item.Path,
                    StringComparer.Ordinal)
                .Select(item =>
                {
                    var text = File.ReadAllText(item.Path, Encoding.UTF8);
                    return (item, text, tree: CSharpSyntaxTree.ParseText(text,
                        new CSharpParseOptions(LanguageVersion.Latest),
                        Normalize(item.Path)));
                }).ToArray();
            var references = TrustedPlatformReferences();
            references.AddRange(request.References.Select(item =>
                MetadataReference.CreateFromFile(Path.GetFullPath(item.Path))));
            var compilation = CSharpCompilation.Create(
                "SagaScriptModuleUsage", sourceData.Select(item => item.tree),
                references, new CSharpCompilationOptions(
                    OutputKind.DynamicallyLinkedLibrary));
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var (source, text, tree) in sourceData)
            {
                var model = compilation.GetSemanticModel(tree,
                    ignoreAccessibility: true);
                foreach (var node in tree.GetRoot().DescendantNodesAndSelf())
                {
                    if (node.AncestorsAndSelf().OfType<UsingDirectiveSyntax>()
                        .Any()) continue;
                    if (node is InvocationExpressionSyntax)
                        AddSymbol(model.GetSymbolInfo(node).Symbol, node,
                            source.Path, text, tree, model, ownership, seen,
                            usages);
                    else if (node is ObjectCreationExpressionSyntax)
                        AddSymbol(model.GetTypeInfo(node).Type, node,
                            source.Path, text, tree, model, ownership, seen,
                            usages);
                    else if (node is MemberAccessExpressionSyntax member &&
                             member.Parent is not InvocationExpressionSyntax)
                        AddSymbol(model.GetSymbolInfo(node).Symbol, node,
                            source.Path, text, tree, model, ownership, seen,
                            usages);
                    else if (node is TypeSyntax type &&
                             IsExplicitTypeSyntax(type))
                        AddSymbol(model.GetTypeInfo(type).Type, node,
                            source.Path, text, tree, model, ownership, seen,
                            usages);
                }
            }
        }
        usages = usages.OrderBy(item => item.SourcePath, StringComparer.Ordinal)
            .ThenBy(item => item.StartByte)
            .ThenBy(item => item.DocumentationId, StringComparer.Ordinal)
            .ToList();
        return new ModuleUsageReport
        {
            Status = diagnostics.Count == 0 ? "Passed" : "Failed",
            ProjectId = request.ProjectId,
            ProjectHash = request.ProjectHash,
            LockHash = request.LockHash,
            GraphHash = request.GraphHash,
            GenerationId = request.GenerationId,
            GenerationHash = request.GenerationHash,
            GenerationManifestHash = request.GenerationManifestHash,
            ReferenceHash = request.ReferenceHash,
            OwnershipCatalogHash = request.OwnershipCatalogHash,
            PackageId = request.PackageId,
            Module = request.Module,
            Usages = usages,
            Diagnostics = diagnostics.Order(StringComparer.Ordinal).ToArray(),
        };
    }

    private static bool IsExplicitTypeSyntax(TypeSyntax type)
    {
        if (type.IsVar) return false;
        return type.Parent switch
        {
            VariableDeclarationSyntax declaration =>
                ReferenceEquals(declaration.Type, type),
            ParameterSyntax parameter =>
                ReferenceEquals(parameter.Type, type),
            CastExpressionSyntax cast =>
                ReferenceEquals(cast.Type, type),
            BaseTypeSyntax baseType =>
                ReferenceEquals(baseType.Type, type),
            DeclarationExpressionSyntax declaration =>
                ReferenceEquals(declaration.Type, type),
            TypeArgumentListSyntax => true,
            _ => false,
        };
    }

    private static void AddSymbol(ISymbol? symbol, SyntaxNode node,
        string sourcePath, string text, SyntaxTree tree, SemanticModel model,
        IReadOnlyDictionary<string, Ownership> ownership,
        HashSet<string> seen, List<ModuleUsageEntry> usages)
    {
        if (symbol is null) return;
        var original = symbol.OriginalDefinition;
        var documentationId = original.GetDocumentationCommentId() ??
            symbol.GetDocumentationCommentId() ?? "";
        if (!ownership.TryGetValue(documentationId, out var owner)) return;
        var span = node.Span;
        var key = Normalize(sourcePath) + ":" + span.Start + ":" +
            span.End + ":" + documentationId;
        if (!seen.Add(key)) return;
        var lines = tree.GetLineSpan(span);
        var containing = node.AncestorsAndSelf().OfType<TypeDeclarationSyntax>()
            .FirstOrDefault();
        var containingSymbol = containing is null ? null :
            model.GetDeclaredSymbol(containing);
        usages.Add(new ModuleUsageEntry
        {
            SourcePath = Normalize(sourcePath),
            StartByte = Encoding.UTF8.GetByteCount(text.AsSpan(0, span.Start)),
            EndByte = Encoding.UTF8.GetByteCount(text.AsSpan(0, span.End)),
            StartLine = lines.StartLinePosition.Line + 1,
            StartColumn = lines.StartLinePosition.Character + 1,
            EndLine = lines.EndLinePosition.Line + 1,
            EndColumn = lines.EndLinePosition.Character + 1,
            ContainingType = containingSymbol?.ToDisplayString() ?? "",
            ContainingBehavior =
                containingSymbol?.BaseType?.ToDisplayString() ?? "",
            DocumentationId = documentationId,
            ApiId = owner.ApiId,
            Surface = owner.Surface,
            Module = owner.Module,
            PackageId = owner.PackageId,
        });
    }

    private static List<MetadataReference> TrustedPlatformReferences()
    {
        var paths = (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")
                as string ?? "")
            .Split(Path.PathSeparator,
                StringSplitOptions.RemoveEmptyEntries);
        return paths.Distinct(StringComparer.Ordinal)
            .Select(path => MetadataReference.CreateFromFile(path))
            .Cast<MetadataReference>().ToList();
    }

    private static void VerifyFile(string path, string expectedHash,
        string label, List<string> diagnostics)
    {
        try
        {
            if (CanonicalJson.HashFile(path) != expectedHash)
                diagnostics.Add(
                    $"Script.ModuleUsage.StaleInput: {label} hash changed: {Normalize(path)}");
        }
        catch (Exception exception) when (exception is IOException or
                                           UnauthorizedAccessException)
        {
            diagnostics.Add(
                $"Script.ModuleUsage.MissingInput: {label}: {exception.Message}");
        }
    }

    private static string Normalize(string value) =>
        value.Replace('\\', '/');
}
