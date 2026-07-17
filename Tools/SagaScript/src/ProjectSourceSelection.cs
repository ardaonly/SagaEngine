// SPDX-FileCopyrightText: 2026 Arda Koyuncu
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

        var sources = project.Manifest.ScriptFolders
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
