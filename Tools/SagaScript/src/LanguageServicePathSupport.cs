// SPDX-License-Identifier: MPL-2.0

using System.Security.Cryptography;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;

namespace SagaScript;

internal static class LanguageServicePathSupport
{
    public static string Canonical(string path)
    {
        var full = Path.GetFullPath(path);
        var root = Path.GetPathRoot(full) ?? "";
        var current = root;
        foreach (var component in Path.GetRelativePath(root, full).Split(
                     Path.DirectorySeparatorChar,
                     StringSplitOptions.RemoveEmptyEntries))
        {
            var candidate = Path.Combine(current, component);
            try
            {
                FileSystemInfo info = Directory.Exists(candidate)
                    ? new DirectoryInfo(candidate) : new FileInfo(candidate);
                current = info.ResolveLinkTarget(returnFinalTarget: true)?
                    .FullName ?? candidate;
            }
            catch (IOException)
            {
                current = candidate;
            }
        }
        return Path.GetFullPath(current);
    }
}

internal static class LanguageServiceReferenceFactory
{
    public static IEnumerable<MetadataReference> Create(JsonObject payload,
        string root)
    {
        var references = (AppContext.GetData(
                "TRUSTED_PLATFORM_ASSEMBLIES") as string ?? "")
            .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries)
            .Select(path => MetadataReference.CreateFromFile(path)).ToList();
        foreach (var value in payload["metadataReferences"]?.AsArray() ?? [])
        {
            var path = LanguageService.Canonical(
                value?["path"]?.GetValue<string>() ?? "");
            var expectedHash = value?["hash"]?.GetValue<string>() ?? "";
            LanguageService.Require(LanguageService.IsContained(root, path),
                "LanguageService.PathEscape",
                "Metadata reference escapes the canonical project root.");
            LanguageService.Require(File.Exists(path),
                "LanguageService.InvalidReference",
                "Metadata reference does not exist.");
            var bytes = File.ReadAllBytes(path);
            var actualHash = "sha256:" +
                Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
            LanguageService.Require(actualHash == expectedHash,
                "LanguageService.InvalidReference",
                "Metadata reference hash does not match.");
            references.Add(MetadataReference.CreateFromImage(bytes,
                filePath: path));
        }
        return references;
    }
}
