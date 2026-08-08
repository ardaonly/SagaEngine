// SPDX-License-Identifier: MPL-2.0

using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace SagaScript;

internal sealed class PackageVisibilityCatalog
{
    internal sealed record Owner(bool Enabled, string PackageId,
        string Module, string ApiId);

    internal sealed record DisabledUsage(Location Location, Owner Owner);

    private readonly IReadOnlyDictionary<string, Owner> owners;

    private PackageVisibilityCatalog(IReadOnlyDictionary<string, Owner> owners)
    {
        this.owners = owners;
    }

    public static PackageVisibilityCatalog Empty { get; } =
        new(new Dictionary<string, Owner>(StringComparer.Ordinal));

    public static PackageVisibilityCatalog LoadWorker(WorkerVerifiedInput input)
    {
        return Parse(File.ReadAllBytes(input.Path), expectedProjectId: null,
            expectedGraphHash: null);
    }

    public static PackageVisibilityCatalog LoadLanguageService(
        JsonObject payload, string canonicalRoot)
    {
        var manifestHash = payload["ownershipManifestHash"]?.GetValue<string>() ?? "";
        if (string.IsNullOrEmpty(manifestHash))
        {
            LanguageService.Require(payload["ownershipManifest"] is null,
                "LanguageService.InvalidOwnership",
                "Ownership manifest input requires an ownership manifest hash.");
            return Empty;
        }

        var input = payload["ownershipManifest"] as JsonObject;
        LanguageService.Require(input is not null,
            "LanguageService.InvalidOwnership",
            "A verified ownership manifest is required for the package snapshot.");
        var path = LanguageService.Canonical(
            input!["path"]?.GetValue<string>() ?? "");
        var inputHash = input["hash"]?.GetValue<string>() ?? "";
        LanguageService.Require(LanguageService.IsContained(canonicalRoot, path),
            "LanguageService.PathEscape",
            "Ownership manifest escapes the canonical project root.");
        LanguageService.Require(File.Exists(path),
            "LanguageService.InvalidOwnership",
            "Ownership manifest does not exist.");
        var bytes = File.ReadAllBytes(path);
        var actualHash = "sha256:" +
            Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
        LanguageService.Require(inputHash == manifestHash && actualHash == inputHash,
            "LanguageService.InvalidOwnership",
            "Ownership manifest hash does not match.");
        try
        {
            return Parse(bytes, payload["projectId"]?.GetValue<string>() ?? "",
                payload["packageGraphHash"]?.GetValue<string>() ?? "");
        }
        catch (Exception exception) when (exception is JsonException or
                                           KeyNotFoundException or
                                           InvalidDataException or
                                           ArgumentException or
                                           InvalidOperationException)
        {
            throw new LanguageService.ProtocolException(
                "LanguageService.InvalidOwnership", exception.Message);
        }
    }

    public bool IsDisabled(ISymbol? symbol) =>
        TryOwner(symbol, out var owner) && !owner.Enabled;

    public bool TryDisabledOwner(ISymbol? symbol, out Owner owner)
    {
        if (TryOwner(symbol, out owner) && !owner.Enabled) return true;
        owner = new Owner(true, "", "", "");
        return false;
    }

    public async Task<IReadOnlyList<DisabledUsage>> FindDisabledUsagesAsync(
        Document document, CancellationToken cancellation)
    {
        var root = await document.GetSyntaxRootAsync(cancellation);
        var model = await document.GetSemanticModelAsync(cancellation);
        if (root is null || model is null) return [];
        return FindDisabledUsages(root, model, cancellation);
    }

