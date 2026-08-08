// SPDX-License-Identifier: MPL-2.0

using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis.CSharp;

namespace SagaScript;

internal static class LanguageServiceProjectSnapshotValidator
{
    public static void Validate(JsonObject payload, string root)
    {
        ValidateCompilerIdentity(payload);
        var generationId = payload["packageGenerationId"]?.GetValue<string>() ?? "";
        var generationHash = payload["packageGeneration"]?.GetValue<string>() ?? "";
        var graphHash = payload["packageGraphHash"]?.GetValue<string>() ?? "";
        var referenceHash = payload["referenceManifestHash"]?.GetValue<string>() ?? "";
        var ownershipHash = payload["ownershipManifestHash"]?.GetValue<string>() ?? "";
        var hasPackageIdentity = new[] { generationId, generationHash,
            graphHash, referenceHash, ownershipHash }.Any(value =>
                !string.IsNullOrEmpty(value));
        if (!hasPackageIdentity)
        {
            LanguageService.Require(payload["generationManifest"] is null &&
                payload["referenceManifest"] is null &&
                payload["ownershipManifest"] is null,
                "LanguageService.InvalidPackageSnapshot",
                "Verified package inputs require package identities.");
            return;
        }
        LanguageService.Require(generationId.Length == 64 &&
            generationId.All(character => character is >= '0' and <= '9' or
                >= 'a' and <= 'f') &&
            generationHash == "sha256:" + generationId &&
            graphHash.StartsWith("sha256:", StringComparison.Ordinal) &&
            referenceHash.StartsWith("sha256:", StringComparison.Ordinal) &&
            ownershipHash.StartsWith("sha256:", StringComparison.Ordinal),
            "LanguageService.InvalidPackageSnapshot",
            "Package generation or graph identity is invalid.");
        var generationBytes = VerifiedInput(payload, "generationManifest",
            expectedHash: null, root);
        var referenceBytes = VerifiedInput(payload, "referenceManifest",
            referenceHash, root);
        var ownershipInput = payload["ownershipManifest"] as JsonObject;
        _ = VerifiedFile(ownershipInput, root,
            "LanguageService.InvalidOwnership");
        LanguageService.Require(
            ownershipInput!["hash"]?.GetValue<string>() == ownershipHash,
            "LanguageService.InvalidOwnership",
            "ownershipManifest hash does not match its package identity.");
        using var generation = JsonDocument.Parse(generationBytes);
        var generationRoot = generation.RootElement;
        var projectIdentity = payload["projectId"]?.GetValue<string>() ?? "";
        LanguageService.Require(
            generationRoot.GetProperty("generationId").GetString() == generationId &&
            generationRoot.GetProperty("generationHash").GetString() == generationHash &&
            generationRoot.GetProperty("graphHash").GetString() == graphHash &&
            generationRoot.GetProperty("projectId").GetString() == projectIdentity,
            "LanguageService.InvalidPackageSnapshot",
            "Generation manifest does not match the candidate package snapshot.");
        var files = generationRoot.GetProperty("files").EnumerateArray()
            .ToDictionary(value => value.GetProperty("path").GetString() ?? "",
                value => value.GetProperty("hash").GetString() ?? "",
                StringComparer.Ordinal);
        LanguageService.Require(
            files.GetValueOrDefault("csharp_references_v1.json") == referenceHash &&
            files.GetValueOrDefault("api_ownership_v1.json") == ownershipHash,
            "LanguageService.InvalidPackageSnapshot",
            "Generation manifest input hashes are incoherent.");
        using var references = JsonDocument.Parse(referenceBytes);
        LanguageService.Require(
            references.RootElement.GetProperty("projectId").GetString() ==
                projectIdentity &&
            references.RootElement.GetProperty("graphHash").GetString() == graphHash,
            "LanguageService.InvalidPackageSnapshot",
            "Reference manifest does not match the candidate package snapshot.");
        ValidateReferences(payload, references.RootElement, root);
    }

