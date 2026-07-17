// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;

namespace SagaProjectKit.Core;

public sealed class ProjectLoader
{
    private static readonly string[] Classifications =
        ["SourceTruth", "GeneratedState", "PersonalState", "SharedWorkspaceState"];

    public ProjectLoadResult Load(string input, bool checkReferences = true)
    {
        var diagnostics = new List<ProjectDiagnostic>();
        var manifestPath = ResolveManifest(input, diagnostics);
        if (manifestPath.Length == 0)
        {
            return Result("", "", null, [], diagnostics);
        }
        var root = Path.GetDirectoryName(manifestPath)!;
        ProjectManifest? manifest;
        try
        {
            manifest = JsonSerializer.Deserialize<ProjectManifest>(
                File.ReadAllBytes(manifestPath), CanonicalJson.SerializerOptions);
        }
        catch (JsonException exception)
        {
            diagnostics.Add(Error("Project.Manifest.ParseFailed", manifestPath,
                $"Project manifest JSON is invalid: {exception.Message}"));
            return Result(root, manifestPath, null, [], diagnostics);
        }
        if (manifest is null)
        {
            diagnostics.Add(Error("Project.Manifest.Invalid", manifestPath,
                "Project manifest root must be an object."));
            return Result(root, manifestPath, null, [], diagnostics);
        }

        ValidateManifest(manifest, diagnostics);
        var references = ResolveReferences(root, manifest);
        if (checkReferences)
        {
            foreach (var reference in references.Where(item => !item.Exists))
            {
                diagnostics.Add(Error("Project.Reference.Missing", reference.RelativePath,
                    $"Referenced {reference.Category} path does not exist."));
            }
        }
        return Result(root, manifestPath, manifest, references, diagnostics);
    }

    public static bool IsProjectRelative(string value) =>
        !string.IsNullOrWhiteSpace(value) && !Path.IsPathRooted(value) &&
        !(value.Length >= 2 && char.IsLetter(value[0]) && value[1] == ':') &&
        Normalize(value).Split('/', StringSplitOptions.RemoveEmptyEntries).All(segment => segment != "..");

    public static string Normalize(string value) => value.Replace('\\', '/').Trim();

