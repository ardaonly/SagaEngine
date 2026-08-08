// SPDX-License-Identifier: MPL-2.0

using System.Text;
using System.Text.Json;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace SagaScript;

internal static class PackageVisibilityValidator
{
    public static void Validate(ProjectSourceSelection selection,
        string runtimeBridgeAssembly, WorkerVerifiedInput ownershipInput,
        List<WorkerDiagnostic> diagnostics)
    {
        PackageVisibilityCatalog catalog;
        try
        {
            catalog = PackageVisibilityCatalog.LoadWorker(ownershipInput);
        }
        catch (Exception exception) when (exception is IOException or
                                           JsonException or
                                           KeyNotFoundException or
                                           ArgumentException or
                                           InvalidDataException or
                                           InvalidOperationException)
        {
            diagnostics.Add(Error("Script.Worker.OwnershipInvalid",
                ownershipInput.Path, exception.Message));
            return;
        }
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
        foreach (var (relative, tree) in sourceData)
        {
            var model = compilation.GetSemanticModel(tree,
                ignoreAccessibility: true);
            foreach (var usage in catalog.FindDisabledUsages(tree.GetRoot(), model))
            {
                var line = usage.Location.GetLineSpan().StartLinePosition;
                diagnostics.Add(Error(
                    "Script.PackageVisibility.DisabledModule", relative,
                    $"Package-owned API '{usage.Owner.ApiId}' at {line.Line + 1}:{line.Character + 1} requires disabled module '{usage.Owner.PackageId}/{usage.Owner.Module}'."));
            }
        }
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
