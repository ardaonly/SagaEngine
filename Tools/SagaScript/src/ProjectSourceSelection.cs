// SPDX-License-Identifier: MPL-2.0

using SagaProjectKit.Core;

namespace SagaScript;

internal sealed record ProjectSourceSelection(string ProjectRoot, ProjectLoadResult Project,
    IReadOnlyList<string> SourceFiles)
{
    public static ProjectSourceSelection Load(string projectPath)
    {
        var project = new ProjectLoader().Load(projectPath, checkReferences: true);
        if (!project.Passed || project.Manifest is null)
        {
            var message = string.Join("; ", project.Diagnostics
                .OrderBy(item => item.Code, StringComparer.Ordinal)
                .Select(item => $"{item.Code}: {item.Message}"));
            throw new InvalidOperationException("Project validation failed: " + message);
        }

        var scriptFolders = project.Manifest.ScriptFolders;
        var missingKinds = scriptFolders.Count(item => string.IsNullOrWhiteSpace(item.Kind));
        if (scriptFolders.Any(item => item.Kind.Length > 0 && item.Kind != "csharp"))
            throw new InvalidOperationException(
                "Project contains an unknown or unavailable script-folder language; no C# fallback is allowed.");
        if (missingKinds > 0 && (scriptFolders.Count != 1 || missingKinds != 1))
            throw new InvalidOperationException(
                "Schema-0 script folders without kind are ambiguous; declare kind 'csharp' explicitly.");
        var sources = scriptFolders
            .OrderBy(item => item.Id, StringComparer.Ordinal)
            .Select(item => Path.Combine(project.ProjectRoot, item.Path))
            .SelectMany(path => SourceDiscovery.Discover([path]))
            .Distinct(StringComparer.Ordinal)
            .Order(StringComparer.Ordinal)
            .ToArray();
        if (sources.Length == 0)
        {
            throw new InvalidOperationException("Project contains no selected C# source files.");
        }
        return new ProjectSourceSelection(project.ProjectRoot, project, sources);
    }
}