    private static string ResolveManifest(string input, List<ProjectDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(input))
        {
            diagnostics.Add(Error("Project.Input.Missing", "", "A project path is required."));
            return "";
        }
        var full = Path.GetFullPath(input);
        if (File.Exists(full))
        {
            if (!string.Equals(Path.GetExtension(full), ".sagaproj", StringComparison.OrdinalIgnoreCase))
            {
                diagnostics.Add(Error("Project.Input.UnsupportedFileType", full,
                    "Explicit project input must be a .sagaproj file."));
                return "";
            }
            return full;
        }
        if (!Directory.Exists(full))
        {
            diagnostics.Add(Error("Project.Input.Missing", Normalize(input), "Project path does not exist."));
            return "";
        }
        var manifests = Directory.GetFiles(full, "*.sagaproj", SearchOption.TopDirectoryOnly)
            .OrderBy(Normalize, StringComparer.Ordinal).ToArray();
        if (manifests.Length == 1)
        {
            return manifests[0];
        }
        diagnostics.Add(Error(manifests.Length == 0 ? "Project.Manifest.Missing" : "Project.Manifest.Ambiguous",
            Normalize(full), manifests.Length == 0
                ? "Project directory does not contain a .sagaproj manifest."
                : "Project directory contains more than one .sagaproj manifest."));
        return "";
    }

    private static void ValidateManifest(ProjectManifest manifest, List<ProjectDiagnostic> diagnostics)
    {
        if (manifest.SchemaVersion != 0)
        {
            diagnostics.Add(Error("Project.Manifest.SchemaVersionUnsupported", "schemaVersion",
                "Only .sagaproj schemaVersion 0 is supported."));
        }
        Require(manifest.ProjectId, "projectId", diagnostics);
        Require(manifest.DisplayName, "displayName", diagnostics);
        Require(manifest.EngineCompatibility.MinimumVersion, "engineCompatibility.minimumVersion", diagnostics);
        Require(manifest.EngineCompatibility.TargetVersion, "engineCompatibility.targetVersion", diagnostics);
        Require(manifest.EngineCompatibility.Channel, "engineCompatibility.channel", diagnostics);
        RequirePath(manifest.Paths.Diagnostics, "paths.diagnostics", diagnostics);
        RequirePath(manifest.Paths.GeneratedReports, "paths.generatedReports", diagnostics);
        OptionalPath(manifest.Paths.SourceTruth, "paths.sourceTruth", diagnostics);
        OptionalPath(manifest.Paths.GeneratedState, "paths.generatedState", diagnostics);
        OptionalPath(manifest.Paths.PersonalState, "paths.personalState", diagnostics);
        OptionalPath(manifest.Paths.SharedWorkspaceState, "paths.sharedWorkspaceState", diagnostics);

        var all = ReferenceGroups(manifest).SelectMany(group => group.Items.Select(item => (group.Category, Item: item))).ToArray();
        foreach (var (category, item) in all)
        {
            Require(item.Id, $"{category}.id", diagnostics);
            RequirePath(item.Path, $"{category}.path", diagnostics);
        }
        foreach (var duplicate in all.Where(item => item.Item.Id.Length > 0)
                     .GroupBy(item => item.Item.Id, StringComparer.Ordinal).Where(group => group.Count() > 1))
        {
            diagnostics.Add(Error("Project.Reference.DuplicateId", duplicate.Key,
                "Project reference ids must be unique across the manifest."));
        }
        foreach (var duplicate in manifest.Scenes.Where(item => item.Id.Length > 0)
                     .GroupBy(item => item.Id, StringComparer.Ordinal).Where(group => group.Count() > 1))
            diagnostics.Add(Error("Project.Scene.DuplicateId", duplicate.Key,
                "Scene reference identities must be unique."));
        foreach (var dependency in manifest.Dependencies)
        {
            Require(dependency.PackageId, "dependencies.packageId", diagnostics);
            Require(dependency.Version, "dependencies.version", diagnostics);
            if (dependency.Modules.Count == 0)
            {
                diagnostics.Add(Error("Project.Dependency.ModulesMissing", dependency.PackageId,
                    "Dependency intent must select at least one module."));
            }
        }
        foreach (var policy in manifest.StatePolicies)
        {
            RequirePath(policy.Path, "statePolicies.path", diagnostics);
            if (!Classifications.Contains(policy.Classification, StringComparer.Ordinal))
            {
                diagnostics.Add(Error("Project.StatePolicy.InvalidClassification", policy.Path,
                    "State classification must be SourceTruth, GeneratedState, PersonalState, or SharedWorkspaceState."));
            }
            if (policy.Tracked && policy.Classification != "SourceTruth" && string.IsNullOrWhiteSpace(policy.Exception))
            {
                diagnostics.Add(Error("Project.StatePolicy.TrackedExceptionMissing", policy.Path,
                    "Tracked non-source state requires an explicit exception."));
            }
        }
    }

    private static IReadOnlyList<ResolvedProjectReference> ResolveReferences(string root, ProjectManifest manifest)
    {
        return ReferenceGroups(manifest)
            .SelectMany(group => group.Items.Select(item => new ResolvedProjectReference
            {
                Category = group.Category,
                Id = item.Id,
                Kind = item.Kind,
                RelativePath = Normalize(item.Path),
                AbsolutePath = Normalize(Path.GetFullPath(Path.Combine(root, item.Path))),
                Exists = File.Exists(Path.Combine(root, item.Path)) || Directory.Exists(Path.Combine(root, item.Path)),
            }))
            .OrderBy(item => item.Category, StringComparer.Ordinal)
            .ThenBy(item => item.Id, StringComparer.Ordinal)
            .ThenBy(item => item.RelativePath, StringComparer.Ordinal)
            .ToArray();
    }

    private static IEnumerable<(string Category, IReadOnlyList<ProjectReference> Items)> ReferenceGroups(ProjectManifest manifest)
    {
        yield return ("scene", manifest.Scenes);
        yield return ("asset", manifest.Assets);
        yield return ("scriptFolder", manifest.ScriptFolders);
        yield return ("inputMap", manifest.InputMaps);
        yield return ("launchProfile", manifest.LaunchProfiles);
        yield return ("packageProfile", manifest.PackageProfiles);
    }

    private static void Require(string value, string path, List<ProjectDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            diagnostics.Add(Error("Project.Manifest.MissingField", path, "Required field is missing or empty."));
        }
    }

    private static void RequirePath(string value, string path, List<ProjectDiagnostic> diagnostics)
    {
        Require(value, path, diagnostics);
        OptionalPath(value, path, diagnostics);
    }

    private static void OptionalPath(string value, string path, List<ProjectDiagnostic> diagnostics)
    {
        if (value.Length == 0)
        {
            return;
        }
        if (Path.IsPathRooted(value) ||
            (value.Length >= 2 && char.IsLetter(value[0]) && value[1] == ':'))
        {
            diagnostics.Add(Error("Project.Path.AbsoluteNotAllowed", path,
                "Project paths must be relative."));
        }
        if (Normalize(value).Split('/', StringSplitOptions.RemoveEmptyEntries).Any(segment => segment == ".."))
        {
            diagnostics.Add(Error("Project.Path.ParentTraversalNotAllowed", path,
                "Project paths must not contain parent traversal."));
        }
    }

    private static ProjectLoadResult Result(string root, string path, ProjectManifest? manifest,
        IReadOnlyList<ResolvedProjectReference> references, IEnumerable<ProjectDiagnostic> diagnostics) => new()
    {
        ProjectRoot = Normalize(root),
        ManifestPath = Normalize(path),
        ProjectHash = File.Exists(path) ? CanonicalJson.HashFile(path) : "",
        Manifest = manifest,
        References = references,
        Diagnostics = diagnostics.OrderByDescending(item => item.Severity == "Error")
            .ThenBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal).ToArray(),
    };

    private static ProjectDiagnostic Error(string code, string path, string message) => new()
    {
        Code = code,
        Path = Normalize(path),
        Message = message,
    };
}