    public IReadOnlyList<DisabledUsage> FindDisabledUsages(SyntaxNode root,
        SemanticModel model, CancellationToken cancellation = default)
    {
        var result = new List<DisabledUsage>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var node in root.DescendantNodesAndSelf())
        {
            cancellation.ThrowIfCancellationRequested();
            if (node.AncestorsAndSelf().OfType<UsingDirectiveSyntax>().Any())
                continue;
            var symbol = SymbolFor(model, node, cancellation);
            if (!TryDisabledOwner(symbol, out var owner)) continue;
            var identity = node.Span.Start + ":" + DocumentationId(symbol);
            if (seen.Add(identity))
                result.Add(new DisabledUsage(node.GetLocation(), owner));
        }
        return result;
    }

    private bool TryOwner(ISymbol? symbol, out Owner owner)
    {
        if (symbol is IMethodSymbol { ReducedFrom: not null } method &&
            TryExactOwner(method.ReducedFrom, out owner))
            return true;
        if (TryExactOwner(symbol, out owner)) return true;
        owner = new Owner(true, "", "", "");
        return false;
    }

    private bool TryExactOwner(ISymbol? symbol, out Owner owner)
    {
        if (symbol is not null)
        {
            var id = DocumentationId(symbol);
            if (!string.IsNullOrEmpty(id) && owners.TryGetValue(id, out owner!))
                return true;
            id = symbol.OriginalDefinition.GetDocumentationCommentId() ?? "";
            if (!string.IsNullOrEmpty(id) && owners.TryGetValue(id, out owner!))
                return true;
        }
        owner = new Owner(true, "", "", "");
        return false;
    }

    private static string DocumentationId(ISymbol? symbol) =>
        symbol?.OriginalDefinition.GetDocumentationCommentId() ??
        symbol?.GetDocumentationCommentId() ?? "";

    private static ISymbol? SymbolFor(SemanticModel model, SyntaxNode node,
        CancellationToken cancellation) => node switch
    {
        InvocationExpressionSyntax => model.GetSymbolInfo(node, cancellation).Symbol,
        ObjectCreationExpressionSyntax => model.GetTypeInfo(node, cancellation).Type,
        MemberAccessExpressionSyntax member
            when member.Parent is not InvocationExpressionSyntax =>
            model.GetSymbolInfo(node, cancellation).Symbol,
        TypeSyntax type when ExplicitType(type) =>
            model.GetTypeInfo(node, cancellation).Type,
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

    private static PackageVisibilityCatalog Parse(byte[] bytes,
        string? expectedProjectId, string? expectedGraphHash)
    {
        using var document = JsonDocument.Parse(bytes);
        var root = document.RootElement;
        if (root.GetProperty("schemaVersion").GetInt32() != 1)
            throw new InvalidDataException("Unsupported ownership manifest schema.");
        var projectId = root.GetProperty("projectId").GetString() ?? "";
        if (!string.IsNullOrEmpty(expectedProjectId) && projectId != expectedProjectId)
            throw new InvalidDataException("Ownership manifest project identity does not match.");
        var generation = root.GetProperty("generationIdentity").GetString() ?? "";
        var graphHash = root.GetProperty("graphHash").GetString() ?? "";
        if (string.IsNullOrEmpty(generation) || generation != graphHash)
            throw new InvalidDataException("Ownership generation identity is invalid.");
        if (!string.IsNullOrEmpty(expectedGraphHash) &&
            graphHash != expectedGraphHash)
            throw new InvalidDataException(
                "Ownership graph identity does not match.");
        var result = new Dictionary<string, Owner>(StringComparer.Ordinal);
        foreach (var symbol in root.GetProperty("symbols").EnumerateArray())
        {
            var id = symbol.GetProperty("documentationId").GetString() ?? "";
            if (string.IsNullOrWhiteSpace(id) || !result.TryAdd(id, new Owner(
                    symbol.GetProperty("enabled").GetBoolean(),
                    symbol.GetProperty("packageId").GetString() ?? "",
                    symbol.GetProperty("module").GetString() ?? "",
                    symbol.GetProperty("apiId").GetString() ?? "")))
                throw new InvalidDataException("Ownership manifest contains an invalid or duplicate symbol identity.");
        }
        return new PackageVisibilityCatalog(result);
    }
}