    private static void ValidateCompilerIdentity(JsonObject payload)
    {
        var compiler = payload["compilerVersion"]?.GetValue<string>() ?? "4.11.0";
        var language = payload["languageVersion"]?.GetValue<string>() ??
            "latest-major";
        var actualCompiler = typeof(CSharpCompilation).Assembly.GetName()
            .Version?.ToString() ?? "";
        LanguageService.Require(
            actualCompiler.StartsWith(compiler, StringComparison.Ordinal),
            "LanguageService.CompilerIdentityMismatch",
            "Candidate compiler identity does not match the language service.");
        LanguageService.Require(language == "latest-major",
            "LanguageService.LanguageIdentityMismatch",
            "Candidate language identity is unsupported.");
    }

    private static void ValidateReferences(JsonObject payload,
        JsonElement references, string root)
    {
        var referenceRoot = Path.GetDirectoryName(LanguageService.Canonical(
            (payload["referenceManifest"] as JsonObject)?["path"]?
                .GetValue<string>() ?? "")) ?? "";
        var expected = references.GetProperty("references").EnumerateArray()
            .Where(value => value.GetProperty("trust").GetString() ==
                "TrustedEngineDistribution" &&
                value.GetProperty("kind").GetString() != "analyzer")
            .ToDictionary(value => LanguageService.Canonical(Path.Combine(
                    referenceRoot, value.GetProperty(
                        "resolvedAssemblyLogicalPath").GetString() ?? "")),
                value => value.GetProperty("resolvedAssemblyHash").GetString() ?? "",
                StringComparer.Ordinal);
        var supplied = (payload["metadataReferences"]?.AsArray() ?? [])
            .Select(value => value as JsonObject).ToArray();
        LanguageService.Require(supplied.Length == expected.Count,
            "LanguageService.InvalidReference",
            "Metadata reference inventory is incomplete or ambiguous.");
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var reference in supplied)
        {
            var path = LanguageService.Canonical(
                reference?["path"]?.GetValue<string>() ?? "");
            var hash = reference?["hash"]?.GetValue<string>() ?? "";
            LanguageService.Require(seen.Add(path) &&
                expected.GetValueOrDefault(path) == hash,
                "LanguageService.InvalidReference",
                "Metadata reference is absent from the verified reference manifest.");
        }
        var runtimeBridge = payload["runtimeBridgeIdentity"]?
            .GetValue<string>() ?? "";
        if (!string.IsNullOrEmpty(runtimeBridge))
            LanguageService.Require(expected.Values.Contains(runtimeBridge,
                    StringComparer.Ordinal),
                "LanguageService.InvalidReference",
                "Runtime bridge identity is absent from the reference inventory.");
        foreach (var analyzer in payload["analyzers"]?.AsArray() ?? [])
            _ = VerifiedFile(analyzer as JsonObject, root,
                "LanguageService.InvalidAnalyzer");
    }

    private static byte[] VerifiedInput(JsonObject payload, string name,
        string? expectedHash, string root)
    {
        var input = payload[name] as JsonObject;
        LanguageService.Require(input is not null,
            "LanguageService.InvalidPackageSnapshot",
            $"A verified {name} input is required.");
        var bytes = VerifiedFile(input, root,
            "LanguageService.InvalidPackageSnapshot");
        if (expectedHash is not null)
            LanguageService.Require(input!["hash"]?.GetValue<string>() ==
                expectedHash, "LanguageService.InvalidPackageSnapshot",
                $"{name} hash does not match its package identity.");
        return bytes;
    }

    private static byte[] VerifiedFile(JsonObject? input, string root,
        string code)
    {
        LanguageService.Require(input is not null, code,
            "Verified input is missing.");
        var path = LanguageService.Canonical(
            input!["path"]?.GetValue<string>() ?? "");
        var expectedHash = input["hash"]?.GetValue<string>() ?? "";
        LanguageService.Require(LanguageService.IsContained(root, path),
            "LanguageService.PathEscape",
            "Verified input escapes the canonical project root.");
        LanguageService.Require(File.Exists(path), code,
            "Verified input does not exist.");
        var bytes = File.ReadAllBytes(path);
        var actualHash = "sha256:" +
            Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
        LanguageService.Require(actualHash == expectedHash, code,
            "Verified input hash does not match.");
        return bytes;
    }
}
